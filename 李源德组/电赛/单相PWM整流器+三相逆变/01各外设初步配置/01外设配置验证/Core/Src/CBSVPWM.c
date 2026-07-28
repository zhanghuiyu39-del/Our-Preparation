#include "CBSVPWM.h"

#include <float.h>
#include <stddef.h>
#include <string.h>

/*
 * 本文件只负责“三相电压指令 -> 零序注入 -> 桥臂占空比”的数学运算，
 * 不负责ADC采样、HRTIM寄存器更新、Gate Enable或故障状态机。
 * 数据流如下：
 * 三相电压指令(V) -> 除以Vdc/2归一化 -> 去除三相平均值
 *                  -> 最大最小值零序注入 -> 统一限幅 -> duty_u/v/w/n
 *
 * vu/vv/vw_command：控制器或开环发生器给出的三相电压指令，单位V。
 * modulation_u/v/w/n：桥臂无量纲调制量，0对应50%占空比。
 * duty_u/v/w/n：最终占空比，范围0~1，由应用层写入具体PWM外设。
 */

/* 用乘法代替运行时除法构造三相平均值，同时保持所有运算为单精度。 */
#define CBSVPWM_ONE_THIRD (0.33333333333333333333f)

/* 判断输入是否为有限单精度数，避免依赖不同ARM C库对isfinite()的实现。 */
static uint8_t CBSVPWM_IsFinite(float value)
{
    if ((value != value) || (value > FLT_MAX) || (value < -FLT_MAX)) {
        return 0U;
    }

    return 1U;
}

/* 返回单精度数的绝对值；输入在调用前已经完成有限性检查。 */
static float CBSVPWM_Abs(float value)
{
    return (value < 0.0f) ? -value : value;
}

/* 求三个调制量的最大值，用于仿真模型中的最大最小值零序注入。 */
static float CBSVPWM_Max3(float a, float b, float c)
{
    float maximum = (a > b) ? a : b;
    return (maximum > c) ? maximum : c;
}

/* 求三个调制量的最小值，用于仿真模型中的最大最小值零序注入。 */
static float CBSVPWM_Min3(float a, float b, float c)
{
    float minimum = (a < b) ? a : b;
    return (minimum < c) ? minimum : c;
}

/*
 * 把运行结果恢复到中性状态。
 * 该操作不修改用户配置和initialized，因此Reset后可以直接进行下一次有效计算。
 */
static void CBSVPWM_SetNeutralOutput(CBSVPWM_t *svpwm)
{
    svpwm->normalized_u = 0.0f;
    svpwm->normalized_v = 0.0f;
    svpwm->normalized_w = 0.0f;
    svpwm->average = 0.0f;
    svpwm->zero_sequence = 0.0f;
    svpwm->applied_scale = 1.0f;

    svpwm->modulation_u = 0.0f;
    svpwm->modulation_v = 0.0f;
    svpwm->modulation_w = 0.0f;
    svpwm->modulation_n = 0.0f;

    svpwm->duty_u = 0.5f;
    svpwm->duty_v = 0.5f;
    svpwm->duty_w = 0.5f;
    svpwm->duty_n = 0.5f;

    svpwm->valid = 0U;
    svpwm->limited = 0U;
}

/* 将理论占空比约束到0～1，防止单精度舍入在边界处产生轻微越界。 */
static float CBSVPWM_LimitDuty(float duty)
{
    if (duty > 1.0f) {
        return 1.0f;
    }

    if (duty < 0.0f) {
        return 0.0f;
    }

    return duty;
}

/*
 * 三桥臂和四桥臂共用的计算主体。
 * four_leg为0时只计算U/V/W；为1时额外使用仿真模型的Mn=mzero-mavg关系。
 */
