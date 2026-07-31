#include "pfc_control.h"

#include "PI.h"
#include "SPWM.h"
#include <float.h>
#include <math.h>
#include <string.h>

/*
 * 模块职责与数据流：
 *
 * 固定VBUS反馈 -> 1 kHz PI -> RMS电流幅值 -> 10 kHz软启动
 * 额定50 Hz软件相位 -> 理想VAC/Iref -> 电感压降前馈 -> 调制量 -> SPWM/HRTIM
 *
 * PFC_USER_ENABLE_ADC=0时不读取任何ADC数据，也不存在软件过流、过压和AWD保护。
 * PR控制器没有在本文件初始化或调用，后续恢复真实双闭环时应新增独立模式，不能在这里
 * 临时混入PR_Calc()或实测反馈。
 */
#define PFC_CONTROL_PI_DIVIDER       10U
#define PFC_CONTROL_TWO_PI           6.28318530718f
#define PFC_CONTROL_SQRT_TWO         1.41421356237f
#define PFC_CONTROL_RAMP_EPSILON_A   0.00001f

static const PFC_Params *control_params = 0; /* 初始化写，10 kHz ISR只读；对象必须常驻。 */
static PI_t voltage_pi;                     /* 1 kHz虚拟母线PI对象；主循环不得直接读写。 */
static volatile PFC_ControlTelemetry control_telemetry; /* ISR写，Getter在临界区复制。 */

static float control_phase_rad = 0.0f;      /* rad，VAC正向过零定义为0，只由10 kHz ISR写。 */
static float control_phase_step_rad = 0.0f; /* rad/sample，由实测VAC周期更新。 */
static float control_applied_current_rms = 0.0f; /* A RMS，软启动后的实际开环指令。 */
static uint32_t control_last_cross_sequence = 0U; /* 最近已处理的VAC过零序号。 */
static uint16_t control_saturation_count = 0U;    /* 连续限幅样本数，10 kHz ISR写。 */
static uint8_t control_outer_divider = 0U;        /* 10分频计数，10 kHz到1 kHz。 */
static uint8_t control_initialized = 0U;          /* 1表示参数、PI和SPWM初始化全部成功。 */

/** @brief 判断单精度数值既不是NaN也不是正负无穷。 */
static uint8_t PFC_ControlFloatValid(float value)
{
    return (uint8_t)((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

/** @brief 记录控制故障并返回HAL_ERROR；实际HRTIM关断由应用层统一入口完成。 */
static HAL_StatusTypeDef PFC_ControlFail(uint32_t fault_bits)
{
    control_telemetry.fault_bits |= fault_bits;
    control_telemetry.healthy = 0U;
    return HAL_ERROR;
}

HAL_StatusTypeDef PFC_Control_Init(const PFC_Params *params)
{
    float pi_output_max; /* A RMS，由允许峰值电流除以sqrt(2)得到。 */

    control_params = params;
    control_initialized = 0U;
    (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));

    if ((params == 0) || (PFC_Params_PowerAllowed(params) == 0U) ||
        (params->control_frequency <= 0.0f) ||
        (params->voltage_loop_frequency_hz <= 0.0f) ||
        (fabsf(params->control_frequency -
               PFC_CONTROL_PI_DIVIDER * params->voltage_loop_frequency_hz) > 0.5f))
    {
        return PFC_ControlFail(PFC_FAULT_PARAM | PFC_FAULT_CONTROL);
    }

    pi_output_max = params->current_command_target / PFC_CONTROL_SQRT_TWO;
    PI_Init(&voltage_pi, params->voltage_pi_kp, params->voltage_pi_ki,
            params->voltage_loop_frequency_hz, 0.0f, pi_output_max);

    if ((voltage_pi.initialized == 0U) ||
        (params->virtual_current_preset_rms >= pi_output_max) ||
        (SPWM_Init(params) != HAL_OK))
    {
        return PFC_ControlFail(PFC_FAULT_PARAM | PFC_FAULT_CONTROL);
    }

    control_initialized = 1U;
    PFC_Control_Reset();
    return HAL_OK;
}

void PFC_Control_Reset(void)
{
    PI_Reset(&voltage_pi);
    control_phase_rad = 0.0f;
    control_phase_step_rad = 0.0f;
    control_applied_current_rms = 0.0f;
    control_last_cross_sequence = 0U;
    control_saturation_count = 0U;
    control_outer_divider = 0U;
    SPWM_Reset();

    (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));
    control_telemetry.mode = PFC_CONTROL_IDLE;
    control_telemetry.pr_enabled = 0U;
    if (control_params != 0)
    {
        /* READY阶段也显示已加载的虚拟参考/反馈，避免尚未启动时误显示为0。 */
        control_telemetry.virtual_vbus_reference = control_params->vbus_target;
        control_telemetry.virtual_vbus_feedback = control_params->virtual_vbus_feedback;
    }
    control_telemetry.healthy = control_initialized;
}

HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_Measurement *measurement)
{
    uint32_t primask; /* 保存调用前中断屏蔽状态，退出临界区时原样恢复。 */
    HAL_StatusTypeDef status = HAL_OK;

    if ((control_initialized == 0U) || (measurement == 0))
    {
        return HAL_ERROR;
    }
    if (mode == PFC_CONTROL_IDLE)
    {
        PFC_Control_Reset();
        return HAL_OK;
    }
    if (((control_params->adc_enabled != 0U) &&
         ((measurement->valid == 0U) || (measurement->vac_locked == 0U))) ||
        ((mode != PFC_CONTROL_VIRTUAL_RAMP) &&
         (mode != PFC_CONTROL_VIRTUAL_RUN)))
    {
        return HAL_ERROR;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (mode == PFC_CONTROL_VIRTUAL_RAMP)
    {
        PI_Reset(&voltage_pi);
        if (PI_PresetOutput(&voltage_pi, control_params->vbus_target,
                            control_params->virtual_vbus_feedback,
                            control_params->virtual_current_preset_rms) == 0U)
        {
            status = HAL_ERROR;
        }
        else
        {
            control_phase_rad = 0.0f;
            control_phase_step_rad = PFC_CONTROL_TWO_PI *
                                     ((control_params->adc_enabled != 0U) ?
                                      measurement->vac_frequency_hz :
                                      control_params->grid_frequency_hz) /
                                     control_params->control_frequency;
            control_last_cross_sequence = measurement->vac_zero_cross_sequence;
            control_applied_current_rms = 0.0f;
            control_saturation_count = 0U;
            control_outer_divider = 0U;
            (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));
            control_telemetry.mode = mode;
            control_telemetry.virtual_vbus_reference = control_params->vbus_target;
            control_telemetry.virtual_vbus_feedback = control_params->virtual_vbus_feedback;
            control_telemetry.pi_output_current_rms = voltage_pi.output;
            control_telemetry.pi_error = voltage_pi.error;
            control_telemetry.pr_enabled = 0U;
            /*
             * PD0可能在工频周期任意位置按下。此处不能把“当前时刻”误当成过零点，
             * 因而先保持中性Compare，等待ADC测量发布下一次真实正向过零后再开始斜坡。
             */
            control_telemetry.phase_synchronized =
                (control_params->adc_enabled == 0U) ? 1U : 0U;
            control_telemetry.healthy = 1U;
        }
    }
    else
    {
        control_telemetry.mode = PFC_CONTROL_VIRTUAL_RUN;
        control_telemetry.ramp_complete = 1U;
    }
    if (primask == 0U)
    {
        __enable_irq();
    }
    return status;
}

