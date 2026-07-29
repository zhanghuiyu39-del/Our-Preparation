#include "pfc_params.h"

/*
 * 本模块只提供编译期只读参数档和一致性检查，不负责在线标定或Flash存储。
 * 数据流：活动参数档 -> 测量标定/AWD -> PI+PR控制 -> 状态机和SPWM执行层。
 *
 * 修改活动档只改下面一行。赛题档目前故意保持calibration_valid=0，防止把仿真
 * 参数直接用于实物。5 V档中的ADC比例同样是占位值；完成实测前不得改确认标志。
 */
/*
 * 编译期活动档选择，运行中不可切换：0=5 V低压档，1=36 V赛题预留档。
 * 这里必须使用数值宏，不能使用enum名字参与#if，否则预处理器会把未知标识符当作0。
 */
#define PFC_ACTIVE_PROFILE_ID  0U

#if PFC_ACTIVE_PROFILE_ID == 0U
/* 5 V试验档常驻只读对象；仅本文件持有，外部通过PFC_Params_GetActive()取得地址。 */
static const PFC_Params pfc_5v_test_params =
{
    /* 参数档：5 V RMS、50 Hz隔离交流输入。 */
    PFC_PROFILE_5V_TEST,
    /* VAC额定值/频率/频率容差/严重峰值越界，单位依次为V RMS、Hz、Hz、V peak。 */
    5.0f, 50.0f, 3.0f, 8.5f,
    /* VBUS目标、启动/运行门槛、告警/关断及目标容差，单位均为V。 */
    9.0f, 5.0f, 4.0f, 9.8f, 10.5f, 0.5f,
    /* 负载ohm、输入电感H、母线电容F和控制频率Hz。 */
    30.0f, 470.0e-6f, 4700.0e-6f, 10000.0f,
    /* 调制上限、电流峰值目标A、爬升A/s和软件过流A peak。 */
    0.90f, 0.80f, 0.50f, 1.20f,

    /* 10 kHz PR：Kp、Kr、Wc(rad/s)和输出限幅(V)。 */
    0.50f, 5.0f, 5.0f, 3.0f,
    /* 1 kHz母线PI：Kp(A/V)、Ki(A/(V*s))、调用频率和陷波阻尼(rad/s)。 */
    0.05f, 1.0f, 1000.0f, 50.0f,
    /* VBUS参考斜率、探测电流、探测斜率和电流误差RMS门槛。 */
    1.0f, 0.20f, 5.0f, 0.15f,
    /* 电流环最短观察/超时、母线建立超时(ms)和连续限幅样本数。 */
    60U, 500U, 8000U, 200U,

	
	//需要手动输入
	
    0.001f, 0.010f, 0.010f,     /* 低压调试占位比例：IPFC A/count、VAC V/count、VBUS V/count。 */
   
    2048U, 2048U,        /* 占位零点：IPFC count、VAC count。 */

    1, 1, 1,            /* 占位极性：IPFC、VAC、全桥调制方向，只允许+1或-1。 */
   
    0U                  /* 0表示以上比例、零点、极性和阈值尚未实测确认，必须禁止功率输出。 */
	
	

	
};
#else
/* 36 V赛题预留档常驻只读对象；当前标定无效，不能进入带功率状态。 */
static const PFC_Params pfc_contest_36v_params =
{
    /* 赛题档参数骨架；当前只用于保留接口，禁止直接用于实物。 */
    PFC_PROFILE_CONTEST_36V,
    /* VAC额定值/频率/频率容差/严重峰值越界，单位依次为V RMS、Hz、Hz、V peak。 */
    36.0f, 50.0f, 3.0f, 55.0f,
    /* VBUS目标、启动/运行门槛、告警/关断及目标容差，单位均为V。 */
    60.0f, 45.0f, 40.0f, 64.0f, 66.0f, 1.0f,
    /* 负载ohm、输入电感H、母线电容F和控制频率Hz。 */
    32.4f, 470.0e-6f, 4700.0e-6f, 10000.0f,
    /* 调制上限、电流峰值目标A、爬升A/s和软件过流A peak。 */
    0.90f, 4.80f, 1.00f, 5.20f,

    /* 赛题仿真增益骨架；完成36 V实测前仍由calibration_valid禁止带功率。 */
    5.0f, 20.0f, 5.0f, 80.0f,
    0.30f, 15.0f, 1000.0f, 50.0f,
    5.0f, 0.50f, 10.0f, 0.50f,
    60U, 500U, 8000U, 200U,

    /* 赛题档占位比例：模拟前端量程和保护链确认后重新标定。 */
    0.001f, 0.020f, 0.020f,
    /* 占位零点与极性，不能由5 V档直接复制。 */
    2048U, 2048U,
    1, 1, 1,
    /* 保持0，直至36 V档所有板级参数和保护阈值完成实测。 */
    0U
};
#endif

