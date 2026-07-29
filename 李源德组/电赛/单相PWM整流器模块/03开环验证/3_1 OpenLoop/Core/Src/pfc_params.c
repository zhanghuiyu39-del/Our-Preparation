#include "pfc_params.h"

/*
 * 本模块只负责提供只读参数档和执行参数一致性检查，不负责在线标定或Flash存储。
 * 数据流：编译期参数档 -> PFC_Params_GetActive() -> 测量/状态机/SPWM只读使用。
 *
 * 下列ADC比例和零点仍是低压调试占位值，因此calibration_valid故意保持0。
 * 完成万用表/示波器标定、核对极性及保护量程后，填写实测值并改为1；在此之前
 * 系统只允许采样、OLED和VOFA调试，不允许PE0拉高。
 */
static const PFC_Params pfc_5v_test_params =
{
    PFC_PROFILE_5V_TEST,
    5.0f, 50.0f,
    9.0f, 6.0f, 10.0f, 10.5f,
    30.0f, 470.0e-6f, 4700.0e-6f,
    10000.0f, 0.90f,
    0.15f, 0.80f, 1.20f,
    0.001f, 0.010f, 0.010f,
    2048U, 2048U,
    1, 1, 1,
    0U
};

const PFC_Params *PFC_Params_GetActive(void)
{
    return &pfc_5v_test_params;
}

uint8_t PFC_Params_IsValid(const PFC_Params *params)
{
    if ((params == 0) || (params->calibration_valid == 0U))
    {
        return 0U;
    }

    if ((params->profile_id != PFC_PROFILE_5V_TEST) ||
        (params->vac_nominal_rms <= 0.0f) ||
        (params->grid_frequency_hz < 45.0f) ||
        (params->grid_frequency_hz > 65.0f) ||
        (params->vbus_start_min <= 0.0f) ||
        (params->vbus_target <= params->vbus_start_min) ||
        (params->input_inductance <= 0.0f) ||
        (params->bus_capacitance <= 0.0f) ||
        (params->ipfc_amp_per_count <= 0.0f) ||
        (params->vac_volt_per_count <= 0.0f) ||
        (params->vbus_volt_per_count <= 0.0f) ||
        (params->ipfc_zero_count < 64U) ||
        (params->ipfc_zero_count > 4031U) ||
        (params->vac_zero_count < 64U) ||
        (params->vac_zero_count > 4031U) ||
        ((params->ipfc_polarity != 1) && (params->ipfc_polarity != -1)) ||
        ((params->vac_polarity != 1) && (params->vac_polarity != -1)) ||
        ((params->bridge_polarity != 1) && (params->bridge_polarity != -1)) ||
        (params->modulation_limit <= 0.0f) ||
        (params->modulation_limit > 0.95f) ||
        (params->current_command_start <= 0.0f) ||
        (params->current_command_max < params->current_command_start) ||
        (params->current_trip <= params->current_command_max) ||
        (params->vbus_overvoltage_trip <= params->vbus_target) ||
        (params->control_frequency < 1000.0f))
    {
        return 0U;
    }

    return 1U;
}
