#include "pfc_control.h"

#include "PI.h"
#include "SPWM.h"
#include <float.h>
#include <math.h>
#include <string.h>

/*
 * 本模块负责“VBUS电压PI外环 + VAC同步电感模型内层”。
 * IPFC在当前工程中只有原始码监视，不进入误差、控制量或过流判据。
 * 数据流：ADC1完整DMA快照 -> 100 Hz陷波 -> 1 kHz PI -> 虚拟电流RMS指令
 * -> SPWM VAC同步与L*di/dt前馈 -> HRTIM Compare预装载。
 * 高频路径只允许执行确定时间的浮点计算和HRTIM Compare写入，禁止阻塞外设。
 */
#define PFC_CONTROL_PI_DIVIDER   10U
#define PFC_CONTROL_TWO_PI       6.28318530718f
#define PFC_CONTROL_SQRT_TWO     1.41421356237f
#define PFC_CONTROL_VBUS_DIV_MIN 2.0f

typedef struct
{
    float input_1;
    float input_2;
    float output_1;
    float output_2;
} PFC_NotchState;

typedef struct
{
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} PFC_NotchCoefficients;

static const PFC_Params *control_params = 0;
static PI_t voltage_pi;
static PFC_NotchState vbus_notch;
static PFC_NotchCoefficients notch_coefficients;
static volatile PFC_ControlTelemetry control_telemetry;
volatile PFC_ControlFailureSnapshot PFC_ControlDiagnostic;

static float virtual_current_rms_command = 0.0f;
static float control_vbus_reference = 0.0f;
static uint16_t outer_divider = 0U;
static uint16_t saturation_count = 0U;
static uint16_t vbus_regulation_error_ms = 0U;
static uint16_t period_sample_count = 0U;
static uint16_t period_limited_count = 0U;
static uint8_t period_bad_cycles = 0U;
static uint32_t last_period_cross_sequence = 0U;
static uint8_t control_initialized = 0U;

static uint8_t PFC_ControlFloatValid(float value)
{
    return (uint8_t)((value == value) && (value <= FLT_MAX) &&
                     (value >= -FLT_MAX));
}

/**
 * @brief  把模块内部遥测整理成公共PI-only输出结构。
 * @param  output 接收本周期结果的地址；允许传NULL表示调用者只需要内部遥测。
 * @note   本函数只复制标量，不操作HRTIM，也不进入临界区；调用者位于同一个10 kHz ISR。
 */
static void PFC_ControlFillOutput(PFC_ControlOutput *output)
{
    if (output == 0)
    {
        return;
    }
    output->vbus_reference = control_telemetry.vbus_reference;
    output->virtual_current_rms = control_telemetry.current_rms_command;
    output->virtual_current_instantaneous = control_telemetry.current_reference;
    output->inductor_voltage_reference =
        control_telemetry.inductor_voltage_reference;
    output->bridge_voltage_reference =
        control_telemetry.bridge_voltage_reference;
    output->modulation = control_telemetry.modulation;
    output->pi_limited = control_telemetry.pi_limited;
    output->modulation_limited = control_telemetry.modulation_limited;
    output->fault_bits = control_telemetry.fault_bits;
}

static void PFC_ControlNotchPreset(float value)
{
    vbus_notch.input_1 = value;
    vbus_notch.input_2 = value;
    vbus_notch.output_1 = value;
    vbus_notch.output_2 = value;
}

static float PFC_ControlNotchStep(float input)
{
    float output = notch_coefficients.b0 * input +
                   notch_coefficients.b1 * vbus_notch.input_1 +
                   notch_coefficients.b2 * vbus_notch.input_2 -
                   notch_coefficients.a1 * vbus_notch.output_1 -
                   notch_coefficients.a2 * vbus_notch.output_2;

    vbus_notch.input_2 = vbus_notch.input_1;
    vbus_notch.input_1 = input;
    vbus_notch.output_2 = vbus_notch.output_1;
    vbus_notch.output_1 = output;
    return output;
}