static uint8_t CBSVPWM_CalcInternal(CBSVPWM_t *svpwm,
                                    float vu_command,
                                    float vv_command,
                                    float vw_command,
                                    float dc_voltage,
                                    uint8_t four_leg)
{
    /* 归一化增益单位为1/V，用于把相电压指令换算成无量纲调制指令。 */
    float normalization_gain;

    /* 去除三相平均值后的无量纲分量，是零序注入算法的实际输入。 */
    float centered_u;
    float centered_v;
    float centered_w;

    /* 去平均后三相分量的极值，用于计算公共零序分量。 */
    float maximum;
    float minimum;

    /* 尚未执行统一限幅的U/V/W/N桥臂无量纲调制量。 */
    float modulation_u;
    float modulation_v;
    float modulation_w;
    float modulation_n;

    /* 最大桥臂指令绝对值，以及保持相间比例不变的公共缩放系数(0, 1]。 */
    float maximum_absolute;
    float scale;

    /* 未初始化对象和非法物理输入不能参与除法或转换为PWM占空比。 */
    if ((svpwm == NULL) || (svpwm->initialized == 0U)) {
        if (svpwm != NULL) {
            CBSVPWM_SetNeutralOutput(svpwm);
        }
        return 0U;
    }

    if ((CBSVPWM_IsFinite(vu_command) == 0U) ||
        (CBSVPWM_IsFinite(vv_command) == 0U) ||
        (CBSVPWM_IsFinite(vw_command) == 0U) ||
        (CBSVPWM_IsFinite(dc_voltage) == 0U) ||
        (dc_voltage < svpwm->minimum_dc_voltage)) {
        CBSVPWM_SetNeutralOutput(svpwm);
        return 0U;
    }

    /*
     * 与Simulink模型一致，三相电压指令除以Vdc/2得到归一化调制指令。
     * 归一化后再次检查有限性，覆盖极端输入导致单精度乘法溢出的情况。
     */
    normalization_gain = 2.0f / dc_voltage;
    svpwm->normalized_u = vu_command * normalization_gain;
    svpwm->normalized_v = vv_command * normalization_gain;
    svpwm->normalized_w = vw_command * normalization_gain;

    if ((CBSVPWM_IsFinite(svpwm->normalized_u) == 0U) ||
        (CBSVPWM_IsFinite(svpwm->normalized_v) == 0U) ||
        (CBSVPWM_IsFinite(svpwm->normalized_w) == 0U)) {
        CBSVPWM_SetNeutralOutput(svpwm);
        return 0U;
    }

    /*
     * InvertQuTrans子系统先分离三相平均值，再对去平均值后的三相量注入SVPWM零序。
     * 分别乘1/3后再相加可降低大数相加产生中间溢出的风险。
     */
    svpwm->average = svpwm->normalized_u * CBSVPWM_ONE_THIRD +
                     svpwm->normalized_v * CBSVPWM_ONE_THIRD +
                     svpwm->normalized_w * CBSVPWM_ONE_THIRD;
    centered_u = svpwm->normalized_u - svpwm->average;
    centered_v = svpwm->normalized_v - svpwm->average;
    centered_w = svpwm->normalized_w - svpwm->average;

    if ((CBSVPWM_IsFinite(svpwm->average) == 0U) ||
        (CBSVPWM_IsFinite(centered_u) == 0U) ||
        (CBSVPWM_IsFinite(centered_v) == 0U) ||
        (CBSVPWM_IsFinite(centered_w) == 0U)) {
        CBSVPWM_SetNeutralOutput(svpwm);
        return 0U;
    }

    /* 标准CBSVPWM最大最小值法：mzero=-(max+min)/2。 */
    maximum = CBSVPWM_Max3(centered_u, centered_v, centered_w);
    minimum = CBSVPWM_Min3(centered_u, centered_v, centered_w);
    svpwm->zero_sequence = -0.5f * maximum - 0.5f * minimum;

    modulation_u = centered_u + svpwm->zero_sequence;
    modulation_v = centered_v + svpwm->zero_sequence;
    modulation_w = centered_w + svpwm->zero_sequence;

    /*
     * 四桥臂中性调制量对应目标模型的Gn：Mn=mzero-mavg。
     * 三桥臂没有N桥臂，固定为零调制量，即50%中性占空比。
     */
    modulation_n = (four_leg != 0U) ?
                   (svpwm->zero_sequence - svpwm->average) : 0.0f;

    if ((CBSVPWM_IsFinite(svpwm->zero_sequence) == 0U) ||
        (CBSVPWM_IsFinite(modulation_u) == 0U) ||
        (CBSVPWM_IsFinite(modulation_v) == 0U) ||
        (CBSVPWM_IsFinite(modulation_w) == 0U) ||
        (CBSVPWM_IsFinite(modulation_n) == 0U)) {
        CBSVPWM_SetNeutralOutput(svpwm);
        return 0U;
    }

    /*
     * 限幅时对全部已启用桥臂使用同一比例，保持相间电压矢量方向不变。
     * 逐相独立削顶会破坏三相比例，因此不用于CBSVPWM线性区限制。
     */
    maximum_absolute = CBSVPWM_Max3(CBSVPWM_Abs(modulation_u),
                                    CBSVPWM_Abs(modulation_v),
                                    CBSVPWM_Abs(modulation_w));
    if ((four_leg != 0U) && (CBSVPWM_Abs(modulation_n) > maximum_absolute)) {
        maximum_absolute = CBSVPWM_Abs(modulation_n);
    }

    scale = 1.0f;
    svpwm->limited = 0U;
    if (maximum_absolute > svpwm->modulation_limit) {
        scale = svpwm->modulation_limit / maximum_absolute;
        svpwm->limited = 1U;
    }

    svpwm->applied_scale = scale;
    svpwm->modulation_u = modulation_u * scale;
    svpwm->modulation_v = modulation_v * scale;
    svpwm->modulation_w = modulation_w * scale;
    svpwm->modulation_n = modulation_n * scale;

    /* 载波比较的等效占空比为duty=(1+M)/2，应用层再映射到具体定时器。 */
    svpwm->duty_u = CBSVPWM_LimitDuty(0.5f * (1.0f + svpwm->modulation_u));
    svpwm->duty_v = CBSVPWM_LimitDuty(0.5f * (1.0f + svpwm->modulation_v));
    svpwm->duty_w = CBSVPWM_LimitDuty(0.5f * (1.0f + svpwm->modulation_w));
    svpwm->duty_n = CBSVPWM_LimitDuty(0.5f * (1.0f + svpwm->modulation_n));
    svpwm->valid = 1U;

    return 1U;
}

