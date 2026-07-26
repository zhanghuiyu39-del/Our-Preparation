#include "SPWM.h"  /* 本模块的公共接口声明。 */
#include "hrtim.h" /* CubeMX生成的HRTIM句柄和HAL宏声明。 */

#include <float.h>  /* FLT_MAX用于识别无穷大等非法浮点输入。 */
#include <stdint.h> /* uint16_t、uint32_t和int32_t定宽整数类型。 */

/* 当前CubeMX中Timer A/B的Up-Down周期值均为34000。 */
#define SPWM_HRTIM_PERIOD_COUNTS       (34000U)

/* 50%占空比对应的比较中心，同时也是差分调制的缩放系数。 */
#define SPWM_HRTIM_HALF_PERIOD_COUNTS  (SPWM_HRTIM_PERIOD_COUNTS / 2U)

/* 限制占空比在5%～95%，为死区和功率级切换保留安全余量。 */
#define SPWM_MODULATION_LIMIT          (0.90f)

/* 原正弦表共400点；10 kHz调用时每次跨2点得到200点/50 Hz。 */
#define SPWM_SINE_TABLE_SIZE           (400U)
#define SPWM_SINE_INDEX_STEP           (2U)

/* 原表的直流中心为1875，正弦峰值相对中心为1800。 */
#define SPWM_SINE_TABLE_CENTER         (1875)
#define SPWM_SINE_TABLE_AMPLITUDE      (1800.0f)

/* 开环试波使用的调制度，由SPWM_Init()或设置函数更新。 */
static float spwm_open_loop_amplitude = 0.0f;

/* 开环正弦相位索引，只允许由10 kHz控制入口更新。 */
static uint16_t spwm_sine_index = 0U;

/*
 * 400点正弦数据仅作为-1～+1波形模板使用。
 * 数值不再代表TIM1比较值，运行时会先减去1875再除以1800。
 */
static const uint16_t spwm_sine_table[SPWM_SINE_TABLE_SIZE] =
{
1875,1903,1931,1959,1988,2016,2044,2072,2100,2128,2156,2184,2212,2240,2267,2295,
2322,2349,2377,2404,2431,2458,2484,2511,2537,2563,2589,2615,2641,2666,2692,2717,
2742,2766,2791,2815,2839,2863,2886,2910,2933,2955,2978,3000,3022,3044,3065,3086,
3107,3127,3147,3167,3187,3206,3225,3243,3261,3279,3297,3314,3331,3347,3363,3379,
3394,3409,3424,3438,3452,3465,3478,3491,3503,3515,3526,3537,3548,3558,3568,3577,
3586,3595,3603,3611,3618,3625,3631,3637,3643,3648,3652,3657,3660,3664,3667,3669,
3671,3673,3674,3674,3675,3674,3674,3673,3671,3669,3667,3664,3660,3657,3652,3648,
3643,3637,3631,3625,3618,3611,3603,3595,3586,3577,3568,3558,3548,3537,3526,3515,
3503,3491,3478,3465,3452,3438,3424,3409,3394,3379,3363,3347,3331,3314,3297,3279,
3261,3243,3225,3206,3187,3167,3147,3127,3107,3086,3065,3044,3022,3000,2978,2955,
2933,2910,2886,2863,2839,2815,2791,2766,2742,2717,2692,2666,2641,2615,2589,2563,
2537,2511,2484,2458,2431,2404,2377,2349,2322,2295,2267,2240,2212,2184,2156,2128,
2100,2072,2044,2016,1988,1959,1931,1903,1875,1846,1818,1790,1761,1733,1705,1677,
1649,1621,1593,1565,1537,1509,1482,1454,1427,1400,1372,1345,1318,1291,1265,1238,
1212,1186,1160,1134,1108,1083,1057,1032,1007,983,958,934,910,886,863,839,
816,794,771,749,727,705,684,663,642,622,602,582,562,543,524,506,
488,470,452,435,418,402,386,370,355,340,325,311,297,284,271,258,
246,234,223,212,201,191,181,172,163,154,146,138,131,124,118,112,
106,101,97,92,89,85,82,80,78,76,75,75,75,75,75,76,
78,80,82,85,89,92,97,101,106,112,118,124,131,138,146,154,
163,172,181,191,201,212,223,234,246,258,271,284,297,311,325,340,
355,370,386,402,418,435,452,470,488,506,524,543,562,582,602,622,
642,663,684,705,727,749,771,794,816,839,863,886,910,934,958,983,
1007,1032,1057,1083,1108,1134,1160,1186,1212,1238,1265,1291,1318,1345,1372,1400,
1427,1454,1482,1509,1537,1565,1593,1621,1649,1677,1705,1733,1761,1790,1818,1846
};