HAL_StatusTypeDef PFC_Control_Step10k(const PFC_Measurement *measurement)
{
    float pi_output_rms;       /* A RMS，本周期使用的PI目标输出。 */
    float ramp_step;           /* A RMS/sample，10 kHz软启动单步增量。 */
    float sine_value;          /* 当前同步相位的正弦值。 */
    float cosine_value;        /* 当前同步相位的余弦值。 */
    float angular_frequency;   /* rad/s，由当前锁定VAC频率计算。 */
    float synthetic_vac;       /* V，理想额定VAC瞬时参考，不使用ADC幅值。 */
    float current_reference;   /* A，理想同相输入电流瞬时参考。 */
    float current_derivative;  /* A/s，理想电流参考解析导数。 */
    float bridge_voltage;      /* V，全桥电压前馈指令。 */
    float modulation;          /* 归一化差分调制量。 */
    uint8_t saturated;         /* 1表示PI或SPWM当前触及限幅。 */

    if ((control_initialized == 0U) || (control_params == 0) ||
        (measurement == 0) ||
        ((control_params->adc_enabled != 0U) &&
         ((measurement->valid == 0U) || (measurement->vac_locked == 0U))) ||
        ((control_telemetry.mode != PFC_CONTROL_VIRTUAL_RAMP) &&
         (control_telemetry.mode != PFC_CONTROL_VIRTUAL_RUN)))
    {
        return PFC_ControlFail(PFC_FAULT_CONTROL | PFC_FAULT_VAC_LOST);
    }

    if ((control_params->adc_enabled != 0U) &&
        (measurement->vac_zero_cross_sequence != control_last_cross_sequence))
    {
        control_last_cross_sequence = measurement->vac_zero_cross_sequence;
        control_phase_rad = 0.0f;
        control_phase_step_rad = PFC_CONTROL_TWO_PI *
                                 measurement->vac_frequency_hz /
                                 control_params->control_frequency;
        control_telemetry.phase_synchronized = 1U;
    }

    if (control_telemetry.phase_synchronized == 0U)
    {
        /* 最多等待一个50 Hz周期；等待期间PWM引脚虽已开放，但两桥臂保持50%中性指令。 */
        if (SPWM_ForceNeutral() != HAL_OK)
        {
            return PFC_ControlFail(PFC_FAULT_HRTIM | PFC_FAULT_MODULATION);
        }
        control_telemetry.measured_vbus_monitor = measurement->vbus;
        control_telemetry.fast_heartbeat++;
        control_telemetry.healthy = 1U;
        return HAL_OK;
    }

    if (++control_outer_divider >= PFC_CONTROL_PI_DIVIDER)
    {
        control_outer_divider = 0U;
        pi_output_rms = PI_Calc(&voltage_pi, control_params->vbus_target,
                                control_params->virtual_vbus_feedback);
        if (PFC_ControlFloatValid(pi_output_rms) == 0U)
        {
            return PFC_ControlFail(PFC_FAULT_CONTROL);
        }
        control_telemetry.outer_heartbeat++;
    }
    else
    {
        pi_output_rms = voltage_pi.output;
    }

    ramp_step = control_params->virtual_current_slew_a_s /
                control_params->control_frequency;
    if (control_applied_current_rms < pi_output_rms)
    {
        control_applied_current_rms += ramp_step;
        if (control_applied_current_rms > pi_output_rms)
        {
            control_applied_current_rms = pi_output_rms;
        }
    }
    else if (control_applied_current_rms > pi_output_rms)
    {
        control_applied_current_rms -= ramp_step;
        if (control_applied_current_rms < pi_output_rms)
        {
            control_applied_current_rms = pi_output_rms;
        }
    }

    sine_value = sinf(control_phase_rad);
    cosine_value = cosf(control_phase_rad);
    angular_frequency = PFC_CONTROL_TWO_PI *
                        ((control_params->adc_enabled != 0U) ?
                         measurement->vac_frequency_hz :
                         control_params->grid_frequency_hz);
    synthetic_vac = PFC_CONTROL_SQRT_TWO * control_params->vac_nominal_rms * sine_value;
    current_reference = PFC_CONTROL_SQRT_TWO * control_applied_current_rms * sine_value;
    current_derivative = PFC_CONTROL_SQRT_TWO * control_applied_current_rms *
                         angular_frequency * cosine_value;
    bridge_voltage = synthetic_vac - control_params->input_inductance * current_derivative;
    modulation = (float)control_params->bridge_polarity * bridge_voltage /
                 control_params->vbus_target;

    if ((PFC_ControlFloatValid(synthetic_vac) == 0U) ||
        (PFC_ControlFloatValid(current_reference) == 0U) ||
        (PFC_ControlFloatValid(bridge_voltage) == 0U) ||
        (PFC_ControlFloatValid(modulation) == 0U) ||
        (SPWM_ApplyModulation(modulation) != HAL_OK))
    {
        return PFC_ControlFail(PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
    }

    saturated = (uint8_t)((voltage_pi.limited != 0U) ||
                          (SPWM_IsLimited() != 0U));
    if (saturated != 0U)
    {
        if (control_saturation_count < 0xFFFFU)
        {
            control_saturation_count++;
        }
        if (control_saturation_count >= control_params->saturation_trip_samples)
        {
            return PFC_ControlFail(PFC_FAULT_CONTROL_SATURATION);
        }
    }
    else
    {
        control_saturation_count = 0U;
    }

    control_telemetry.virtual_vbus_reference = control_params->vbus_target;
    control_telemetry.virtual_vbus_feedback = control_params->virtual_vbus_feedback;
    control_telemetry.measured_vbus_monitor =
        (control_params->adc_enabled != 0U) ? measurement->vbus : 0.0f;
    control_telemetry.pi_output_current_rms = pi_output_rms;
    control_telemetry.applied_current_rms = control_applied_current_rms;
    control_telemetry.synthetic_vac_reference = synthetic_vac;
    control_telemetry.current_reference = current_reference;
    control_telemetry.bridge_voltage_reference = bridge_voltage;
    control_telemetry.modulation = SPWM_GetModulation();
    control_telemetry.pi_error = voltage_pi.error;
    control_telemetry.saturation_count = control_saturation_count;
    control_telemetry.pi_limited = voltage_pi.limited;
    control_telemetry.modulation_limited = SPWM_IsLimited();
    control_telemetry.ramp_complete =
        (uint8_t)(fabsf(control_applied_current_rms - pi_output_rms) <=
                  PFC_CONTROL_RAMP_EPSILON_A);
    control_telemetry.pr_enabled = 0U;
    control_telemetry.fast_heartbeat++;
    control_telemetry.healthy = 1U;

    control_phase_rad += control_phase_step_rad;
    if (control_phase_rad >= PFC_CONTROL_TWO_PI)
    {
        control_phase_rad -= PFC_CONTROL_TWO_PI;
    }
    return HAL_OK;
}

void PFC_Control_GetTelemetry(PFC_ControlTelemetry *telemetry)
{
    uint32_t primask;

    if (telemetry == 0)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    (void)memcpy(telemetry, (const void *)&control_telemetry, sizeof(*telemetry));
    if (primask == 0U)
    {
        __enable_irq();
    }
}

uint8_t PFC_Control_IsHealthy(void)
{
    return control_telemetry.healthy;
}
