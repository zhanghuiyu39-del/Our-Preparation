#include "SPWM.h"
#include "pfc_hrtim.h"
#include <float.h>
#include <math.h>

/*
 * 本模块负责“有效测量 -> VAC同步 -> 开环电感模型 -> Compare预装载”，不负责启停。
 * 模型：i_ref=Ipk*sin(theta)，v_bridge=vac-L*di_ref/dt，m=v_bridge/vbus。
 * IPFC由上层PR/PI控制器使用；本模块只负责把全桥归一化命令转换为Compare，
 * 不负责参数档的精密母线稳压。
 */
#define SPWM_TWO_PI        6.28318530718f /* rad，一个完整电角周期。 */
#define SPWM_VBUS_DIV_MIN  2.0f          /* V，调制除法允许使用的最小母线电压。 */

/*
 * 初始化完成后常驻的只读参数地址。SPWM_Init写入，其余函数只读；参数对象必须常驻。
 */
static const PFC_Params *spwm_params = 0;

/* 最近一次有效同步快照的模块内副本，由ADC1完整DMA回调路径写和使用。 */
static PFC_Measurement spwm_measurement = {0};

/* 下列10 kHz运行量只由ADC1完整DMA回调路径写；主循环通过Getter读取标量。 */
static float spwm_phase = 0.0f;           /* rad，VAC负到正过零定义为0。 */
static float spwm_phase_step = 0.0f;      /* rad/控制周期，由实测VAC频率计算。 */
static float spwm_current_command = 0.0f; /* A peak，从0按参数斜坡增加。 */
static float spwm_modulation = 0.0f;      /* 归一化差分调制量，受modulation_limit限制。 */
static uint32_t spwm_last_cross_sequence = 0U; /* 最近已处理的VAC正向过零序列号。 */
static uint8_t spwm_synchronized = 0U;   /* 1表示已用有效正向过零建立连续相位。 */
static uint8_t spwm_healthy = 0U;        /* 1表示最近一次输入检查和Compare写入成功。 */
static uint8_t spwm_limited = 0U;        /* 1表示最近一次调制指令触及软件限幅。 */

/**
 * @brief  判断单精度数值是否既非NaN也非正负无穷。
 * @param  value 待检查浮点数。
 * @retval 1表示有限数，0表示NaN或无穷。
 */
