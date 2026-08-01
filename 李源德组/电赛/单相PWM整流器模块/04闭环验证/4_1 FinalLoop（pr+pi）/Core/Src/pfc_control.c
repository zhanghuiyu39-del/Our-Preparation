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
/* 正向过零样本允许的投入窗口：5 V档至少0.5 V，36 V档按额定RMS的8%得到2.88 V。 */
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
volatile PFC_ControlFailureSnapshot PFC_ControlDiagnostic; /* Keil Watch公开只读诊断现场。 */

/* 以下运行量只由10 kHz ISR写；EnterMode在关中断临界区内修改。 */
static float control_current_rms_command = 0.0f; /* A RMS，当前内环幅值命令。 */
static float control_vbus_reference = 0.0f;      /* V，当前外环软启动参考。 */
static float control_last_valid_vbus = 0.0f;     /* V，放宽模式发生单帧异常时的控制除数备份。 */
static float vbus_filter_sample_1 = 0.0f;        /* V，最近一个有效VBUS样本；仅由10 kHz控制路径写入。 */
static float vbus_filter_sample_2 = 0.0f;        /* V，前一个有效VBUS样本；用于中值滤波抑制尖峰。 */
static float vbus_filter_output = 0.0f;          /* V，中值加一阶低通后的控制反馈。 */
static uint8_t vbus_filter_initialized = 0U;     /* 1表示滤波器已有有效初值。 */
static float error_square_sum = 0.0f;            /* A^2，工频周期误差平方和。 */
static uint32_t error_sample_count = 0U;          /* 当前误差RMS窗口样本数。 */
static uint32_t error_window_samples = 200U;      /* 一工频周期对应的10 kHz样本数。 */
static uint32_t current_mode_samples = 0U;        /* 电流探测阶段累计10 kHz样本数。 */
static uint16_t outer_divider = 0U;               /* 10 kHz到1 kHz的分频计数。 */
static uint16_t saturation_count = 0U;            /* 连续限幅样本数。 */
static uint32_t period_sample_count = 0U;         /* 当前工频周期已经统计的10 kHz样本数。 */
static uint32_t period_saturation_count = 0U;     /* 当前工频周期PR/SPWM限幅样本数。 */
static uint8_t period_saturation_bad_cycles = 0U;/* 连续严重削顶的完整工频周期数。 */
static uint16_t vbus_regulation_error_ms = 0U;    /* PI限幅且母线失调的连续时间。 */
static uint16_t outer_period_ms = 1U;             /* 每次外环计算代表的时间，当前1 kHz时为1 ms。 */
static uint8_t control_initialized = 0U;          /* PI、PR、SPWM均初始化成功。 */

