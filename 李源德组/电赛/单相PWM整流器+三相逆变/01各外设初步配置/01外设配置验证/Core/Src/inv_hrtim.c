#include "inv_hrtim.h"

#include <float.h>

#include "gpio.h"
#include "hrtim.h"

/*
 * 本文件只负责“三相占空比 -> HRTIM C/D/E比较值 -> PWM输出”的底层映射，
 * 不负责CBSVPWM计算、ADC数据处理、OLED显示或带功率启动条件判断。
 * 数据流如下：
 * duty_u/v/w -> 反向换算CMP1 -> C/D/E预装载寄存器 -> Valley边界同步生效
 *
 * Master提供ADC Trigger 2公共时基；Timer C/D/E分别对应U/V/W桥臂。
 * Output 1为主PWM，Output 2由HRTIM死区单元生成互补波形。
 * INV_HRTIM_StartSampling()只启动采样时基，只有EnablePowerStage()才可能拉高PE1。
 */

/* U/V/W分别使用Timer C/D/E，每相的Output 2由HRTIM死区单元生成互补波形。 */
#define INV_HRTIM_OUTPUTS (HRTIM_OUTPUT_TC1 | HRTIM_OUTPUT_TC2 | \
                           HRTIM_OUTPUT_TD1 | HRTIM_OUTPUT_TD2 | \
                           HRTIM_OUTPUT_TE1 | HRTIM_OUTPUT_TE2)

/* 采样启动需要共享Master和逆变专用Timer C/D/E同时计数。 */
#define INV_HRTIM_COUNTERS (HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_C | \
                            HRTIM_TIMERID_TIMER_D | HRTIM_TIMERID_TIMER_E)

/* 安全停机只停止逆变专用计数器，保留可能同时服务PFC的共享Master。 */
#define INV_HRTIM_INV_COUNTERS (HRTIM_TIMERID_TIMER_C | \
                                HRTIM_TIMERID_TIMER_D | \
                                HRTIM_TIMERID_TIMER_E)

/* 0.05～0.95占空比按反向映射对应CMP1=32300～1700。 */
#define INV_HRTIM_COMPARE_MIN (1700U)
#define INV_HRTIM_COMPARE_MAX (32300U)
/* 中心对齐周期34000时，CMP1=17000对应50%中性占空比。 */
#define INV_HRTIM_NEUTRAL_CMP (17000U)

/* 软件启动前置标志：仅表示采样计数器已启动，不代表PWM输出或PE1已经使能。 */
static uint8_t inv_sampling_started = 0U;

/* 避免依赖不同ARM C库的isfinite()实现。 */
static uint8_t INV_HRTIM_IsFinite(float value)
{
    if ((value != value) || (value > FLT_MAX) || (value < -FLT_MAX)) {
        return 0U;
    }

    return 1U;
}

/* FLT3事件标志由硬件置位；本模块只读取，不擅自清除公共故障记录。 */
static uint8_t INV_HRTIM_IsFault3Set(void)
{
    return (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET) ? 1U : 0U;
}

/* 当前Valley/Set-Reset极性已经确定为CMP=Period*(1-duty)。 */
static uint16_t INV_HRTIM_DutyToCompare(float duty)
{
    /* 先以浮点数计算反向映射结果，返回前加0.5实现最接近整数的舍入。 */
    float compare = (float)INV_HRTIM_PERIOD_COUNTS * (1.0f - duty);
    return (uint16_t)(compare + 0.5f);
}

/**
 * @brief  启动Master及Timer C/D/E计数器，为ADC Trigger 2提供10 kHz时基。
 * @retval HAL_OK 计数器启动成功；其他值表示HAL启动失败。
 * @note   在ADC3/4/5 DMA进入等待触发状态后调用；本函数强制PE1为低并关闭PWM输出。
 */
HAL_StatusTypeDef INV_HRTIM_StartSampling(void)
{
    /* 保存HAL计数器启动结果，并据此更新模块的软件前置状态。 */
    HAL_StatusTypeDef status;

    /* 计数器启动前再次建立硬件安全态，防止旧调试状态遗留输出使能。 */
    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

    if (hhrtim1.Instance != HRTIM1) {
        inv_sampling_started = 0U;
        return HAL_ERROR;
    }

    (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, INV_HRTIM_OUTPUTS);
    status = HAL_HRTIM_WaveformCounterStart(&hhrtim1, INV_HRTIM_COUNTERS);
    inv_sampling_started = (status == HAL_OK) ? 1U : 0U;

    return status;
}

/**
 * @brief  将U/V/W三相占空比换算并写入Timer C/D/E的CMP1预装载寄存器。
 * @param  duty_u U相占空比，无量纲，允许范围0.05~0.95。
 * @param  duty_v V相占空比，无量纲，允许范围0.05~0.95。
 * @param  duty_w W相占空比，无量纲，允许范围0.05~0.95。
 * @retval HAL_OK 三相参数均有效并完成写入；HAL_ERROR表示参数非法。
 * @note   三相参数会先全部检查再统一写入，避免只更新部分桥臂。
 */