static HAL_StatusTypeDef PFC_ControlFail(PFC_ControlFailureReason reason,
                                         uint32_t fault_bits)
{
    if (control_telemetry.failure_reason == PFC_CONTROL_FAIL_NONE)
    {
        control_telemetry.failure_reason = reason;
        PFC_ControlDiagnostic.reason = reason;
        PFC_ControlDiagnostic.fault_bits = fault_bits;
        PFC_ControlDiagnostic.mode = control_telemetry.mode;
        PFC_ControlDiagnostic.current_rms_command_a =
            virtual_current_rms_command;
        PFC_ControlDiagnostic.current_reference_a =
            control_telemetry.current_reference;
        PFC_ControlDiagnostic.current_error_a = 0.0f;
        PFC_ControlDiagnostic.pr_raw_output_v = 0.0f;
        PFC_ControlDiagnostic.pr_output_v = 0.0f;
        PFC_ControlDiagnostic.modulation = control_telemetry.modulation;
    }
    else
    {
        PFC_ControlDiagnostic.fault_bits |= fault_bits;
    }
    control_telemetry.fault_bits |= fault_bits;
    control_telemetry.healthy = 0U;
    return HAL_ERROR;
}

HAL_StatusTypeDef PFC_Control_Init(const PFC_Params *params)
{
    float pi_output_max;
    float notch_omega;
    float tustin_gain;
    float tustin_gain_squared;
    float notch_omega_squared;
    float notch_denominator;

    control_params = params;
    control_initialized = 0U;
    (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));
    if ((params == 0) || (params->control_frequency <= 0.0f) ||
        (params->grid_frequency_hz <= 0.0f) ||
        (params->voltage_loop_frequency_hz <= 4.0f * params->grid_frequency_hz) ||
        (fabsf(params->control_frequency -
               PFC_CONTROL_PI_DIVIDER * params->voltage_loop_frequency_hz) > 0.5f))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_INIT_PARAM,
                               PFC_FAULT_PARAM | PFC_FAULT_CONTROL);
    }

    /* Params字段保存的是虚拟峰值上限，PI输出单位为A RMS。 */
    pi_output_max = params->current_command_target / PFC_CONTROL_SQRT_TWO;
    PI_Init(&voltage_pi, params->voltage_pi_kp, params->voltage_pi_ki,
            params->voltage_loop_frequency_hz, 0.0f, pi_output_max);

    notch_omega = PFC_CONTROL_TWO_PI * 2.0f * params->grid_frequency_hz;
    tustin_gain = notch_omega /
                  tanf(notch_omega / (2.0f * params->voltage_loop_frequency_hz));
    tustin_gain_squared = tustin_gain * tustin_gain;
    notch_omega_squared = notch_omega * notch_omega;
    notch_denominator = tustin_gain_squared +
                        params->vbus_notch_damping_rad_s * tustin_gain +
                        notch_omega_squared;
    notch_coefficients.b0 = (tustin_gain_squared + notch_omega_squared) /
                            notch_denominator;
    notch_coefficients.b1 = 2.0f * (notch_omega_squared - tustin_gain_squared) /
                            notch_denominator;
    notch_coefficients.b2 = notch_coefficients.b0;
    notch_coefficients.a1 = notch_coefficients.b1;
    notch_coefficients.a2 = (tustin_gain_squared -
                             params->vbus_notch_damping_rad_s * tustin_gain +
                             notch_omega_squared) / notch_denominator;

    if ((voltage_pi.initialized == 0U) ||
        (PFC_ControlFloatValid(notch_coefficients.b0) == 0U) ||
        (PFC_ControlFloatValid(notch_coefficients.b1) == 0U) ||
        (PFC_ControlFloatValid(notch_coefficients.a2) == 0U) ||
        (SPWM_Init(params) != HAL_OK))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_INIT_CONTROLLER,
                               PFC_FAULT_PARAM | PFC_FAULT_CONTROL);
    }

    control_initialized = 1U;
    PFC_Control_Reset();
    return HAL_OK;
}

void PFC_Control_Reset(void)
{
    PI_Reset(&voltage_pi);
    (void)memset(&vbus_notch, 0, sizeof(vbus_notch));
    virtual_current_rms_command = 0.0f;
    control_vbus_reference = 0.0f;
    outer_divider = 0U;
    saturation_count = 0U;
    vbus_regulation_error_ms = 0U;
    period_sample_count = 0U;
    period_limited_count = 0U;
    period_bad_cycles = 0U;
    last_period_cross_sequence = 0U;
    SPWM_Reset();
    (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));
    (void)memset((void *)&PFC_ControlDiagnostic, 0,
                 sizeof(PFC_ControlDiagnostic));
    control_telemetry.mode = PFC_CONTROL_IDLE;
    control_telemetry.healthy = control_initialized;
    control_telemetry.unprotected_heavy_override =
        PFC_UnprotectedHeavyOverride;
}

