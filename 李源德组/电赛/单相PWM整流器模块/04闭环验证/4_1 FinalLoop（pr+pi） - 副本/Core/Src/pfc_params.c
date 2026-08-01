#include "pfc_params.h"

/*
 * 本模块把pfc_user_config.h中的人工配置转换为只读PFC_Params对象，并提供分层校验。
 * 数据流：用户配置宏 -> 只读参数对象 -> 测量/AWD、控制器、状态机与显示。
 *
 * 本文件不保存需要人工修改的数值，也不负责在线改参或Flash存储。用户调试时只改
 * pfc_user_config.h；运行模式、ADC工程量许可和PWM功率许可不能再由一个标志混用。
 */

/* 依据活动参数档，把统一字段名映射到对应的用户配置区域。 */
#if PFC_USER_ACTIVE_PROFILE == PFC_USER_PROFILE_5V
#define PFC_CFG_PROFILE_ID                 PFC_PROFILE_5V_TEST
#define PFC_CFG_CALIBRATION_CONFIRMED      PFC_USER_5V_CALIBRATION_CONFIRMED
#define PFC_CFG_VAC_NOMINAL_RMS            PFC_USER_5V_VAC_NOMINAL_RMS
#define PFC_CFG_GRID_FREQUENCY_HZ          PFC_USER_5V_GRID_FREQUENCY_HZ
#define PFC_CFG_GRID_TOLERANCE_HZ          PFC_USER_5V_GRID_TOLERANCE_HZ
#define PFC_CFG_VAC_PEAK_TRIP_V            PFC_USER_5V_VAC_PEAK_TRIP_V
#define PFC_CFG_VBUS_TARGET_V              PFC_USER_5V_VBUS_TARGET_V
#define PFC_CFG_VBUS_START_MIN_V           PFC_USER_5V_VBUS_START_MIN_V
#define PFC_CFG_VBUS_RUN_MIN_V             PFC_USER_5V_VBUS_RUN_MIN_V
#define PFC_CFG_VBUS_WARN_V                PFC_USER_5V_VBUS_WARN_V
#define PFC_CFG_VBUS_TRIP_V                PFC_USER_5V_VBUS_TRIP_V
#define PFC_CFG_VBUS_TOLERANCE_V           PFC_USER_5V_VBUS_TOLERANCE_V
#define PFC_CFG_LOAD_RESISTANCE_OHM        PFC_USER_5V_LOAD_RESISTANCE_OHM
#define PFC_CFG_INPUT_INDUCTANCE_H         PFC_USER_5V_INPUT_INDUCTANCE_H
#define PFC_CFG_BUS_CAPACITANCE_F          PFC_USER_5V_BUS_CAPACITANCE_F
#define PFC_CFG_CONTROL_FREQUENCY_HZ       PFC_USER_5V_CONTROL_FREQUENCY_HZ
#define PFC_CFG_MODULATION_LIMIT           PFC_USER_5V_MODULATION_LIMIT
#define PFC_CFG_CURRENT_TARGET_A_PEAK      PFC_USER_5V_CURRENT_TARGET_A_PEAK
#define PFC_CFG_CURRENT_RAMP_A_PER_S       PFC_USER_5V_CURRENT_RAMP_A_PER_S
#define PFC_CFG_CURRENT_TRIP_A_PEAK        PFC_USER_5V_CURRENT_TRIP_A_PEAK
#define PFC_CFG_PR_KP                      PFC_USER_5V_PR_KP
#define PFC_CFG_PR_KR                      PFC_USER_5V_PR_KR
#define PFC_CFG_PR_BANDWIDTH_RAD_S         PFC_USER_5V_PR_BANDWIDTH_RAD_S
#define PFC_CFG_PR_OUTPUT_LIMIT_V          PFC_USER_5V_PR_OUTPUT_LIMIT_V
#define PFC_CFG_PI_KP_A_PER_V              PFC_USER_5V_PI_KP_A_PER_V
#define PFC_CFG_PI_KI_A_PER_VS             PFC_USER_5V_PI_KI_A_PER_VS
#define PFC_CFG_VOLTAGE_LOOP_HZ            PFC_USER_5V_VOLTAGE_LOOP_HZ
#define PFC_CFG_NOTCH_DAMPING_RAD_S        PFC_USER_5V_NOTCH_DAMPING_RAD_S
#define PFC_CFG_VBUS_SLEW_V_PER_S          PFC_USER_5V_VBUS_SLEW_V_PER_S
#define PFC_CFG_PROBE_CURRENT_A_RMS        PFC_USER_5V_PROBE_CURRENT_A_RMS
#define PFC_CFG_PROBE_SLEW_A_PER_S         PFC_USER_5V_PROBE_SLEW_A_PER_S
#define PFC_CFG_CURRENT_ERROR_MAX_A_RMS    PFC_USER_5V_CURRENT_ERROR_MAX_A_RMS
#define PFC_CFG_PROBE_MIN_MS               PFC_USER_5V_PROBE_MIN_MS
#define PFC_CFG_CURRENT_TIMEOUT_MS         PFC_USER_5V_CURRENT_TIMEOUT_MS
#define PFC_CFG_VBUS_TIMEOUT_MS            PFC_USER_5V_VBUS_TIMEOUT_MS
#define PFC_CFG_SATURATION_SAMPLES         PFC_USER_5V_SATURATION_SAMPLES
#define PFC_CFG_PERIOD_SATURATION_RATIO    PFC_USER_5V_PERIOD_SATURATION_RATIO
#define PFC_CFG_PERIOD_SATURATION_CYCLES   PFC_USER_5V_PERIOD_SATURATION_CYCLES
#define PFC_CFG_VBUS_REGULATION_TIMEOUT_MS PFC_USER_5V_VBUS_REGULATION_TIMEOUT_MS
#define PFC_CFG_IPFC_A_PER_COUNT           PFC_USER_5V_IPFC_A_PER_COUNT
#define PFC_CFG_VAC_V_PER_COUNT            PFC_USER_5V_VAC_V_PER_COUNT
#define PFC_CFG_VBUS_V_PER_COUNT           PFC_USER_5V_VBUS_V_PER_COUNT
#define PFC_CFG_IPFC_ZERO_COUNT            PFC_USER_5V_IPFC_ZERO_COUNT
#define PFC_CFG_VAC_ZERO_COUNT             PFC_USER_5V_VAC_ZERO_COUNT
#define PFC_CFG_IPFC_POLARITY              PFC_USER_5V_IPFC_POLARITY
#define PFC_CFG_VAC_POLARITY               PFC_USER_5V_VAC_POLARITY
#define PFC_CFG_BRIDGE_POLARITY            PFC_USER_5V_BRIDGE_POLARITY
#elif PFC_USER_ACTIVE_PROFILE == PFC_USER_PROFILE_36V_LIGHT_100R
#define PFC_CFG_PROFILE_ID                 PFC_PROFILE_36V_LIGHT_100R
#define PFC_CFG_CALIBRATION_CONFIRMED      PFC_USER_36V_CALIBRATION_CONFIRMED
#define PFC_CFG_VAC_NOMINAL_RMS            PFC_USER_36V_LIGHT_VAC_NOMINAL_RMS
#define PFC_CFG_GRID_FREQUENCY_HZ          PFC_USER_36V_LIGHT_GRID_FREQUENCY_HZ
#define PFC_CFG_GRID_TOLERANCE_HZ          PFC_USER_36V_LIGHT_GRID_TOLERANCE_HZ
#define PFC_CFG_VAC_PEAK_TRIP_V            PFC_USER_36V_LIGHT_VAC_PEAK_TRIP_V
#define PFC_CFG_VBUS_TARGET_V              PFC_USER_36V_LIGHT_VBUS_TARGET_V
#define PFC_CFG_VBUS_START_MIN_V           PFC_USER_36V_LIGHT_VBUS_START_MIN_V
#define PFC_CFG_VBUS_RUN_MIN_V             PFC_USER_36V_LIGHT_VBUS_RUN_MIN_V
#define PFC_CFG_VBUS_WARN_V                PFC_USER_36V_LIGHT_VBUS_WARN_V
#define PFC_CFG_VBUS_TRIP_V                PFC_USER_36V_LIGHT_VBUS_TRIP_V
#define PFC_CFG_VBUS_TOLERANCE_V           PFC_USER_36V_LIGHT_VBUS_TOLERANCE_V
#define PFC_CFG_LOAD_RESISTANCE_OHM        PFC_USER_36V_LIGHT_LOAD_RESISTANCE_OHM
#define PFC_CFG_INPUT_INDUCTANCE_H         PFC_USER_36V_LIGHT_INPUT_INDUCTANCE_H
#define PFC_CFG_BUS_CAPACITANCE_F          PFC_USER_36V_LIGHT_BUS_CAPACITANCE_F
#define PFC_CFG_CONTROL_FREQUENCY_HZ       PFC_USER_36V_LIGHT_CONTROL_FREQUENCY_HZ
#define PFC_CFG_MODULATION_LIMIT           PFC_USER_36V_LIGHT_MODULATION_LIMIT
#define PFC_CFG_CURRENT_TARGET_A_PEAK      PFC_USER_36V_LIGHT_CURRENT_TARGET_A_PEAK
#define PFC_CFG_CURRENT_RAMP_A_PER_S       PFC_USER_36V_LIGHT_CURRENT_RAMP_A_PER_S
#define PFC_CFG_CURRENT_TRIP_A_PEAK        PFC_USER_36V_LIGHT_CURRENT_TRIP_A_PEAK
#define PFC_CFG_PR_KP                      PFC_USER_36V_LIGHT_PR_KP
#define PFC_CFG_PR_KR                      PFC_USER_36V_LIGHT_PR_KR
#define PFC_CFG_PR_BANDWIDTH_RAD_S         PFC_USER_36V_LIGHT_PR_BANDWIDTH_RAD_S
#define PFC_CFG_PR_OUTPUT_LIMIT_V          PFC_USER_36V_LIGHT_PR_OUTPUT_LIMIT_V
#define PFC_CFG_PI_KP_A_PER_V              PFC_USER_36V_LIGHT_PI_KP_A_PER_V
#define PFC_CFG_PI_KI_A_PER_VS             PFC_USER_36V_LIGHT_PI_KI_A_PER_VS
#define PFC_CFG_VOLTAGE_LOOP_HZ            PFC_USER_36V_LIGHT_VOLTAGE_LOOP_HZ
#define PFC_CFG_NOTCH_DAMPING_RAD_S        PFC_USER_36V_LIGHT_NOTCH_DAMPING_RAD_S
#define PFC_CFG_VBUS_SLEW_V_PER_S          PFC_USER_36V_LIGHT_VBUS_SLEW_V_PER_S
#define PFC_CFG_PROBE_CURRENT_A_RMS        PFC_USER_36V_LIGHT_PROBE_CURRENT_A_RMS
#define PFC_CFG_PROBE_SLEW_A_PER_S         PFC_USER_36V_LIGHT_PROBE_SLEW_A_PER_S
#define PFC_CFG_CURRENT_ERROR_MAX_A_RMS    PFC_USER_36V_LIGHT_CURRENT_ERROR_MAX_A_RMS
#define PFC_CFG_PROBE_MIN_MS               PFC_USER_36V_LIGHT_PROBE_MIN_MS
#define PFC_CFG_CURRENT_TIMEOUT_MS         PFC_USER_36V_LIGHT_CURRENT_TIMEOUT_MS
#define PFC_CFG_VBUS_TIMEOUT_MS            PFC_USER_36V_LIGHT_VBUS_TIMEOUT_MS
#define PFC_CFG_SATURATION_SAMPLES         PFC_USER_36V_LIGHT_SATURATION_SAMPLES
#define PFC_CFG_PERIOD_SATURATION_RATIO    PFC_USER_36V_LIGHT_PERIOD_SATURATION_RATIO
#define PFC_CFG_PERIOD_SATURATION_CYCLES   PFC_USER_36V_LIGHT_PERIOD_SATURATION_CYCLES
#define PFC_CFG_VBUS_REGULATION_TIMEOUT_MS PFC_USER_36V_LIGHT_VBUS_REGULATION_TIMEOUT_MS
#define PFC_CFG_IPFC_A_PER_COUNT           PFC_USER_36V_IPFC_A_PER_COUNT
#define PFC_CFG_VAC_V_PER_COUNT            PFC_USER_36V_VAC_V_PER_COUNT
#define PFC_CFG_VBUS_V_PER_COUNT           PFC_USER_36V_VBUS_V_PER_COUNT
#define PFC_CFG_IPFC_ZERO_COUNT            PFC_USER_36V_IPFC_ZERO_COUNT
#define PFC_CFG_VAC_ZERO_COUNT             PFC_USER_36V_VAC_ZERO_COUNT
#define PFC_CFG_IPFC_POLARITY              PFC_USER_36V_IPFC_POLARITY
#define PFC_CFG_VAC_POLARITY               PFC_USER_36V_VAC_POLARITY
#define PFC_CFG_BRIDGE_POLARITY            PFC_USER_36V_BRIDGE_POLARITY
#else
#define PFC_CFG_PROFILE_ID                 PFC_PROFILE_CONTEST_36V
#define PFC_CFG_CALIBRATION_CONFIRMED      PFC_USER_36V_CALIBRATION_CONFIRMED
#define PFC_CFG_VAC_NOMINAL_RMS            PFC_USER_36V_VAC_NOMINAL_RMS
#define PFC_CFG_GRID_FREQUENCY_HZ          PFC_USER_36V_GRID_FREQUENCY_HZ
#define PFC_CFG_GRID_TOLERANCE_HZ          PFC_USER_36V_GRID_TOLERANCE_HZ
#define PFC_CFG_VAC_PEAK_TRIP_V            PFC_USER_36V_VAC_PEAK_TRIP_V
#define PFC_CFG_VBUS_TARGET_V              PFC_USER_36V_VBUS_TARGET_V
#define PFC_CFG_VBUS_START_MIN_V           PFC_USER_36V_VBUS_START_MIN_V
#define PFC_CFG_VBUS_RUN_MIN_V             PFC_USER_36V_VBUS_RUN_MIN_V
#define PFC_CFG_VBUS_WARN_V                PFC_USER_36V_VBUS_WARN_V
#define PFC_CFG_VBUS_TRIP_V                PFC_USER_36V_VBUS_TRIP_V
#define PFC_CFG_VBUS_TOLERANCE_V           PFC_USER_36V_VBUS_TOLERANCE_V
#define PFC_CFG_LOAD_RESISTANCE_OHM        PFC_USER_36V_LOAD_RESISTANCE_OHM
#define PFC_CFG_INPUT_INDUCTANCE_H         PFC_USER_36V_INPUT_INDUCTANCE_H
#define PFC_CFG_BUS_CAPACITANCE_F          PFC_USER_36V_BUS_CAPACITANCE_F
#define PFC_CFG_CONTROL_FREQUENCY_HZ       PFC_USER_36V_CONTROL_FREQUENCY_HZ
#define PFC_CFG_MODULATION_LIMIT           PFC_USER_36V_MODULATION_LIMIT
#define PFC_CFG_CURRENT_TARGET_A_PEAK      PFC_USER_36V_CURRENT_TARGET_A_PEAK
#define PFC_CFG_CURRENT_RAMP_A_PER_S       PFC_USER_36V_CURRENT_RAMP_A_PER_S
#define PFC_CFG_CURRENT_TRIP_A_PEAK        PFC_USER_36V_CURRENT_TRIP_A_PEAK
#define PFC_CFG_PR_KP                      PFC_USER_36V_PR_KP
#define PFC_CFG_PR_KR                      PFC_USER_36V_PR_KR
#define PFC_CFG_PR_BANDWIDTH_RAD_S         PFC_USER_36V_PR_BANDWIDTH_RAD_S
#define PFC_CFG_PR_OUTPUT_LIMIT_V          PFC_USER_36V_PR_OUTPUT_LIMIT_V
#define PFC_CFG_PI_KP_A_PER_V              PFC_USER_36V_PI_KP_A_PER_V
#define PFC_CFG_PI_KI_A_PER_VS             PFC_USER_36V_PI_KI_A_PER_VS
#define PFC_CFG_VOLTAGE_LOOP_HZ            PFC_USER_36V_VOLTAGE_LOOP_HZ
#define PFC_CFG_NOTCH_DAMPING_RAD_S        PFC_USER_36V_NOTCH_DAMPING_RAD_S
#define PFC_CFG_VBUS_SLEW_V_PER_S          PFC_USER_36V_VBUS_SLEW_V_PER_S
#define PFC_CFG_PROBE_CURRENT_A_RMS        PFC_USER_36V_PROBE_CURRENT_A_RMS
#define PFC_CFG_PROBE_SLEW_A_PER_S         PFC_USER_36V_PROBE_SLEW_A_PER_S
#define PFC_CFG_CURRENT_ERROR_MAX_A_RMS    PFC_USER_36V_CURRENT_ERROR_MAX_A_RMS
#define PFC_CFG_PROBE_MIN_MS               PFC_USER_36V_PROBE_MIN_MS
#define PFC_CFG_CURRENT_TIMEOUT_MS         PFC_USER_36V_CURRENT_TIMEOUT_MS
#define PFC_CFG_VBUS_TIMEOUT_MS            PFC_USER_36V_VBUS_TIMEOUT_MS
#define PFC_CFG_SATURATION_SAMPLES         PFC_USER_36V_SATURATION_SAMPLES
#define PFC_CFG_PERIOD_SATURATION_RATIO    PFC_USER_36V_PERIOD_SATURATION_RATIO
#define PFC_CFG_PERIOD_SATURATION_CYCLES   PFC_USER_36V_PERIOD_SATURATION_CYCLES
#define PFC_CFG_VBUS_REGULATION_TIMEOUT_MS PFC_USER_36V_VBUS_REGULATION_TIMEOUT_MS
#define PFC_CFG_IPFC_A_PER_COUNT           PFC_USER_36V_IPFC_A_PER_COUNT
#define PFC_CFG_VAC_V_PER_COUNT            PFC_USER_36V_VAC_V_PER_COUNT
#define PFC_CFG_VBUS_V_PER_COUNT           PFC_USER_36V_VBUS_V_PER_COUNT
#define PFC_CFG_IPFC_ZERO_COUNT            PFC_USER_36V_IPFC_ZERO_COUNT
#define PFC_CFG_VAC_ZERO_COUNT             PFC_USER_36V_VAC_ZERO_COUNT
#define PFC_CFG_IPFC_POLARITY              PFC_USER_36V_IPFC_POLARITY
#define PFC_CFG_VAC_POLARITY               PFC_USER_36V_VAC_POLARITY
#define PFC_CFG_BRIDGE_POLARITY            PFC_USER_36V_BRIDGE_POLARITY
#endif