HAL_StatusTypeDef INV_HRTIM_SetDuty(float duty_u,
                                    float duty_v,
                                    float duty_w)
{
    /* 先验证全部输入，再统一写寄存器，避免只更新部分桥臂。 */
    if ((INV_HRTIM_IsFinite(duty_u) == 0U) ||
        (INV_HRTIM_IsFinite(duty_v) == 0U) ||
        (INV_HRTIM_IsFinite(duty_w) == 0U) ||
        (duty_u < INV_HRTIM_DUTY_MIN) || (duty_u > INV_HRTIM_DUTY_MAX) ||
        (duty_v < INV_HRTIM_DUTY_MIN) || (duty_v > INV_HRTIM_DUTY_MAX) ||
        (duty_w < INV_HRTIM_DUTY_MIN) || (duty_w > INV_HRTIM_DUTY_MAX)) {
        return HAL_ERROR;
    }

    return INV_HRTIM_SetCompare(INV_HRTIM_DutyToCompare(duty_u),
                                INV_HRTIM_DutyToCompare(duty_v),
                                INV_HRTIM_DutyToCompare(duty_w));
}

/**
 * @brief  直接写入U/V/W三相的CMP1计数值。
 * @param  cmp_u Timer C比较值，对应U相桥臂。
 * @param  cmp_v Timer D比较值，对应V相桥臂。
 * @param  cmp_w Timer E比较值，对应W相桥臂。
 * @retval HAL_OK 三个比较值均在1700~32300范围内；否则返回HAL_ERROR。
 * @note   CMP1启用预装载，写入值将在CubeMX配置的Valley更新边界同步生效。
 */
HAL_StatusTypeDef INV_HRTIM_SetCompare(uint16_t cmp_u,
                                       uint16_t cmp_v,
                                       uint16_t cmp_w)
{
    if ((hhrtim1.Instance != HRTIM1) ||
        (cmp_u < INV_HRTIM_COMPARE_MIN) || (cmp_u > INV_HRTIM_COMPARE_MAX) ||
        (cmp_v < INV_HRTIM_COMPARE_MIN) || (cmp_v > INV_HRTIM_COMPARE_MAX) ||
        (cmp_w < INV_HRTIM_COMPARE_MIN) || (cmp_w > INV_HRTIM_COMPARE_MAX)) {
        return HAL_ERROR;
    }

    /* 三个CMP1均启用预装载，将在CubeMX配置的Valley边界同步生效。 */
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C,
                           HRTIM_COMPAREUNIT_1, cmp_u);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D,
                           HRTIM_COMPAREUNIT_1, cmp_v);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E,
                           HRTIM_COMPAREUNIT_1, cmp_w);

    return HAL_OK;
}

/**
 * @brief  以50%初始占空比开放六路PWM，并在FLT3正常时拉高逆变Gate Enable。
 * @retval HAL_OK PWM输出和PE1均已使能；其他值表示前置条件或HAL调用失败。
 * @note   该函数会真正使能外部门极驱动器，只能由通过全部安全检查的状态机调用。
 */
HAL_StatusTypeDef INV_HRTIM_EnablePowerStage(void)
{
    /* 保存六路输出启动结果；只有HAL_OK且复查无Fault后才允许拉高PE1。 */
    HAL_StatusTypeDef status;

    /* PE1最后才允许置位；任何前置检查失败都保持驱动关闭。 */
    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

    if ((hhrtim1.Instance != HRTIM1) ||
        (inv_sampling_started == 0U) ||
        (INV_HRTIM_IsFault3Set() != 0U)) {
        return HAL_ERROR;
    }

    if (INV_HRTIM_SetCompare(INV_HRTIM_NEUTRAL_CMP,
                             INV_HRTIM_NEUTRAL_CMP,
                             INV_HRTIM_NEUTRAL_CMP) != HAL_OK) {
        return HAL_ERROR;
    }

    status = HAL_HRTIM_WaveformOutputStart(&hhrtim1, INV_HRTIM_OUTPUTS);
    if (status != HAL_OK) {
        INV_HRTIM_StopAll();
        return status;
    }

    /* 输出开放后复查FLT3，覆盖启动边界恰好到来的驱动器故障。 */
    if (INV_HRTIM_IsFault3Set() != 0U) {
        INV_HRTIM_StopAll();
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_SET);
    return HAL_OK;
}

/**
 * @brief  关闭逆变Gate Enable、C/D/E六路PWM输出和逆变专用计数器。
 * @note   可在Fault或ADC/DMA错误路径调用；保留共享Master，避免影响PFC时基。
 */
void INV_HRTIM_StopAll(void)
{
    /* 安全顺序固定为先关驱动器，再撤销PWM输出和逆变专用计数器。 */
    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

    if (hhrtim1.Instance == HRTIM1) {
        (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, INV_HRTIM_OUTPUTS);
        (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, INV_HRTIM_INV_COUNTERS);
    }

    inv_sampling_started = 0U;
}