void PFC_Control_ResetPreserveFault(void)
{
    PFC_ControlTelemetry failed_telemetry = control_telemetry;
    PFC_ControlFailureSnapshot failed_diagnostic = PFC_ControlDiagnostic;
    PFC_Control_Reset();
    control_telemetry = failed_telemetry;
    PFC_ControlDiagnostic = failed_diagnostic;
    control_telemetry.healthy = 0U;
}

HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_ControlInput *input)
{
    uint32_t primask;
    float vbus;

    if ((control_initialized == 0U) || (control_params == 0) ||
        (input == 0))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_ENTER_PRECONDITION,
                               PFC_FAULT_CONTROL);
    }

    vbus = input->vbus_v;
    if ((mode != PFC_CONTROL_IDLE) &&
        ((PFC_ControlFloatValid(vbus) == 0U) ||
         (vbus <= PFC_CONTROL_VBUS_DIV_MIN)))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_VBUS_DIVISION,
                               PFC_FAULT_CONTROL);
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (mode == PFC_CONTROL_IDLE)
    {
        PFC_Control_Reset();
    }
    else if ((mode == PFC_CONTROL_PRIME) ||
             (mode == PFC_CONTROL_POWER_RAMP))
    {
        PI_Reset(&voltage_pi);
        PFC_ControlNotchPreset(vbus);
        virtual_current_rms_command = 0.0f;
        control_vbus_reference = vbus;
        outer_divider = 0U;
        control_telemetry.mode = mode;
        control_telemetry.vbus_reference = vbus;
        control_telemetry.vbus_filtered = vbus;
        control_telemetry.current_loop_qualified = 0U;
        control_telemetry.vbus_reference_reached = 0U;
        control_telemetry.failure_reason = PFC_CONTROL_FAIL_NONE;
        control_telemetry.fault_bits = 0U;
        control_telemetry.healthy = 1U;
        control_telemetry.prime_waiting = (mode == PFC_CONTROL_PRIME) ? 1U : 0U;
        control_telemetry.prime_start_cross_sequence =
            input->vac_zero_cross_sequence;
        PFC_ControlDiagnostic.prime_start_cross_sequence =
            input->vac_zero_cross_sequence;
        if (mode == PFC_CONTROL_POWER_RAMP)
        {
            period_sample_count = 0U;
            period_limited_count = 0U;
            period_bad_cycles = 0U;
            last_period_cross_sequence = input->vac_zero_cross_sequence;
        }
    }
    else if (mode == PFC_CONTROL_VBUS_RAMP)
    {
        PFC_ControlNotchPreset(vbus);
        control_vbus_reference = vbus;
        outer_divider = 0U;
        if (PI_PresetOutput(&voltage_pi, vbus, vbus,
                            virtual_current_rms_command) == 0U)
        {
            if (primask == 0U) { __enable_irq(); }
            return PFC_ControlFail(PFC_CONTROL_FAIL_PI_PRESET,
                                   PFC_FAULT_CONTROL);
        }
        control_telemetry.mode = mode;
        control_telemetry.healthy = 1U;
    }
    else if (mode == PFC_CONTROL_VBUS_RUN)
    {
        control_vbus_reference = control_params->vbus_target;
        control_telemetry.mode = mode;
        control_telemetry.vbus_reference_reached = 1U;
        control_telemetry.healthy = 1U;
    }
    else
    {
        if (primask == 0U) { __enable_irq(); }
        return PFC_ControlFail(PFC_CONTROL_FAIL_ENTER_MODE,
                               PFC_FAULT_CONTROL);
    }
    if (primask == 0U) { __enable_irq(); }
    return HAL_OK;
}