static uint8_t SPWM_FloatValid(float value)
{
    return (uint8_t)((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

/**
 * @brief  绑定常驻参数、清除同步/斜坡状态并写入50%中性Compare。
 * @param  params 常驻只读参数档地址；本函数不取得其所有权。
 * @retval HAL_OK表示参数地址和中性Compare写入有效，否则返回HAL_ERROR。
 * @note   在HRTIM输出开放前调用；不启动计数器，也不开放PWM引脚。
 */
HAL_StatusTypeDef SPWM_Init(const PFC_Params *params)
{
    spwm_params = params;
    SPWM_Reset();
    return (spwm_healthy != 0U) ? HAL_OK : HAL_ERROR;
}

/**
 * @brief  接收最新测量快照，并按VAC正向过零校正开环相位和频率步进。
 * @param  measurement 当前ADC1控制周期的一致性有效快照。
 * @retval HAL_OK表示快照和参数有效，HAL_ERROR表示不能继续调制。
 * @note   每个10 kHz ADC1完整DMA周期调用一次；本函数本身不写HRTIM Compare。
 */
HAL_StatusTypeDef SPWM_SyncUpdate(const PFC_Measurement *measurement)
{
    if ((measurement == 0) || (measurement->valid == 0U) ||
        (PFC_Params_PowerAllowed(spwm_params) == 0U))
    {
        spwm_healthy = 0U;
        return HAL_ERROR;
    }

    spwm_measurement = *measurement;
    if ((measurement->vac_locked != 0U) &&
        (measurement->vac_zero_cross_sequence != spwm_last_cross_sequence))
    {
        spwm_last_cross_sequence = measurement->vac_zero_cross_sequence;
        spwm_phase = 0.0f;
        spwm_phase_step = SPWM_TWO_PI * measurement->vac_frequency_hz /
                          spwm_params->control_frequency;
        spwm_synchronized = 1U;
    }
    else if (spwm_synchronized != 0U)
    {
        spwm_phase += spwm_phase_step;
        if (spwm_phase >= SPWM_TWO_PI)
        {
            spwm_phase -= SPWM_TWO_PI;
        }
    }

    spwm_healthy = 1U;
    return HAL_OK;
}

/**
 * @brief  执行一次电流峰值斜坡、开环电感前馈计算和HRTIM Compare更新。
 * @retval HAL_OK表示调制量有效且Compare写入成功，否则返回HAL_ERROR。
 * @note   只在RAMP/RUN状态的10 kHz ADC ISR路径调用；当前不是电流闭环。
 */
HAL_StatusTypeDef SPWM_OpenLoopRampStep(void)
{
    float omega;              /* rad/s，由锁定后的VAC频率计算。 */
    float di_ref_dt;          /* A/s，正弦电流参考的解析导数。 */
    float bridge_voltage_ref; /* V，电网电压减去电感压降后的全桥参考。 */
    float ramp_step;          /* A peak/控制周期，本次电流峰值指令增量。 */

    if ((spwm_params == 0) || (spwm_synchronized == 0U) ||
        (spwm_measurement.valid == 0U) ||
        (spwm_measurement.vbus <= SPWM_VBUS_DIV_MIN))
    {
        spwm_healthy = 0U;
        return HAL_ERROR;
    }

    if ((spwm_measurement.vbus < spwm_params->vbus_target) &&
        (spwm_current_command < spwm_params->current_command_target))
    {
        ramp_step = spwm_params->current_ramp_a_per_s /
                    spwm_params->control_frequency;
        spwm_current_command += ramp_step;
        if (spwm_current_command > spwm_params->current_command_target)
        {
            spwm_current_command = spwm_params->current_command_target;
        }
    }

    omega = SPWM_TWO_PI * spwm_measurement.vac_frequency_hz;
    di_ref_dt = omega * spwm_current_command * cosf(spwm_phase);
    bridge_voltage_ref = spwm_measurement.vac -
                         (spwm_params->input_inductance * di_ref_dt);

    return SPWM_ApplyModulation((float)spwm_params->bridge_polarity *
                                bridge_voltage_ref / spwm_measurement.vbus);
}

/**
 * @brief  校验并限幅差分调制量，再映射为Timer A/B互补的Compare 1值。
 * @param  modulation 归一化差分调制指令，正负方向由bridge_polarity统一处理。
 * @retval HAL_OK表示两个Compare预装载写入成功，HAL_ERROR表示数值或HRTIM无效。
 * @note   映射关系为A=0.5*(1-m)、B=0.5*(1+m)；不改变计数器和输出开放状态。
 */
HAL_StatusTypeDef SPWM_ApplyModulation(float modulation)
{
    float applied;     /* 实际采用的归一化调制量，范围为正负modulation_limit。 */
    uint32_t compare_a;/* Timer A Compare 1计数值，写入前已限制在合法范围。 */
    uint32_t compare_b;/* Timer B Compare 1计数值，方向与Timer A相反。 */

    if ((spwm_params == 0) || (SPWM_FloatValid(modulation) == 0U))
    {
        spwm_healthy = 0U;
        return HAL_ERROR;
    }

    applied = modulation;
    spwm_limited = 0U;
    if (applied > spwm_params->modulation_limit)
    {
        applied = spwm_params->modulation_limit;
        spwm_limited = 1U;
    }
    else if (applied < -spwm_params->modulation_limit)
    {
        applied = -spwm_params->modulation_limit;
        spwm_limited = 1U;
    }

    compare_a = (uint32_t)((float)PFC_HRTIM_HALF_PERIOD_COUNTS *
                            (1.0f - applied) + 0.5f);
    compare_b = (uint32_t)((float)PFC_HRTIM_HALF_PERIOD_COUNTS *
                            (1.0f + applied) + 0.5f);

    if (PFC_HRTIM_SetCompare((uint16_t)compare_a, (uint16_t)compare_b) != HAL_OK)
    {
        spwm_healthy = 0U;
        return HAL_ERROR;
    }

    spwm_modulation = applied;
    spwm_healthy = 1U;
    return HAL_OK;
}

/** @brief 返回最近成功写入的归一化调制量；主循环只读。 */
float SPWM_GetModulation(void) { return spwm_modulation; }
/** @brief 返回当前开环电流峰值指令，单位A peak；主循环只读。 */
float SPWM_GetCurrentCommand(void) { return spwm_current_command; }
/** @brief 返回1表示VAC正向过零已建立开环相位，返回0表示尚未同步。 */
uint8_t SPWM_IsSynchronized(void) { return spwm_synchronized; }
/** @brief 返回1表示最近一次调制路径正常，返回0表示最近发生输入或写入错误。 */
uint8_t SPWM_IsHealthy(void) { return spwm_healthy; }
/** @brief 返回1表示最近调制指令被限幅，返回0表示未触及限幅。 */
uint8_t SPWM_IsLimited(void) { return spwm_limited; }

/**
 * @brief  把Timer A/B Compare恢复为相同的50%中性值。
 * @retval HAL_OK表示Compare写入成功，否则返回HAL_ERROR。
 * @note   不改变相位和电流斜坡，不启动或停止HRTIM计数器/输出。
 */
HAL_StatusTypeDef SPWM_ForceNeutral(void)
{
    /* 本地状态只反映本次中性Compare写入结果。 */
    HAL_StatusTypeDef status = PFC_HRTIM_SetCompare(PFC_HRTIM_HALF_PERIOD_COUNTS,
                                                     PFC_HRTIM_HALF_PERIOD_COUNTS);
    if (status == HAL_OK)
    {
        spwm_modulation = 0.0f;
        spwm_limited = 0U;
    }
    else
    {
        spwm_healthy = 0U;
    }
    return status;
}

/**
 * @brief  清除VAC同步、相位、电流斜坡和限幅状态，并强制写入中性Compare。
 * @note   初始化和人工停机时调用；不会关闭HRTIM输出，因此停机路径必须先关输出。
 */
void SPWM_Reset(void)
{
    spwm_phase = 0.0f;
    spwm_phase_step = 0.0f;
    spwm_current_command = 0.0f;
    spwm_modulation = 0.0f;
    spwm_last_cross_sequence = 0U;
    spwm_synchronized = 0U;
    spwm_limited = 0U;
    spwm_healthy = (spwm_params != 0) ? 1U : 0U;
    if (SPWM_ForceNeutral() != HAL_OK)
    {
        spwm_healthy = 0U;
    }
}
