#include "SPWM.h"
#include "pfc_hrtim.h"
#include <float.h>
#include <math.h>

/*
 * 本模块负责“同步测量 -> 电压前馈开环模型 -> Compare预装载”，不负责启停功率输出。
 * 数据流：PFC_Measurement -> VAC正向过零校正相位 -> i_ref及di_ref/dt
 *                          -> v_bridge_ref -> 归一化调制量
 *                          -> Timer A/B Compare 1预装载 -> 周期边界生效。
 *
 * 当前模型为 v_bridge_ref = v_ac - L*di_ref/dt，IPFC只参与监测和保护，不反馈到该模型。
 * 因此它不是PR电流闭环，也不能保证9 V精确稳压；后续闭环只替换桥侧电压校正项。
 */
#define SPWM_HRTIM_PERIOD_COUNTS       34000U
#define SPWM_HRTIM_HALF_PERIOD_COUNTS  17000U
#define SPWM_TWO_PI                    6.28318530718f
#define SPWM_VBUS_DIV_MIN              2.0f
#define SPWM_CURRENT_RAMP_PER_STEP      0.00005f /* 0.05 A / 100 ms @ 10 kHz */

/* ADC1完整DMA ISR是这些运行量的唯一写入者；主循环只通过Getter读取单个标量。 */
static const PFC_Params *spwm_params = 0;   /* 初始化后只读，参数对象必须常驻。 */
static PFC_Measurement spwm_measurement = {0};
static float spwm_phase = 0.0f;             /* rad，0表示VAC正向过零。 */
static float spwm_phase_step = 0.0f;        /* rad/control step */
static float spwm_current_command = 0.0f;   /* A peak */
static float spwm_modulation = 0.0f;        /* 归一化差分调制量 */
static uint32_t spwm_last_cross_sequence = 0U;
static uint8_t spwm_synchronized = 0U;
static uint8_t spwm_healthy = 0U;

static float SPWM_LimitModulation(float modulation)
{
    float limit = 0.0f;

    if ((spwm_params == 0) || (modulation != modulation) ||
        (modulation > FLT_MAX) || (modulation < -FLT_MAX))
    {
        spwm_healthy = 0U;
        return 0.0f;
    }

    limit = spwm_params->modulation_limit;
    if (modulation > limit)
    {
        return limit;
    }
    if (modulation < -limit)
    {
        return -limit;
    }
    return modulation;
}

void SPWM_Init(const PFC_Params *params)
{
    spwm_params = params;
    SPWM_Reset();
}

void SPWM_SyncUpdate(const PFC_Measurement *measurement)
{
    if ((measurement == 0) || (measurement->valid == 0U) || (spwm_params == 0))
    {
        spwm_healthy = 0U;
        return;
    }

    spwm_measurement = *measurement;
    if ((measurement->vac_locked != 0U) &&
        (measurement->vac_zero_cross_sequence != spwm_last_cross_sequence))
    {
        spwm_last_cross_sequence = measurement->vac_zero_cross_sequence;
        /* 测量模块只在VAC负到正过零时推进序列，因此把该时刻定义为theta=0。 */
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
}

void SPWM_OpenLoopRampStep(void)
{
    float omega;
    float di_ref_dt;
    float bridge_voltage_ref;
    float divisor;

    if ((spwm_params == 0) || (spwm_synchronized == 0U) ||
        (spwm_measurement.valid == 0U))
    {
        spwm_healthy = 0U;
        return;
    }

    /* 首次启动直接给0.15 A峰值，之后每100 ms约增加0.05 A，直到母线或指令上限。 */
    if (spwm_current_command < spwm_params->current_command_start)
    {
        spwm_current_command = spwm_params->current_command_start;
    }
    else if ((spwm_measurement.vbus < spwm_params->vbus_target) &&
             (spwm_current_command < spwm_params->current_command_max))
    {
        spwm_current_command += SPWM_CURRENT_RAMP_PER_STEP;
        if (spwm_current_command > spwm_params->current_command_max)
        {
            spwm_current_command = spwm_params->current_command_max;
        }
    }

    omega = SPWM_TWO_PI * spwm_measurement.vac_frequency_hz;
    di_ref_dt = omega * spwm_current_command * cosf(spwm_phase);
    /* L*di/dt为电感目标压降；bridge_polarity用于匹配功率板桥臂实际接线方向。 */
    bridge_voltage_ref = spwm_measurement.vac -
                         (spwm_params->input_inductance * di_ref_dt);
    divisor = (spwm_measurement.vbus > SPWM_VBUS_DIV_MIN) ?
              spwm_measurement.vbus : SPWM_VBUS_DIV_MIN;
    spwm_modulation = SPWM_ApplyModulation(
        (float)spwm_params->bridge_polarity * bridge_voltage_ref / divisor);
}

float SPWM_ApplyModulation(float modulation)
{
    float applied = SPWM_LimitModulation(modulation);
    uint32_t compare_a = (uint32_t)((float)SPWM_HRTIM_HALF_PERIOD_COUNTS *
                                    (1.0f - applied) + 0.5f);
    uint32_t compare_b = (uint32_t)((float)SPWM_HRTIM_HALF_PERIOD_COUNTS *
                                    (1.0f + applied) + 0.5f);

    /* 正调制使Timer A Compare减小、Timer B Compare增大，形成全桥差分输出。 */
    PFC_HRTIM_SetCompare((uint16_t)compare_a, (uint16_t)compare_b);
    return applied;
}

float SPWM_GetModulation(void)
{
    return spwm_modulation;
}

float SPWM_GetCurrentCommand(void)
{
    return spwm_current_command;
}

uint8_t SPWM_IsSynchronized(void)
{
    return spwm_synchronized;
}

uint8_t SPWM_IsHealthy(void)
{
    return spwm_healthy;
}

void SPWM_ForceNeutral(void)
{
    spwm_modulation = SPWM_ApplyModulation(0.0f);
}

void SPWM_Reset(void)
{
    spwm_phase = 0.0f;
    spwm_phase_step = 0.0f;
    spwm_current_command = 0.0f;
    spwm_modulation = 0.0f;
    spwm_last_cross_sequence = 0U;
    spwm_synchronized = 0U;
    spwm_healthy = (spwm_params != 0) ? 1U : 0U;
    SPWM_ForceNeutral();
}