HAL_StatusTypeDef PFC_Control_Step10k(const PFC_ControlInput *input,
                                      PFC_ControlOutput *output)
{
    PFC_Measurement model_measurement = {0};
    const PFC_Measurement *measurement = &model_measurement;
    float vbus_filtered;
    float virtual_current_target;
    float current_instantaneous;
    float inductor_voltage;
    float bridge_voltage;

    if ((control_initialized == 0U) || (control_params == 0) ||
        (input == 0))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_MEASUREMENT,
                               PFC_FAULT_CONTROL);
    }

    /* SPWM底层仍复用测量快照类型，但只填PI-only模型真正允许读取的VAC/VBUS字段。 */
    model_measurement.vac = input->vac_v;
    model_measurement.vbus = input->vbus_v;
    model_measurement.vac_frequency_hz = input->vac_frequency_hz;
    model_measurement.vac_zero_cross_sequence = input->vac_zero_cross_sequence;
    model_measurement.vac_locked = input->vac_locked;
    PFC_ControlDiagnostic.vbus_v = input->vbus_v;
    PFC_ControlDiagnostic.vac_v = input->vac_v;
    PFC_ControlDiagnostic.vbus_used_v = input->vbus_v;
    model_measurement.valid = 1U;

    if (control_telemetry.mode == PFC_CONTROL_PRIME)
    {
        /* PRIME只同步相位并预装载电网前馈，不允许能量指令进入。 */
        if (SPWM_ModelStep(measurement, 0.0f, &current_instantaneous,
                           &inductor_voltage, &bridge_voltage) != HAL_OK)
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_MODULATION,
                                   PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
        }
        control_telemetry.prime_waiting =
            (uint8_t)(((measurement->vac_locked != 0U) &&
                       (measurement->vac_zero_cross_sequence !=
                        control_telemetry.prime_start_cross_sequence)) ? 0U : 1U);
        control_telemetry.fast_heartbeat++;
        control_telemetry.modulation = SPWM_GetModulation();
        control_telemetry.inductor_voltage_reference = inductor_voltage;
        control_telemetry.bridge_voltage_reference = bridge_voltage;
        control_telemetry.healthy = 1U;
        PFC_ControlFillOutput(output);
        return HAL_OK;
    }

    if ((control_telemetry.mode != PFC_CONTROL_POWER_RAMP) &&
        (control_telemetry.mode != PFC_CONTROL_VBUS_RAMP) &&
        (control_telemetry.mode != PFC_CONTROL_VBUS_RUN))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_MODE, PFC_FAULT_CONTROL);
    }

    if (control_telemetry.mode == PFC_CONTROL_POWER_RAMP)
    {
        virtual_current_rms_command +=
            control_params->current_probe_slew_a_s /
            control_params->control_frequency;
        virtual_current_target = control_params->current_probe_rms;
        if (virtual_current_rms_command > virtual_current_target)
        {
            virtual_current_rms_command = virtual_current_target;
        }
    }
    else if (++outer_divider >= PFC_CONTROL_PI_DIVIDER)
    {
        outer_divider = 0U;
        vbus_filtered = PFC_ControlNotchStep(measurement->vbus);
        if (PFC_ControlFloatValid(vbus_filtered) == 0U)
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_NOTCH_NONFINITE,
                                   PFC_FAULT_CONTROL);
        }
        if (control_telemetry.mode == PFC_CONTROL_VBUS_RAMP)
        {
            control_vbus_reference +=
                control_params->vbus_reference_slew_v_s /
                control_params->voltage_loop_frequency_hz;
            if (control_vbus_reference >= control_params->vbus_target)
            {
                control_vbus_reference = control_params->vbus_target;
                control_telemetry.vbus_reference_reached = 1U;
            }
        }
        virtual_current_rms_command = PI_Calc(&voltage_pi,
                                              control_vbus_reference,
                                              vbus_filtered);
        if ((PFC_ControlFloatValid(virtual_current_rms_command) == 0U) ||
            (virtual_current_rms_command < 0.0f))
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_PI_NONFINITE,
                                   PFC_FAULT_CONTROL);
        }
        control_telemetry.vbus_filtered = vbus_filtered;
        control_telemetry.outer_heartbeat++;
        control_telemetry.pi_limited = voltage_pi.limited;
        if ((control_telemetry.mode == PFC_CONTROL_VBUS_RUN) &&
            (voltage_pi.limited != 0U) &&
            (fabsf(vbus_filtered - control_params->vbus_target) >
             control_params->vbus_target_tolerance))
        {
            if (vbus_regulation_error_ms <
                control_params->vbus_regulation_timeout_ms)
            {
                vbus_regulation_error_ms++;
            }
            control_telemetry.vbus_regulation_error_ms =
                vbus_regulation_error_ms;
            if (vbus_regulation_error_ms >=
                control_params->vbus_regulation_timeout_ms)
            {
                return PFC_ControlFail(PFC_CONTROL_FAIL_VBUS_REGULATION,
                                       PFC_FAULT_VBUS_REGULATION);
            }
        }
        else
        {
            vbus_regulation_error_ms = 0U;
            control_telemetry.vbus_regulation_error_ms = 0U;
        }
    }

    if (SPWM_ModelStep(measurement, virtual_current_rms_command,
                       &current_instantaneous, &inductor_voltage,
                       &bridge_voltage) != HAL_OK)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_SPWM_WRITE,
                               PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
    }

    control_telemetry.vbus_reference = control_vbus_reference;
    control_telemetry.current_rms_command = virtual_current_rms_command;
    control_telemetry.current_reference = current_instantaneous;
    control_telemetry.current_error = 0.0f;
    control_telemetry.current_error_rms = 0.0f;
    control_telemetry.pr_output_v = 0.0f;
    control_telemetry.inductor_voltage_reference = inductor_voltage;
    control_telemetry.bridge_voltage_reference = bridge_voltage;
    control_telemetry.phase_rad = SPWM_GetPhase();
    control_telemetry.modulation = SPWM_GetModulation();
    control_telemetry.modulation_limited = SPWM_IsLimited();
    control_telemetry.pr_limited = 0U;
    control_telemetry.fast_heartbeat++;
    control_telemetry.healthy = 1U;
    if (SPWM_IsLimited() != 0U)
    {
        if (saturation_count < 0xFFFFU) { saturation_count++; }
        if (saturation_count >= control_params->saturation_trip_samples)
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_CONTINUOUS_SATURATION,
                                   PFC_FAULT_CONTROL_SATURATION);
        }
    }
    else
    {
        saturation_count = 0U;
    }
    control_telemetry.saturation_count = saturation_count;

    /*
     * 以VAC正向过零序列结算上一个完整工频周期的调制度削顶比例。
     * 连续限幅保护负责捕获长时间贴边，本统计负责捕获每周期重复出现的片段式削顶。
     */
    if (measurement->vac_zero_cross_sequence != last_period_cross_sequence)
    {
        if (period_sample_count != 0U)
        {
            control_telemetry.period_saturation_ratio =
                (float)period_limited_count / (float)period_sample_count;
            if (control_telemetry.period_saturation_ratio >
                control_params->period_saturation_ratio)
            {
                if (period_bad_cycles < 0xFFU) { period_bad_cycles++; }
            }
            else
            {
                period_bad_cycles = 0U;
            }
            control_telemetry.period_saturation_bad_cycles = period_bad_cycles;
            if (period_bad_cycles >= control_params->period_saturation_cycles)
            {
                return PFC_ControlFail(PFC_CONTROL_FAIL_PERIOD_SATURATION,
                                       PFC_FAULT_CONTROL_SATURATION);
            }
        }
        last_period_cross_sequence = measurement->vac_zero_cross_sequence;
        period_sample_count = 0U;
        period_limited_count = 0U;
    }
    if (period_sample_count < 0xFFFFU) { period_sample_count++; }
    if ((SPWM_IsLimited() != 0U) && (period_limited_count < 0xFFFFU))
    {
        period_limited_count++;
    }
    control_telemetry.period_saturation_samples = period_limited_count;
    PFC_ControlDiagnostic.current_rms_command_a = virtual_current_rms_command;
    PFC_ControlDiagnostic.current_reference_a = current_instantaneous;
    PFC_ControlDiagnostic.bridge_reference_v = bridge_voltage;
    PFC_ControlDiagnostic.pr_output_v = inductor_voltage;
    PFC_ControlDiagnostic.modulation = control_telemetry.modulation;
    PFC_ControlFillOutput(output);
    return HAL_OK;
}

void PFC_Control_GetTelemetry(PFC_ControlTelemetry *telemetry)
{
    uint32_t primask;
    if (telemetry == 0) { return; }
    primask = __get_PRIMASK();
    __disable_irq();
    *telemetry = control_telemetry;
    if (primask == 0U) { __enable_irq(); }
}

uint8_t PFC_Control_IsHealthy(void)
{
    return control_telemetry.healthy;
}

void PFC_Control_ConfirmOutputsEnabled(const PFC_ControlInput *input)
{
    if (input == 0) { return; }
    control_telemetry.output_enable_cross_sequence =
        input->vac_zero_cross_sequence;
    if (control_telemetry.output_enable_count < 0xFFU)
    {
        control_telemetry.output_enable_count++;
    }
    control_telemetry.prime_waiting = 0U;
    PFC_ControlDiagnostic.output_enable_cross_sequence =
        input->vac_zero_cross_sequence;
    PFC_ControlDiagnostic.output_enable_count =
        control_telemetry.output_enable_count;
}