/**
 * @brief  返回当前由PFC_ACTIVE_PROFILE_ID选择的只读参数档。
 * @retval 常驻静态参数地址，不会返回NULL。
 * @note   不执行运行期切换，也不代表该参数档已经通过板级标定。
 */
const PFC_Params *PFC_Params_GetActive(void)
{
#if PFC_ACTIVE_PROFILE_ID == 1U
    return &pfc_contest_36v_params;
#else
    return &pfc_5v_test_params;
#endif
}

/**
 * @brief  检查板级标定确认标志以及所有会影响控制和保护的参数范围。
 * @param  params 待检查的常驻PFC参数地址。
 * @retval 1表示参数可参与带功率状态判断，0表示必须停留在安全标定流程。
 * @note   本函数只验证数值自洽性，不能替代示波器、万用表和板级保护实测。
 */
uint8_t PFC_Params_IsValid(const PFC_Params *params)
{
    if ((params == 0) || (params->calibration_valid == 0U))
    {
        return 0U;
    }

    if (((params->profile_id != PFC_PROFILE_5V_TEST) &&
         (params->profile_id != PFC_PROFILE_CONTEST_36V)) ||
        (params->vac_nominal_rms <= 0.0f) ||
        (params->grid_frequency_hz < 45.0f) ||
        (params->grid_frequency_hz > 55.0f) ||
        (params->grid_frequency_tolerance <= 0.0f) ||
        (params->vac_peak_trip <= params->vac_nominal_rms) ||
        (params->vbus_run_min <= 0.0f) ||
        (params->vbus_start_min < params->vbus_run_min) ||
        (params->vbus_target <= params->vbus_start_min) ||
        (params->vbus_overvoltage_warn <= params->vbus_target) ||
        (params->vbus_overvoltage_trip <= params->vbus_overvoltage_warn) ||
        (params->vbus_target_tolerance <= 0.0f) ||
        (params->load_resistance <= 0.0f) ||
        (params->input_inductance <= 0.0f) ||
        (params->bus_capacitance <= 0.0f) ||
        (params->control_frequency < 1000.0f) ||
        (params->modulation_limit <= 0.0f) ||
        (params->modulation_limit > 0.95f) ||
        (params->current_command_target <= 0.0f) ||
        (params->current_ramp_a_per_s <= 0.0f) ||
        (params->current_trip <= params->current_command_target) ||
        (params->current_pr_kp < 0.0f) ||
        (params->current_pr_kr < 0.0f) ||
        (params->current_pr_bandwidth_rad_s <= 0.0f) ||
        (params->current_pr_output_limit_v <= 0.0f) ||
        (params->voltage_pi_kp < 0.0f) ||
        (params->voltage_pi_ki < 0.0f) ||
        (params->voltage_loop_frequency_hz <= 0.0f) ||
        (params->voltage_loop_frequency_hz > params->control_frequency) ||
        (params->voltage_loop_frequency_hz <=
         (4.0f * params->grid_frequency_hz)) ||
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
        (params->ipfc_amp_per_count <= 0.0f) ||
        (params->vac_volt_per_count <= 0.0f) ||
        (params->vbus_volt_per_count <= 0.0f) ||
        (params->ipfc_zero_count < 64U) ||
        (params->ipfc_zero_count > 4031U) ||
        (params->vac_zero_count < 64U) ||
        (params->vac_zero_count > 4031U) ||
        ((params->ipfc_polarity != 1) && (params->ipfc_polarity != -1)) ||
        ((params->vac_polarity != 1) && (params->vac_polarity != -1)) ||
        ((params->bridge_polarity != 1) && (params->bridge_polarity != -1)))
    {
        return 0U;
    }

    return 1U;
}