/* 当前活动档的唯一常驻只读对象；字段顺序与PFC_Params声明保持一致。 */
static const PFC_Params pfc_active_params =
{
    PFC_CFG_PROFILE_ID,
    (PFC_RunMode)PFC_USER_RUN_MODE,
    PFC_CFG_CALIBRATION_CONFIRMED,
    PFC_CFG_VAC_NOMINAL_RMS, PFC_CFG_GRID_FREQUENCY_HZ,
    PFC_CFG_GRID_TOLERANCE_HZ, PFC_CFG_VAC_PEAK_TRIP_V,
    PFC_CFG_VBUS_TARGET_V, PFC_CFG_VBUS_START_MIN_V, PFC_CFG_VBUS_RUN_MIN_V,
    PFC_CFG_VBUS_WARN_V, PFC_CFG_VBUS_TRIP_V, PFC_CFG_VBUS_TOLERANCE_V,
    PFC_CFG_LOAD_RESISTANCE_OHM, PFC_CFG_INPUT_INDUCTANCE_H,
    PFC_CFG_BUS_CAPACITANCE_F, PFC_CFG_CONTROL_FREQUENCY_HZ,
    PFC_CFG_MODULATION_LIMIT, PFC_CFG_CURRENT_TARGET_A_PEAK,
    PFC_CFG_CURRENT_RAMP_A_PER_S, PFC_CFG_CURRENT_TRIP_A_PEAK,
    PFC_CFG_PR_KP, PFC_CFG_PR_KR, PFC_CFG_PR_BANDWIDTH_RAD_S,
    PFC_CFG_PR_OUTPUT_LIMIT_V, PFC_CFG_PI_KP_A_PER_V,
    PFC_CFG_PI_KI_A_PER_VS, PFC_CFG_VOLTAGE_LOOP_HZ,
    PFC_CFG_NOTCH_DAMPING_RAD_S, PFC_CFG_VBUS_SLEW_V_PER_S,
    PFC_CFG_PROBE_CURRENT_A_RMS, PFC_CFG_PROBE_SLEW_A_PER_S,
    PFC_CFG_CURRENT_ERROR_MAX_A_RMS, PFC_CFG_PROBE_MIN_MS,
    PFC_CFG_CURRENT_TIMEOUT_MS, PFC_CFG_VBUS_TIMEOUT_MS,
    PFC_CFG_SATURATION_SAMPLES, PFC_CFG_PERIOD_SATURATION_RATIO,
    PFC_CFG_PERIOD_SATURATION_CYCLES, PFC_CFG_VBUS_REGULATION_TIMEOUT_MS,
    PFC_CFG_IPFC_A_PER_COUNT, PFC_CFG_VAC_V_PER_COUNT,
    PFC_CFG_VBUS_V_PER_COUNT, PFC_CFG_IPFC_ZERO_COUNT,
    PFC_CFG_VAC_ZERO_COUNT, PFC_CFG_IPFC_POLARITY,
    PFC_CFG_VAC_POLARITY, PFC_CFG_BRIDGE_POLARITY
};