/**
 * @brief  初始化CBSVPWM对象并把输出恢复为50%中性占空比。
 * @param  svpwm CBSVPWM运行对象地址，不能为NULL。
 * @param  modulation_limit 最大桥臂调制度，范围为(0, 1]，无量纲。
 * @param  minimum_dc_voltage 允许参与归一化计算的最低母线电压，单位V。
 * @retval 1 参数有效且初始化完成；0 参数非法或对象地址为空。
 * @note   在控制ISR启动前调用一次；本函数不访问HRTIM或Gate Enable。
 */
uint8_t CBSVPWM_Init(CBSVPWM_t *svpwm,
                     float modulation_limit,
                     float minimum_dc_voltage)
{
    if (svpwm == NULL) {
        return 0U;
    }

    /* 先清除整个对象，确保参数非法时initialized保持为0且不存在旧状态残留。 */
    (void)memset(svpwm, 0, sizeof(*svpwm));
    CBSVPWM_SetNeutralOutput(svpwm);

    if ((CBSVPWM_IsFinite(modulation_limit) == 0U) ||
        (CBSVPWM_IsFinite(minimum_dc_voltage) == 0U) ||
        (modulation_limit <= 0.0f) ||
        (modulation_limit > 1.0f) ||
        (minimum_dc_voltage <= 0.0f)) {
        return 0U;
    }

    svpwm->modulation_limit = modulation_limit;
    svpwm->minimum_dc_voltage = minimum_dc_voltage;
    svpwm->initialized = 1U;

    return 1U;
}

/**
 * @brief  清除最近一次计算状态并恢复四路50%占空比。
 * @param  svpwm 已初始化的CBSVPWM运行对象地址；NULL输入会直接返回。
 * @note   保留初始化参数，停机或故障后仍需由应用层单独关闭PWM输出。
 */
void CBSVPWM_Reset(CBSVPWM_t *svpwm)
{
    if (svpwm == NULL) {
        return;
    }

    CBSVPWM_SetNeutralOutput(svpwm);
}

/**
 * @brief  根据U/V/W三相电压指令计算三桥臂CBSVPWM占空比。
 * @param  svpwm CBSVPWM运行对象地址。
 * @param  vu_command U相电压指令，单位V。
 * @param  vv_command V相电压指令，单位V。
 * @param  vw_command W相电压指令，单位V。
 * @param  dc_voltage 当前直流母线电压，单位V。
 * @retval 1 本周期计算有效；0 输入非法，此时输出恢复为50%占空比。
 * @note   函数无阻塞，可在10 kHz控制ISR中调用，但不会自动更新HRTIM。
 */
uint8_t CBSVPWM_Calc3Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage)
{
    return CBSVPWM_CalcInternal(svpwm,
                                vu_command,
                                vv_command,
                                vw_command,
                                dc_voltage,
                                0U);
}

/**
 * @brief  根据U/V/W三相电压指令计算U/V/W/N四桥臂CBSVPWM占空比。
 * @param  svpwm CBSVPWM运行对象地址。
 * @param  vu_command U相对目标中性点的电压指令，单位V。
 * @param  vv_command V相对目标中性点的电压指令，单位V。
 * @param  vw_command W相对目标中性点的电压指令，单位V。
 * @param  dc_voltage 当前直流母线电压，单位V。
 * @retval 1 本周期计算有效；0 输入非法，此时输出恢复为50%占空比。
 * @note   这里只计算N桥臂占空比，不代表HRTIM F及其功率硬件已经启用。
 */
uint8_t CBSVPWM_Calc4Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage)
{
    return CBSVPWM_CalcInternal(svpwm,
                                vu_command,
                                vv_command,
                                vw_command,
                                dc_voltage,
                                1U);
}
