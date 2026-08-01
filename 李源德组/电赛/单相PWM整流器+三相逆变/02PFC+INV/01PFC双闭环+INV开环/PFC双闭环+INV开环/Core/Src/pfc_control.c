#include "pfc_control.h"

#include "PI.h"
#include "pr.h"
#include "SPWM.h"
#include <float.h>
#include <math.h>
#include <string.h>

/*
 * 本模块负责5 V档的“1 kHz母线PI外环 + 10 kHz电流PR内环”，不负责ADC换算、
 * PD0启停、HRTIM输出许可或故障恢复。数据流如下：
 *
 * VBUS -> 100 Hz陷波 -> PI -> 电流RMS指令
 * VAC/额定RMS -> 瞬时电流参考 -> PR -> VAC前馈 -> 调制量 -> SPWM/HRTIM
 *
 * ADC1完整DMA ISR独占写运行状态；主循环和状态机只能通过遥测快照接口读取。
 */
#define PFC_CONTROL_PI_DIVIDER       10U
/* 工频、陷波器和调制计算使用的单精度常量。 */
#define PFC_CONTROL_TWO_PI           6.28318530718f
#define PFC_CONTROL_SQRT_TWO         1.41421356237f
#define PFC_CONTROL_VBUS_DIV_MIN     2.0f
/* 36 V档在|VAC|不超过额定RMS的8%时投入，避免任意相位开放全桥造成单半周浪涌。 */
#define PFC_CONTROL_START_VAC_MIN_V  0.5f
#define PFC_CONTROL_START_VAC_RATIO  0.08f

/** @brief 100 Hz二阶IIR陷波器历史，只在1 kHz外环分支更新。 */
typedef struct
{
    float input_1;
    float input_2;
    float output_1;
    float output_2;
} PFC_NotchState;

/** @brief Tustin预畸变后的二阶陷波器归一化差分系数。 */
typedef struct
{
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} PFC_NotchCoefficients;

static const PFC_Params *control_params = 0; /* 初始化后只读，参数对象必须常驻。 */
static PI_t voltage_pi;                     /* 1 kHz母线电压外环对象。 */
static PR_t current_pr;                     /* 10 kHz输入电流内环对象。 */
static PFC_NotchState vbus_notch;           /* 1 kHz母线100 Hz纹波陷波状态。 */
static PFC_NotchCoefficients notch_coefficients; /* 初始化计算，运行期只读。 */
static volatile PFC_ControlTelemetry control_telemetry; /* ISR写、快照接口读。 */

/* 以下运行量只由10 kHz ISR写；EnterMode在关中断临界区内修改。 */
static float control_current_rms_command = 0.0f; /* A RMS，当前内环幅值命令。 */
static float control_vbus_reference = 0.0f;      /* V，当前外环软启动参考。 */
static float control_last_valid_vbus = 0.0f;     /* V，放宽模式除法回退值；仅10 kHz路径更新。 */
static float error_square_sum = 0.0f;            /* A^2，工频周期误差平方和。 */
static uint32_t error_sample_count = 0U;          /* 当前误差RMS窗口样本数。 */
static uint32_t error_window_samples = 200U;      /* 一工频周期对应的10 kHz样本数。 */
static uint32_t current_mode_samples = 0U;        /* 电流探测阶段累计10 kHz样本数。 */
static uint16_t outer_divider = 0U;               /* 10 kHz到1 kHz的分频计数。 */
static uint16_t saturation_count = 0U;            /* 连续限幅样本数。 */
static uint8_t control_initialized = 0U;          /* PI、PR、SPWM均初始化成功。 */