const PFC_Params *PFC_Params_GetActive(void)
{
    return &pfc_active_params;
}

uint32_t PFC_Params_GetValidationError(const PFC_Params *params)
{
    uint32_t error = PFC_PARAM_ERROR_NONE;

    if (params == 0)
    {
        return PFC_PARAM_ERROR_MODE | PFC_PARAM_ERROR_ELECTRICAL |
               PFC_PARAM_ERROR_CONTROL | PFC_PARAM_ERROR_CALIBRATION |
               PFC_PARAM_ERROR_POLARITY | PFC_PARAM_ERROR_PROTECTION;
    }

    if (((params->profile_id != PFC_PROFILE_5V_TEST) &&
         (params->profile_id != PFC_PROFILE_36V_LIGHT_100R) &&
         (params->profile_id != PFC_PROFILE_CONTEST_36V)) ||
        ((params->run_mode != PFC_RUN_MODE_RAW_ADC) &&
         (params->run_mode != PFC_RUN_MODE_ENGINEERING_CHECK) &&
         (params->run_mode != PFC_RUN_MODE_CLOSED_LOOP)))
    {
        error |= PFC_PARAM_ERROR_MODE;
    }

    if ((params->vac_nominal_rms <= 0.0f) ||
        (params->grid_frequency_hz < 45.0f) ||
        (params->grid_frequency_hz > 55.0f) ||
        (params->grid_frequency_tolerance <= 0.0f) ||
        (params->load_resistance <= 0.0f) ||
        (params->input_inductance <= 0.0f) ||
        (params->bus_capacitance <= 0.0f) ||
        (params->control_frequency < 1000.0f))
    {
        error |= PFC_PARAM_ERROR_ELECTRICAL;
    }

    if ((params->current_pr_kp < 0.0f) || (params->current_pr_kr < 0.0f) ||
        (params->current_pr_bandwidth_rad_s <= 0.0f) ||
        (params->current_pr_output_limit_v <= 0.0f) ||
        (params->voltage_pi_kp < 0.0f) || (params->voltage_pi_ki < 0.0f) ||
        (params->voltage_loop_frequency_hz <= 0.0f) ||
        (params->voltage_loop_frequency_hz > params->control_frequency) ||
        (params->voltage_loop_frequency_hz <= 4.0f * params->grid_frequency_hz) ||
        (params->vbus_notch_damping_rad_s <= 0.0f) ||
        (params->vbus_reference_slew_v_s <= 0.0f) ||
        (params->current_probe_rms <= 0.0f) ||
        (params->current_probe_rms >= params->current_command_target) ||
        (params->current_probe_slew_a_s <= 0.0f) ||
        (params->current_error_rms_max <= 0.0f) ||
        (params->current_probe_min_ms < 20U) ||
        (params->current_loop_timeout_ms <= params->current_probe_min_ms) ||
        (params->vbus_build_timeout_ms <= params->current_loop_timeout_ms) ||
        (params->saturation_trip_samples == 0U) ||
        (params->period_saturation_ratio <= 0.0f) ||
        (params->period_saturation_ratio > 1.0f) ||
        (params->period_saturation_cycles == 0U) ||
        (params->vbus_regulation_timeout_ms == 0U))
    {
        error |= PFC_PARAM_ERROR_CONTROL;
    }

    if ((params->ipfc_amp_per_count <= 0.0f) ||
        (params->vac_volt_per_count <= 0.0f) ||
        (params->vbus_volt_per_count <= 0.0f) ||
        (params->ipfc_zero_count > 4095U) || (params->vac_zero_count > 4095U))
    {
        error |= PFC_PARAM_ERROR_CALIBRATION;
    }

    if (((params->ipfc_polarity != 1) && (params->ipfc_polarity != -1)) ||
        ((params->vac_polarity != 1) && (params->vac_polarity != -1)) ||
        ((params->bridge_polarity != 1) && (params->bridge_polarity != -1)))
    {
        error |= PFC_PARAM_ERROR_POLARITY;
    }

    if ((params->vac_peak_trip <= params->vac_nominal_rms) ||
        (params->vbus_run_min <= 0.0f) ||
        (params->vbus_start_min < params->vbus_run_min) ||
        (params->vbus_target <= params->vbus_start_min) ||
        (params->vbus_overvoltage_warn <= params->vbus_target) ||
        (params->vbus_overvoltage_trip <= params->vbus_overvoltage_warn) ||
        (params->vbus_target_tolerance <= 0.0f) ||
        (params->modulation_limit <= 0.0f) ||
        (params->modulation_limit > 0.95f) ||
        (params->current_command_target <= 0.0f) ||
        (params->current_ramp_a_per_s <= 0.0f) ||
        (params->current_trip <= params->current_command_target))
    {
        error |= PFC_PARAM_ERROR_PROTECTION;
    }
    return error;
}

uint8_t PFC_Params_Validate(const PFC_Params *params)
{
    return (PFC_Params_GetValidationError(params) == PFC_PARAM_ERROR_NONE) ? 1U : 0U;
}

uint8_t PFC_Params_CalibrationReady(const PFC_Params *params)
{
    return (uint8_t)((PFC_Params_Validate(params) != 0U) &&
                     (params->calibration_confirmed != 0U));
}

uint8_t PFC_Params_PowerAllowed(const PFC_Params *params)
{
    if (params == 0)
    {
        return 0U;
    }
    return (uint8_t)((PFC_Params_CalibrationReady(params) != 0U) &&
                     (params->run_mode == PFC_RUN_MODE_CLOSED_LOOP));
}