/** @brief 判断单精度值是否有限，防止NaN或无穷进入IIR和HRTIM。 */
static uint8_t PFC_ControlFloatValid(float value)
{
    return (uint8_t)((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

/**
 * @brief  对母线反馈做轻量中值和低通处理。
 * @param  sample_v 当前一致性快照中的VBUS工程量，单位V。
 * @param  filtered_v 返回供PI和调制除法使用的滤波值，单位V。
 * @retval 1 当前样本有效且输出大于最小除数；0 表示本帧不能更新滤波器。
 * @note   只在10 kHz控制路径调用；不访问DMA、不发送通信、不执行阻塞等待。
 *         该处理只抑制ADC偶发尖峰，不能修复比例、零点或桥臂硬件故障。
 */
static uint8_t PFC_ControlFilterVbus(float sample_v, float *filtered_v)
{
    float a;
    float b;
    float c;
    float median_v;
    float temp;

    if ((filtered_v == 0) ||
        (PFC_ControlFloatValid(sample_v) == 0U) ||
        (sample_v <= PFC_CONTROL_VBUS_DIV_MIN))
    {
        return 0U;
    }

    if (vbus_filter_initialized == 0U)
    {
        vbus_filter_sample_1 = sample_v;
        vbus_filter_sample_2 = sample_v;
        vbus_filter_output = sample_v;
        vbus_filter_initialized = 1U;
    }
    else
    {
        a = vbus_filter_sample_2;
        b = vbus_filter_sample_1;
        c = sample_v;
        vbus_filter_sample_2 = vbus_filter_sample_1;
        vbus_filter_sample_1 = sample_v;
        if (a > b) { temp = a; a = b; b = temp; }
        if (b > c) { temp = b; b = c; c = temp; }
        if (a > b) { temp = a; a = b; b = temp; }
        median_v = b;
        /* alpha=0.10只用于控制反馈；原始VBUS仍保存在measurement中供诊断。 */
        vbus_filter_output += 0.10f * (median_v - vbus_filter_output);
    }

    if ((PFC_ControlFloatValid(vbus_filter_output) == 0U) ||
        (vbus_filter_output <= PFC_CONTROL_VBUS_DIV_MIN))
    {
        return 0U;
    }
    *filtered_v = vbus_filter_output;
    return 1U;
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

/**
 * @brief 记录控制路径的首个细分失败原因和公共故障位。
 * @param reason 本次失败对应的两位D诊断码。
 * @param fault_bits 写入测量层的PFC_FaultMask组合。
 * @note  后续连带失败不得覆盖首个原因；实际HRTIM关断仍由PFC_AppTrip()完成。
 */
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
            control_current_rms_command;
        PFC_ControlDiagnostic.current_reference_a =
            control_telemetry.current_reference;
        PFC_ControlDiagnostic.current_error_a =
            control_telemetry.current_error;
        PFC_ControlDiagnostic.pr_raw_output_v = current_pr.raw_output;
        PFC_ControlDiagnostic.pr_output_v = control_telemetry.pr_output_v;
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
        (params->voltage_loop_frequency_hz <=
         (4.0f * params->grid_frequency_hz)) ||
        (fabsf(params->control_frequency -
               PFC_CONTROL_PI_DIVIDER * params->voltage_loop_frequency_hz) > 0.5f))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_INIT_PARAM,
                               PFC_FAULT_PARAM | PFC_FAULT_CONTROL);
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
    outer_period_ms = (uint16_t)(1000.0f /
                                 params->voltage_loop_frequency_hz + 0.5f);

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
        (error_window_samples == 0U) || (outer_period_ms == 0U) ||
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
    PR_Reset(&current_pr);
    (void)memset(&vbus_notch, 0, sizeof(vbus_notch));
    control_current_rms_command = 0.0f;
    control_vbus_reference = 0.0f;
    control_last_valid_vbus = 0.0f;
    vbus_filter_sample_1 = 0.0f;
    vbus_filter_sample_2 = 0.0f;
    vbus_filter_output = 0.0f;
    vbus_filter_initialized = 0U;
    error_square_sum = 0.0f;
    error_sample_count = 0U;
    current_mode_samples = 0U;
    outer_divider = 0U;
    saturation_count = 0U;
    period_sample_count = 0U;
    period_saturation_count = 0U;
    period_saturation_bad_cycles = 0U;
    vbus_regulation_error_ms = 0U;
    SPWM_Reset();

    (void)memset((void *)&control_telemetry, 0, sizeof(control_telemetry));
    (void)memset((void *)&PFC_ControlDiagnostic, 0,
                 sizeof(PFC_ControlDiagnostic));
    control_telemetry.mode = PFC_CONTROL_IDLE;
    control_telemetry.healthy = control_initialized;
}

void PFC_Control_ResetPreserveFault(void)
{
    PFC_ControlTelemetry failed_telemetry = control_telemetry;
    PFC_ControlFailureSnapshot failed_diagnostic = PFC_ControlDiagnostic;

    /*
     * 先清空控制器历史，避免故障后的PI积分量或PR历史继续保留；再恢复只读遥测，
     * 这样Keil/OLED看到的是故障发生瞬间，而不是被Reset改写后的全零数据。
     */
    PFC_Control_Reset();
    control_telemetry = failed_telemetry;
    PFC_ControlDiagnostic = failed_diagnostic;
    control_telemetry.healthy = 0U;
}

HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_Measurement *measurement)
{
    uint32_t primask;
    HAL_StatusTypeDef status = HAL_OK;
    float vbus_for_mode = 0.0f;

    if ((control_initialized == 0U) || (control_params == 0) ||
        (measurement == 0) ||
        ((mode != PFC_CONTROL_IDLE) && (measurement->valid == 0U)))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_ENTER_PRECONDITION,
                               PFC_FAULT_CONTROL);
    }

    if (measurement != 0)
    {
        PFC_ControlDiagnostic.vbus_v = measurement->vbus;
        PFC_ControlDiagnostic.vac_v = measurement->vac;
        PFC_ControlDiagnostic.ipfc_a = measurement->ipfc;
        if ((measurement->vbus > PFC_CONTROL_VBUS_DIV_MIN) &&
            (PFC_ControlFloatValid(measurement->vbus) != 0U))
        {
            control_last_valid_vbus = measurement->vbus;
            PFC_ControlDiagnostic.vbus_used_v = measurement->vbus;
        }
#if PFC_USER_RELAXED_PWM_TEST != 0U
        if ((control_last_valid_vbus > PFC_CONTROL_VBUS_DIV_MIN) &&
            (PFC_ControlFloatValid(control_last_valid_vbus) != 0U))
        {
            vbus_for_mode = control_last_valid_vbus;
        }
        else if (mode != PFC_CONTROL_PRIME)
        {
            vbus_for_mode = control_params->vbus_target;
        }
#else
        vbus_for_mode = measurement->vbus;
#endif
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
         * PRIME只准备首周期前馈，不开放PWM、不爬升电流。此处保留真实VBUS为0的状态，
         * 让10 kHz路径继续等待有效采样，禁止用60 V目标值伪造首次投入除数。
         */
        PI_Reset(&voltage_pi);
        PR_Reset(&current_pr);
        PFC_ControlNotchPreset(vbus_for_mode);
        control_current_rms_command = 0.0f;
        control_vbus_reference = vbus_for_mode;
        error_square_sum = 0.0f;
        error_sample_count = 0U;
        current_mode_samples = 0U;
        outer_divider = 0U;
        saturation_count = 0U;
        period_sample_count = 0U;
        period_saturation_count = 0U;
        period_saturation_bad_cycles = 0U;
        vbus_regulation_error_ms = 0U;
        control_telemetry.current_loop_qualified = 0U;
        control_telemetry.vbus_reference_reached = 0U;
        control_telemetry.fault_bits = 0U;
        control_telemetry.mode = mode;
        control_telemetry.healthy = 1U;
        control_telemetry.prime_start_cross_sequence =
            measurement->vac_zero_cross_sequence;
        control_telemetry.output_enable_cross_sequence = 0U;
        control_telemetry.startup_vac_v = measurement->vac;
        control_telemetry.startup_vbus_v = vbus_for_mode;
        control_telemetry.startup_modulation = 0.0f;
        control_telemetry.prime_waiting = 1U;
        control_telemetry.output_enable_count = 0U;

        PFC_ControlDiagnostic.prime_start_cross_sequence =
            measurement->vac_zero_cross_sequence;
        PFC_ControlDiagnostic.output_enable_cross_sequence = 0U;
        PFC_ControlDiagnostic.startup_vac_v = measurement->vac;
        PFC_ControlDiagnostic.startup_vbus_v = vbus_for_mode;
        PFC_ControlDiagnostic.startup_modulation = 0.0f;
        PFC_ControlDiagnostic.prime_waiting = 1U;
        PFC_ControlDiagnostic.output_enable_count = 0U;
    }
    else if (mode == PFC_CONTROL_CURRENT_RAMP)
    {
        PI_Reset(&voltage_pi);
        PR_Reset(&current_pr);
        PFC_ControlNotchPreset(vbus_for_mode);
        control_current_rms_command = 0.0f;
        control_vbus_reference = vbus_for_mode;
        error_square_sum = 0.0f;
        error_sample_count = 0U;
        current_mode_samples = 0U;
        outer_divider = 0U;
        saturation_count = 0U;
        period_sample_count = 0U;
        period_saturation_count = 0U;
        period_saturation_bad_cycles = 0U;
        vbus_regulation_error_ms = 0U;
        control_telemetry.current_loop_qualified = 0U;
        control_telemetry.vbus_reference_reached = 0U;
        control_telemetry.fault_bits = 0U;
        control_telemetry.mode = mode;
        control_telemetry.healthy = 1U;
    }
    else if (mode == PFC_CONTROL_VBUS_RAMP)
    {
        PFC_ControlNotchPreset(vbus_for_mode);
        control_vbus_reference = vbus_for_mode;
        outer_divider = 0U;
        vbus_regulation_error_ms = 0U;
        if (PI_PresetOutput(&voltage_pi, control_vbus_reference,
                            vbus_for_mode,
                            control_current_rms_command) == 0U)
        {
            status = PFC_ControlFail(PFC_CONTROL_FAIL_PI_PRESET,
                                     PFC_FAULT_CONTROL);
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
        vbus_regulation_error_ms = 0U;
        /*
         * 启动接管阶段允许出现短时波峰削顶，因此进入稳态时重新开始周期统计。
         * 连续20 ms限幅计数不在这里清零，防止用模式切换掩盖已经持续的饱和。
         */
        period_sample_count = 0U;
        period_saturation_count = 0U;
        period_saturation_bad_cycles = 0U;
        control_telemetry.period_saturation_samples = 0U;
        control_telemetry.period_saturation_ratio = 0.0f;
        control_telemetry.period_saturation_bad_cycles = 0U;
        control_telemetry.mode = mode;
        control_telemetry.vbus_reference_reached = 1U;
    }
    else
    {
        status = PFC_ControlFail(PFC_CONTROL_FAIL_ENTER_MODE,
                                 PFC_FAULT_CONTROL);
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
    float ramp_step;
    float vbus_for_control;
    float filtered_vbus;
    uint8_t saturated;

    if ((control_initialized == 0U) || (control_params == 0))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_NOT_INITIALIZED,
                               PFC_FAULT_CONTROL);
    }
    if ((measurement == 0) || (measurement->valid == 0U))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_MEASUREMENT,
                               PFC_FAULT_CONTROL);
    }
    PFC_ControlDiagnostic.vbus_v = measurement->vbus;
    PFC_ControlDiagnostic.vac_v = measurement->vac;
    PFC_ControlDiagnostic.ipfc_a = measurement->ipfc;

    /*
     * VBUS是PI反馈和调制度除数。正常模式保持原有“无效即报D=09并停机”；当前放宽
     * 波形模式不再让一个异常ADC帧直接撤销PWM，而是使用最近一次有效VBUS继续计算。
     * 若启动后尚无有效历史值，则使用参数档目标值作为有限除数。替代行为只累计到
     * PFC_ControlDiagnostic，既不新增故障位，也不增加任何关断条件。
     */
    vbus_for_control = measurement->vbus;
    if (PFC_ControlFilterVbus(measurement->vbus, &filtered_vbus) != 0U)
    {
        /* 仅控制反馈使用滤波值；Diagnostic.vbus_v仍保留本帧原始换算值。 */
        vbus_for_control = filtered_vbus;
    }
    if ((PFC_ControlFloatValid(vbus_for_control) == 0U) ||
        (vbus_for_control <= PFC_CONTROL_VBUS_DIV_MIN))
    {
#if PFC_USER_RELAXED_PWM_TEST != 0U
        PFC_ControlDiagnostic.last_rejected_vbus_v = vbus_for_control;
        if ((control_last_valid_vbus > PFC_CONTROL_VBUS_DIV_MIN) &&
            (PFC_ControlFloatValid(control_last_valid_vbus) != 0U))
        {
            vbus_for_control = control_last_valid_vbus;
            PFC_ControlDiagnostic.vbus_fallback_count++;
        }
        else if (control_telemetry.mode != PFC_CONTROL_PRIME)
        {
            vbus_for_control = control_params->vbus_target;
            PFC_ControlDiagnostic.vbus_fallback_count++;
        }
        else
        {
            /* PRIME必须先取得真实母线样本；无效时保持等待且输出继续关闭。 */
            vbus_for_control = 0.0f;
        }
#else
        return PFC_ControlFail(PFC_CONTROL_FAIL_VBUS_DIVISION,
                               PFC_FAULT_CONTROL);
#endif
    }
    else
    {
        control_last_valid_vbus = vbus_for_control;
    }
    PFC_ControlDiagnostic.vbus_used_v = vbus_for_control;

    if (control_telemetry.mode == PFC_CONTROL_PRIME)
    {
        float start_vac_window = control_params->vac_nominal_rms *
                                 PFC_CONTROL_START_VAC_RATIO;

        if (start_vac_window < PFC_CONTROL_START_VAC_MIN_V)
        {
            start_vac_window = PFC_CONTROL_START_VAC_MIN_V;
        }

        control_telemetry.startup_vac_v = measurement->vac;
        control_telemetry.startup_vbus_v = vbus_for_control;
        PFC_ControlDiagnostic.startup_vac_v = measurement->vac;
        PFC_ControlDiagnostic.startup_vbus_v = vbus_for_control;

        if ((vbus_for_control <= PFC_CONTROL_VBUS_DIV_MIN) ||
            (PFC_ControlFloatValid(vbus_for_control) == 0U))
        {
            control_telemetry.prime_waiting = 1U;
            control_telemetry.current_rms_command = 0.0f;
            control_telemetry.pr_output_v = 0.0f;
            control_telemetry.fast_heartbeat++;
            control_telemetry.healthy = 1U;
            PFC_ControlDiagnostic.prime_waiting = 1U;
            return HAL_OK;
        }

        modulation = (float)control_params->bridge_polarity *
                     measurement->vac / vbus_for_control;
        if (PFC_ControlFloatValid(modulation) == 0U)
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_MODULATION,
                                   PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
        }
        if (SPWM_ApplyModulation(modulation) != HAL_OK)
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_SPWM_WRITE,
                                   PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
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

        PFC_ControlDiagnostic.startup_modulation = SPWM_GetModulation();
        PFC_ControlDiagnostic.modulation = SPWM_GetModulation();
        PFC_ControlDiagnostic.prime_waiting = control_telemetry.prime_waiting;
        return HAL_OK;
    }

    if ((current_pr.initialized == 0U) || (voltage_pi.initialized == 0U))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_CONTROLLER_STATE,
                               PFC_FAULT_CONTROL);
    }
    if ((control_telemetry.mode == PFC_CONTROL_IDLE) ||
        (control_telemetry.mode > PFC_CONTROL_VBUS_RUN))
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_MODE, PFC_FAULT_CONTROL);
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
            return PFC_ControlFail(PFC_CONTROL_FAIL_NOTCH_NONFINITE,
                                   PFC_FAULT_CONTROL);
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
            return PFC_ControlFail(PFC_CONTROL_FAIL_PI_NONFINITE,
                                   PFC_FAULT_CONTROL);
        }
        control_telemetry.vbus_filtered = vbus_filtered;
        control_telemetry.pi_limited = voltage_pi.limited;
        control_telemetry.outer_heartbeat++;

        /*
         * 启动斜坡期间PI短时达到上限属于渐进接管过程，因此只在VBUS_RUN稳态监督。
         * 若PI已经打满且母线仍偏离目标容差，继续运行只会增加器件应力，超时后停机。
         */
        if ((control_telemetry.mode == PFC_CONTROL_VBUS_RUN) &&
            (voltage_pi.limited != 0U) &&
            (fabsf(vbus_filtered - control_params->vbus_target) >
             control_params->vbus_target_tolerance))
        {
            if ((uint32_t)vbus_regulation_error_ms + outer_period_ms >=
                control_params->vbus_regulation_timeout_ms)
            {
                vbus_regulation_error_ms =
                    control_params->vbus_regulation_timeout_ms;
            }
            else
            {
                vbus_regulation_error_ms =
                    (uint16_t)(vbus_regulation_error_ms + outer_period_ms);
            }
            control_telemetry.vbus_regulation_error_ms =
                vbus_regulation_error_ms;
            if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
                (vbus_regulation_error_ms >=
                 control_params->vbus_regulation_timeout_ms))
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

    current_reference = control_current_rms_command * measurement->vac /
                        control_params->vac_nominal_rms;
    current_error = current_reference - measurement->ipfc;
    pr_output = PR_Calc(&current_pr, current_reference, measurement->ipfc);
    bridge_voltage_reference = measurement->vac - pr_output;

    /* 先发布失败现场；即使本周期随后退出，故障保持接口也能保留这些数值。 */
    control_telemetry.current_rms_command = control_current_rms_command;
    control_telemetry.current_reference = current_reference;
    control_telemetry.current_error = current_error;
    control_telemetry.pr_output_v = pr_output;
    PFC_ControlDiagnostic.current_rms_command_a =
        control_current_rms_command;
    PFC_ControlDiagnostic.current_reference_a = current_reference;
    PFC_ControlDiagnostic.current_error_a = current_error;
    PFC_ControlDiagnostic.pr_raw_output_v = current_pr.raw_output;
    PFC_ControlDiagnostic.pr_output_v = pr_output;
    PFC_ControlDiagnostic.bridge_reference_v = bridge_voltage_reference;
    if (PFC_ControlFloatValid(current_reference) == 0U)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_CURRENT_REFERENCE,
                               PFC_FAULT_CONTROL);
    }
    if (PFC_ControlFloatValid(current_error) == 0U)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_CURRENT_ERROR,
                               PFC_FAULT_CONTROL);
    }
    if (PFC_ControlFloatValid(current_pr.raw_output) == 0U)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_PR_RAW,
                               PFC_FAULT_CONTROL);
    }
    if (PFC_ControlFloatValid(pr_output) == 0U)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_PR_OUTPUT,
                               PFC_FAULT_CONTROL);
    }
    if (PFC_ControlFloatValid(bridge_voltage_reference) == 0U)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_BRIDGE_REFERENCE,
                               PFC_FAULT_CONTROL);
    }

    modulation = (float)control_params->bridge_polarity *
                 bridge_voltage_reference / vbus_for_control;
    control_telemetry.vac_bus_ratio = fabsf(measurement->vac) /
                                      vbus_for_control;
    control_telemetry.modulation_headroom = control_params->modulation_limit -
                                             control_telemetry.vac_bus_ratio;
    control_telemetry.modulation = modulation;
    PFC_ControlDiagnostic.modulation = modulation;
    if (PFC_ControlFloatValid(modulation) == 0U)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_MODULATION,
                               PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
    }
    if (SPWM_ApplyModulation(modulation) != HAL_OK)
    {
        return PFC_ControlFail(PFC_CONTROL_FAIL_SPWM_WRITE,
                               PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
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
    control_telemetry.pr_limited = current_pr.limited;
    control_telemetry.modulation_limited = SPWM_IsLimited();
    if (saturated != 0U)
    {
        if (saturation_count < 0xFFFFU)
        {
            saturation_count++;
        }
        if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
            (saturation_count >= control_params->saturation_trip_samples))
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_CONTINUOUS_SATURATION,
                                   PFC_FAULT_CONTROL_SATURATION);
        }
    }
    else
    {
        saturation_count = 0U;
    }

    /*
     * 连续限幅可发现控制器卡死；波峰削顶却会在过零附近清零连续计数。这里再按
     * 一个工频周期统计限幅占比，连续多个异常周期才判定为严重周期性削顶。
     */
    period_sample_count++;
    if (saturated != 0U)
    {
        period_saturation_count++;
    }
    control_telemetry.period_saturation_samples =
        (period_saturation_count > 0xFFFFU) ? 0xFFFFU :
        (uint16_t)period_saturation_count;
    if (period_sample_count >= error_window_samples)
    {
        control_telemetry.period_saturation_ratio =
            (float)period_saturation_count / (float)period_sample_count;
        if ((control_telemetry.mode == PFC_CONTROL_VBUS_RUN) &&
            (control_telemetry.period_saturation_ratio >
             control_params->period_saturation_ratio))
        {
            if (period_saturation_bad_cycles < 0xFFU)
            {
                period_saturation_bad_cycles++;
            }
        }
        else
        {
            /* CURRENT_RAMP/VBUS_RAMP只保留占比遥测，不累计会导致关断的异常周期。 */
            period_saturation_bad_cycles = 0U;
        }
        control_telemetry.period_saturation_bad_cycles =
            period_saturation_bad_cycles;
        period_sample_count = 0U;
        period_saturation_count = 0U;
        control_telemetry.period_saturation_samples = 0U;
        if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
            (control_telemetry.mode == PFC_CONTROL_VBUS_RUN) &&
            (period_saturation_bad_cycles >=
             control_params->period_saturation_cycles))
        {
            return PFC_ControlFail(PFC_CONTROL_FAIL_PERIOD_SATURATION,
                                   PFC_FAULT_CONTROL_SATURATION);
        }
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

void PFC_Control_ConfirmOutputsEnabled(const PFC_Measurement *measurement)
{
    uint32_t primask;

    if (measurement == 0)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    control_telemetry.output_enable_cross_sequence =
        measurement->vac_zero_cross_sequence;
    PFC_ControlDiagnostic.output_enable_cross_sequence =
        measurement->vac_zero_cross_sequence;
    if (control_telemetry.output_enable_count < 0xFFU)
    {
        control_telemetry.output_enable_count++;
    }
    PFC_ControlDiagnostic.output_enable_count =
        control_telemetry.output_enable_count;
    if (primask == 0U)
    {
        __enable_irq();
    }
}