/** @brief 判断单精度值是否有限，防止NaN或无穷进入IIR和HRTIM。 */
static uint8_t PFC_ControlFloatValid(float value)
{
    return (uint8_t)((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

/** @brief 将陷波器历史预置为稳态直流值，避免切入外环时产生滤波瞬态。 */
static void PFC_ControlNotchPreset(float value)
{
    vbus_notch.input_1 = value;
    vbus_notch.input_2 = value;
    vbus_notch.output_1 = value;
    vbus_notch.output_2 = value;
}

/** @brief 以1 kHz执行一次100 Hz陷波，返回当前滤波母线电压。 */
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

/** @brief 把控制故障写入遥测；实际故障锁存和HRTIM关断由应用层执行。 */
static HAL_StatusTypeDef PFC_ControlFail(uint32_t fault_bits)
{
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
        (params->voltage_loop_frequency_hz <=
         (4.0f * params->grid_frequency_hz)) ||
        (fabsf(params->control_frequency -
               PFC_CONTROL_PI_DIVIDER * params->voltage_loop_frequency_hz) > 0.5f))
    {
        return PFC_ControlFail(PFC_FAULT_PARAM | PFC_FAULT_CONTROL);
    }

    pi_output_max = params->current_command_target / PFC_CONTROL_SQRT_TWO;
    PI_Init(&voltage_pi, params->voltage_pi_kp, params->voltage_pi_ki,
            params->voltage_loop_frequency_hz, 0.0f, pi_output_max);
    PR_Init(&current_pr, params->current_pr_kp, params->current_pr_kr,
            params->grid_frequency_hz, params->current_pr_bandwidth_rad_s,
            params->control_frequency, -params->current_pr_output_limit_v,
            params->current_pr_output_limit_v);
    error_window_samples = (uint32_t)(params->control_frequency /
                                      params->grid_frequency_hz + 0.5f);

    /*
     * 母线单相功率脉动为2倍工频。对该频率做Tustin预畸变，5 V档得到
     * b0=0.9771472、b1=-1.5810574、b2=0.9771472、a1=-1.5810574、
     * a2=0.9542945；参数档改变采样频率或工频后会在初始化时自动重算。
     */
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

    if ((voltage_pi.initialized == 0U) || (current_pr.initialized == 0U) ||
        (error_window_samples == 0U) ||
        (PFC_ControlFloatValid(notch_coefficients.b0) == 0U) ||
        (PFC_ControlFloatValid(notch_coefficients.b1) == 0U) ||
        (PFC_ControlFloatValid(notch_coefficients.a2) == 0U) ||
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
    PR_Reset(&current_pr);
    (void)memset(&vbus_notch, 0, sizeof(vbus_notch));
    control_current_rms_command = 0.0f;
    control_vbus_reference = 0.0f;
    control_last_valid_vbus = 0.0f;
    error_square_sum = 0.0f;
    error_sample_count = 0U;
    current_mode_samples = 0U;
    outer_divider = 0U;
    saturation_count = 0U;
    SPWM_Reset();

    (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));
    control_telemetry.mode = PFC_CONTROL_IDLE;
    control_telemetry.healthy = control_initialized;
}

HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_Measurement *measurement)
{
    uint32_t primask;
    HAL_StatusTypeDef status = HAL_OK;

    if ((control_initialized == 0U) || (measurement == 0) ||
        ((mode != PFC_CONTROL_IDLE) && (measurement->valid == 0U)))
    {
        return HAL_ERROR;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (mode == PFC_CONTROL_IDLE)
    {
        PFC_Control_Reset();
    }
    else if (mode == PFC_CONTROL_PRIME)
    {
        /*
         * PRIME阶段A/B输出仍关闭，只让10 kHz路径把VAC/VBUS前馈写入预装载寄存器。
         * 电流指令和PR历史保持为0，等新的正向过零到来后再由应用层开放输出。
         */
        PI_Reset(&voltage_pi);
        PR_Reset(&current_pr);
        control_current_rms_command = 0.0f;
        control_vbus_reference = measurement->vbus;
        error_square_sum = 0.0f;
        error_sample_count = 0U;
        current_mode_samples = 0U;
        outer_divider = 0U;
        saturation_count = 0U;
        control_telemetry.current_loop_qualified = 0U;
        control_telemetry.vbus_reference_reached = 0U;
        control_telemetry.fault_bits = 0U;
        control_telemetry.mode = mode;
        control_telemetry.prime_start_cross_sequence =
            measurement->vac_zero_cross_sequence;
        control_telemetry.prime_waiting = 1U;
        control_telemetry.startup_modulation = 0.0f;
        control_telemetry.healthy = 1U;
    }
    else if (mode == PFC_CONTROL_CURRENT_RAMP)
    {
        PI_Reset(&voltage_pi);
        PR_Reset(&current_pr);
        PFC_ControlNotchPreset(measurement->vbus);
        control_current_rms_command = 0.0f;
        control_vbus_reference = measurement->vbus;
        if ((measurement->vbus > PFC_CONTROL_VBUS_DIV_MIN) &&
            (PFC_ControlFloatValid(measurement->vbus) != 0U))
        {
            control_last_valid_vbus = measurement->vbus;
        }
#if PFC_USER_RELAXED_PWM_TEST != 0U
        else
        {
            control_last_valid_vbus = control_params->vbus_target;
            control_vbus_reference = control_last_valid_vbus;
        }
#endif
        error_square_sum = 0.0f;
        error_sample_count = 0U;
        current_mode_samples = 0U;
        outer_divider = 0U;
        saturation_count = 0U;
        control_telemetry.current_loop_qualified = 0U;
        control_telemetry.vbus_reference_reached = 0U;
        control_telemetry.fault_bits = 0U;
        control_telemetry.mode = mode;
        control_telemetry.healthy = 1U;
    }
    else if (mode == PFC_CONTROL_VBUS_RAMP)
    {
        PFC_ControlNotchPreset(measurement->vbus);
        control_vbus_reference = measurement->vbus;
        outer_divider = 0U;
        if (PI_PresetOutput(&voltage_pi, control_vbus_reference,
                            measurement->vbus,
                            control_current_rms_command) == 0U)
        {
            status = HAL_ERROR;
        }
        else
        {
            control_telemetry.mode = mode;
            control_telemetry.healthy = 1U;
        }
    }
    else if (mode == PFC_CONTROL_VBUS_RUN)
    {
        control_vbus_reference = control_params->vbus_target;
        control_telemetry.mode = mode;
        control_telemetry.vbus_reference_reached = 1U;
    }
    else
    {
        status = HAL_ERROR;
    }
    if (primask == 0U)
    {
        __enable_irq();
    }
    return status;
}

HAL_StatusTypeDef PFC_Control_Step10k(const PFC_Measurement *measurement)
{
    float current_reference;
    float current_error;
    float pr_output;
    float bridge_voltage_reference;
    float modulation;
    float vbus_for_control;
    float ramp_step;
    uint8_t saturated;

    if ((control_initialized == 0U) || (control_params == 0) ||
        (measurement == 0) || (measurement->valid == 0U))
    {
        return PFC_ControlFail(PFC_FAULT_CONTROL);
    }

    if (control_telemetry.mode == PFC_CONTROL_PRIME)
    {
        float start_vac_window = control_params->vac_nominal_rms *
                                 PFC_CONTROL_START_VAC_RATIO;

        if (start_vac_window < PFC_CONTROL_START_VAC_MIN_V)
        {
            start_vac_window = PFC_CONTROL_START_VAC_MIN_V;
        }
        vbus_for_control = measurement->vbus;
        if ((PFC_ControlFloatValid(vbus_for_control) == 0U) ||
            (vbus_for_control <= PFC_CONTROL_VBUS_DIV_MIN))
        {
            /* 没有真实母线除数时继续等待，绝不使用60 V目标值伪造首周期前馈。 */
            control_telemetry.prime_waiting = 1U;
            control_telemetry.startup_modulation = 0.0f;
            control_telemetry.fast_heartbeat++;
            control_telemetry.healthy = 1U;
            return HAL_OK;
        }

        control_last_valid_vbus = vbus_for_control;
        modulation = (float)control_params->bridge_polarity *
                     measurement->vac / vbus_for_control;
        if ((PFC_ControlFloatValid(modulation) == 0U) ||
            (SPWM_ApplyModulation(modulation) != HAL_OK))
        {
            return PFC_ControlFail(PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
        }

        control_telemetry.vbus_reference = vbus_for_control;
        control_telemetry.vbus_filtered = vbus_for_control;
        control_telemetry.current_rms_command = 0.0f;
        control_telemetry.current_reference = 0.0f;
        control_telemetry.current_error = -measurement->ipfc;
        control_telemetry.pr_output_v = 0.0f;
        control_telemetry.modulation = SPWM_GetModulation();
        control_telemetry.startup_modulation = SPWM_GetModulation();
        control_telemetry.prime_waiting =
            (uint8_t)(((measurement->vac_locked != 0U) &&
                       (measurement->vac_zero_cross_sequence !=
                        control_telemetry.prime_start_cross_sequence) &&
                       (fabsf(measurement->vac) <= start_vac_window)) ? 0U : 1U);
        control_telemetry.fast_heartbeat++;
        control_telemetry.healthy = 1U;
        return HAL_OK;
    }

    if ((current_pr.initialized == 0U) || (voltage_pi.initialized == 0U) ||
        (control_telemetry.mode == PFC_CONTROL_IDLE) ||
        (control_telemetry.mode > PFC_CONTROL_VBUS_RUN))
    {
        return PFC_ControlFail(PFC_FAULT_CONTROL);
    }

    vbus_for_control = measurement->vbus;
    if ((PFC_ControlFloatValid(vbus_for_control) != 0U) &&
        (vbus_for_control > PFC_CONTROL_VBUS_DIV_MIN))
    {
        control_last_valid_vbus = vbus_for_control;
    }
    else
    {
#if PFC_USER_RELAXED_PWM_TEST != 0U
        vbus_for_control = (control_last_valid_vbus > PFC_CONTROL_VBUS_DIV_MIN) ?
                           control_last_valid_vbus : control_params->vbus_target;
#else
        return PFC_ControlFail(PFC_FAULT_CONTROL);
#endif
    }

    if (control_telemetry.mode == PFC_CONTROL_CURRENT_RAMP)
    {
        ramp_step = control_params->current_probe_slew_a_s /
                    control_params->control_frequency;
        control_current_rms_command += ramp_step;
        if (control_current_rms_command > control_params->current_probe_rms)
        {
            control_current_rms_command = control_params->current_probe_rms;
        }
        current_mode_samples++;
    }
    else if (++outer_divider >= PFC_CONTROL_PI_DIVIDER)
    {
        float vbus_filtered;
        outer_divider = 0U;
        vbus_filtered = PFC_ControlNotchStep(vbus_for_control);
        if (PFC_ControlFloatValid(vbus_filtered) == 0U)
        {
            return PFC_ControlFail(PFC_FAULT_CONTROL);
        }

        if (control_telemetry.mode == PFC_CONTROL_VBUS_RAMP)
        {
            control_vbus_reference += control_params->vbus_reference_slew_v_s /
                                      control_params->voltage_loop_frequency_hz;
            if (control_vbus_reference >= control_params->vbus_target)
            {
                control_vbus_reference = control_params->vbus_target;
                control_telemetry.vbus_reference_reached = 1U;
            }
        }
        control_current_rms_command = PI_Calc(&voltage_pi,
                                              control_vbus_reference,
                                              vbus_filtered);
        if (PFC_ControlFloatValid(control_current_rms_command) == 0U)
        {
            return PFC_ControlFail(PFC_FAULT_CONTROL);
        }
        control_telemetry.vbus_filtered = vbus_filtered;
        control_telemetry.outer_heartbeat++;
    }

    current_reference = control_current_rms_command * measurement->vac /
                        control_params->vac_nominal_rms;
    current_error = current_reference - measurement->ipfc;
    pr_output = PR_Calc(&current_pr, current_reference, measurement->ipfc);
    bridge_voltage_reference = measurement->vac - pr_output;
    if ((PFC_ControlFloatValid(current_reference) == 0U) ||
        (PFC_ControlFloatValid(current_error) == 0U) ||
        (PFC_ControlFloatValid(current_pr.raw_output) == 0U) ||
        (PFC_ControlFloatValid(pr_output) == 0U) ||
        (PFC_ControlFloatValid(bridge_voltage_reference) == 0U))
    {
        return PFC_ControlFail(PFC_FAULT_CONTROL);
    }

    modulation = (float)control_params->bridge_polarity *
                 bridge_voltage_reference / vbus_for_control;
    if ((PFC_ControlFloatValid(modulation) == 0U) ||
        (SPWM_ApplyModulation(modulation) != HAL_OK))
    {
        return PFC_ControlFail(PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
    }

    error_square_sum += current_error * current_error;
    error_sample_count++;
    if (error_sample_count >= error_window_samples)
    {
        control_telemetry.current_error_rms =
            sqrtf(error_square_sum / (float)error_sample_count);
        error_square_sum = 0.0f;
        error_sample_count = 0U;
        if ((control_telemetry.mode == PFC_CONTROL_CURRENT_RAMP) &&
            (control_current_rms_command >= control_params->current_probe_rms) &&
            (current_mode_samples >=
             ((uint32_t)control_params->current_probe_min_ms *
              (uint32_t)(control_params->control_frequency / 1000.0f))) &&
            (control_telemetry.current_error_rms <=
             control_params->current_error_rms_max))
        {
            control_telemetry.current_loop_qualified = 1U;
        }
    }

    saturated = (uint8_t)((current_pr.limited != 0U) ||
                          (SPWM_IsLimited() != 0U));
    if (saturated != 0U)
    {
        if (saturation_count < 0xFFFFU)
        {
            saturation_count++;
        }
        if (saturation_count >= control_params->saturation_trip_samples)
        {
#if PFC_USER_RELAXED_PWM_TEST == 0U
            return PFC_ControlFail(PFC_FAULT_CONTROL_SATURATION);
#else
            saturation_count = control_params->saturation_trip_samples;
#endif
        }
    }
    else
    {
        saturation_count = 0U;
    }

    control_telemetry.vbus_reference = control_vbus_reference;
    if (control_telemetry.mode == PFC_CONTROL_CURRENT_RAMP)
    {
        control_telemetry.vbus_filtered = vbus_for_control;
    }
    control_telemetry.current_rms_command = control_current_rms_command;
    control_telemetry.current_reference = current_reference;
    control_telemetry.current_error = current_error;
    control_telemetry.pr_output_v = pr_output;
    control_telemetry.modulation = SPWM_GetModulation();
    control_telemetry.saturation_count = saturation_count;
    control_telemetry.pi_limited = voltage_pi.limited;
    control_telemetry.pr_limited = current_pr.limited;
    control_telemetry.modulation_limited = SPWM_IsLimited();
    control_telemetry.fast_heartbeat++;
    control_telemetry.healthy = 1U;
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
    *telemetry = control_telemetry;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

uint8_t PFC_Control_IsHealthy(void)
{
    return control_telemetry.healthy;
}