/* 将任意开环幅值限制到模块允许的非负调制度范围。 */
static float SPWM_LimitAmplitude(float amplitude)
{
    /* NaN不满足自身相等；超过FLT_MAX的输入属于正负无穷大。 */
    if ((amplitude != amplitude) || (amplitude > FLT_MAX) || (amplitude < -FLT_MAX))
    {
        return 0.0f; /* 非有限输入回退到零调制度。 */
    }

    if (amplitude < 0.0f)
    {
        return 0.0f; /* 开环幅值不允许为负，相位由正弦表决定。 */
    }

    if (amplitude > SPWM_MODULATION_LIMIT)
    {
        return SPWM_MODULATION_LIMIT; /* 超过上限时限制到0.90。 */
    }

    return amplitude; /* 合法幅值保持不变。 */
}

/* 将瞬时调制量限制到-0.90～+0.90，并处理非有限输入。 */
static float SPWM_LimitModulation(float modulation)
{
    /* 非有限控制量不能直接换算为无符号Compare值。 */
    if ((modulation != modulation) || (modulation > FLT_MAX) || (modulation < -FLT_MAX))
    {
        return 0.0f; /* 非法输入使两个桥臂回到50%中性状态。 */
    }

    if (modulation > SPWM_MODULATION_LIMIT)
    {
        return SPWM_MODULATION_LIMIT; /* 限制正向最大调制度。 */
    }

    if (modulation < -SPWM_MODULATION_LIMIT)
    {
        return -SPWM_MODULATION_LIMIT; /* 限制负向最大调制度。 */
    }

    return modulation; /* 合法调制量保持不变。 */
}

/* 初始化开环幅值、相位和HRTIM中性比较值。 */
void SPWM_Init(float open_loop_amplitude)
{
    SPWM_SetOpenLoopAmplitude(open_loop_amplitude); /* 保存经过限幅的开环调制度。 */
    SPWM_Reset();                                   /* 从零相位和50%占空比开始。 */
}

/* 在运行期间修改开环试波幅值，不引入相位跳变。 */
void SPWM_SetOpenLoopAmplitude(float amplitude)
{
    spwm_open_loop_amplitude = SPWM_LimitAmplitude(amplitude); /* 只保存安全范围内的幅值。 */
}

/* 把一个瞬时调制量转换为Timer A/B的差分比较值。 */
float SPWM_ApplyModulation(float modulation)
{
    float applied_modulation; /* 经过有效性检查和上下限处理后的调制量。 */
    float compare_a_float;    /* Timer A比较值的浮点中间结果。 */
    float compare_b_float;    /* Timer B比较值的浮点中间结果。 */
    uint32_t compare_a;       /* Timer A Compare 1最终整数值。 */
    uint32_t compare_b;       /* Timer B Compare 1最终整数值。 */

    applied_modulation = SPWM_LimitModulation(modulation); /* 防止Compare越过安全范围。 */

    /* 正调制时A桥臂占空比增加，因此Valley模式下CMP_A减小。 */
    compare_a_float = (float)SPWM_HRTIM_HALF_PERIOD_COUNTS * (1.0f - applied_modulation);

    /* B桥臂使用反向调制，形成D和1-D的单极性全桥差分关系。 */
    compare_b_float = (float)SPWM_HRTIM_HALF_PERIOD_COUNTS * (1.0f + applied_modulation);

    compare_a = (uint32_t)(compare_a_float + 0.5f); /* 四舍五入为HRTIM计数值。 */
    compare_b = (uint32_t)(compare_b_float + 0.5f); /* 四舍五入为HRTIM计数值。 */

    /* Timer A/B均启用预装载，两个新值会在同一个Valley更新边界生效。 */
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_1, compare_a);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_1, compare_b);

    return applied_modulation; /* 供后续闭环监视实际采用的调制量。 */
}

/* 在10 kHz入口中推进一次50 Hz开环正弦。 */
void SPWM_OpenLoopStep(void)
{
    int32_t centered_sample; /* 去除原表直流中心后的有符号正弦样本。 */
    float normalized_sine;   /* 归一化到约-1～+1的正弦值。 */
    float modulation;        /* 本控制周期需要写入HRTIM的瞬时调制量。 */

    centered_sample = (int32_t)spwm_sine_table[spwm_sine_index] - SPWM_SINE_TABLE_CENTER;
    normalized_sine = (float)centered_sample / SPWM_SINE_TABLE_AMPLITUDE;
    modulation = spwm_open_loop_amplitude * normalized_sine;

    (void)SPWM_ApplyModulation(modulation); /* 写入A/B桥臂本周期的差分比较值。 */

    spwm_sine_index = (uint16_t)(spwm_sine_index + SPWM_SINE_INDEX_STEP); /* 10 kHz下每次跨两个表点。 */

    if (spwm_sine_index >= SPWM_SINE_TABLE_SIZE)
    {
        spwm_sine_index = 0U; /* 200次调用后回到零相位，形成50 Hz周期。 */
    }
}

/* 故障、停机或重新开环前清零相位和调制输出。 */
void SPWM_Reset(void)
{
    spwm_sine_index = 0U;              /* 下一次开环更新从正弦零点开始。 */
    (void)SPWM_ApplyModulation(0.5f);   /* 两个桥臂Compare均恢复到17000。 */
}


