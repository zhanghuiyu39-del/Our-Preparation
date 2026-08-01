#include "pfc_app.h"
#include "SPWM.h"
#include "pfc_control.h"
#include "pfc_hrtim.h"
#include "main.h"

/*
 * 本模块负责PD0人工启停、双闭环阶段迁移和功率输出许可，不负责ADC换算或PI/PR计算。
 * 数据流：1 ms主循环 -> 按键消抖/状态机 -> HRTIM输出启停；
 *           ADC1完整DMA ISR -> PFC_AppFastStep() -> PR/PI控制和Compare更新。
 *
 * 当前驱动板没有MCU可控Gate Enable。Master/A/B计数器只建立ADC触发时基，
 * HAL_HRTIM_WaveformOutputStart()开放PA8~PA11后即可能驱动功率器件，二者必须区分。
 */
/* PD0低有效按键映射；端口和引脚宏来自CubeMX生成的main.h。 */
#define PFC_KEY_GPIO_PORT               START_KEY_GPIO_Port
#define PFC_KEY_GPIO_PIN                START_KEY_Pin

/* 按键时间均以1 ms状态机调用为计时基准。 */
#define PFC_KEY_DEBOUNCE_MS             30U   /* 按下或释放连续稳定时间，单位ms。 */
#define PFC_KEY_SHORT_MIN_MS            50U   /* 有效短按的最短稳定按下时间，单位ms。 */
#define PFC_KEY_SHORT_MAX_MS            1000U /* 有效短按的最长稳定按下时间，单位ms。 */

/* 状态迁移等待时间，单位ms；只由PFC_AppTick1ms()使用。 */
#define PFC_ADC_CHECK_TIME_MS           200U
#define PFC_PRECHARGE_STABLE_MS         500U
/* VAC有效值相对额定值的允许偏差，0.10表示正负10%。 */
#define PFC_VAC_RMS_TOLERANCE           0.10f

static const PFC_Params *app_params = 0; /* 初始化后只读，参数对象必须常驻。 */
/* 当前PFC状态：主循环状态机写，ADC ISR故障入口也可能改为FAULT_LATCH。 */
static volatile PFC_State app_state = PFC_SAFE;

/* 状态驻留时间，单位ms；PFC_AppTick1ms()每次加1，状态迁移时清零。 */
static uint32_t app_state_time_ms = 0U;
/* 被动预充条件连续成立时间，单位ms；只在PASSIVE_PRECHARGE状态累计。 */
static uint32_t app_precharge_stable_ms = 0U;

/* 上一次100 ms IWDG健康检查读取的序列/心跳，由主循环监督函数独占更新。 */
static uint32_t app_last_adc1_sequence = 0U;
static uint32_t app_last_adc2_sequence = 0U;
static uint32_t app_last_fast_heartbeat = 0U;
/* 上一次IWDG监督读取的闭环心跳；只在带功率状态要求其持续前进。 */
static uint32_t app_last_control_heartbeat = 0U;

/* 复位来源锁存值：1表示本次上电入口检测到IWDG复位，仅初始化时写。 */
static uint8_t app_iwdg_reset_seen = 0U;

/* PD0消抖状态均由1 ms主循环写；1表示按下，0表示释放。 */
static uint8_t key_raw_last = 0U;        /* 最近一次GPIO原始采样。 */
static uint8_t key_stable = 0U;          /* 已通过30 ms消抖的稳定状态。 */
static uint8_t key_startup_unlocked = 0U;/* 上电后已确认连续释放30 ms。 */
static uint8_t key_ready_armed = 0U;     /* 进入READY后已确认按键处于释放态。 */
static uint16_t key_release_ms = 0U;     /* 上电解锁前连续释放时间，单位ms。 */
static uint16_t key_debounce_ms = 0U;    /* 当前原始状态连续保持时间，单位ms。 */
static uint16_t key_pressed_ms = 0U;     /* 消抖后稳定按下时间，单位ms，饱和到65535。 */

/**
 * @brief  判断标定、测量、VAC同步和被动预充条件是否同时成立。
 * @param  measurement 由测量模块发布的一致性快照地址。
 * @retval 1表示允许进入或保持READY，0表示至少一项启动条件不满足。
 * @note   只读取参数和快照，不操作HRTIM输出，也不锁存故障。
 */
static uint8_t PFC_AppInputReady(const PFC_Measurement *measurement)
{
#if PFC_USER_RELAXED_PWM_TEST != 0U
    /* 放宽模式只要求快照、标定和参数许可成立；VAC/VBUS门槛由OLED/VOFA记录。 */
    return (uint8_t)((measurement != 0) && (app_params != 0) &&
                     (PFC_Params_PowerAllowed(app_params) != 0U) &&
                     (measurement->valid != 0U));
#else
    float vac_min;
    float vac_max;
    float frequency_min;
    float frequency_max;

    /* 上下限均由活动参数档计算，电压单位V RMS，频率单位Hz。 */

    if ((measurement == 0) || (app_params == 0) ||
        (PFC_Params_PowerAllowed(app_params) == 0U))
    {
        return 0U;
    }

    vac_min = app_params->vac_nominal_rms * (1.0f - PFC_VAC_RMS_TOLERANCE);
    vac_max = app_params->vac_nominal_rms * (1.0f + PFC_VAC_RMS_TOLERANCE);
    frequency_min = app_params->grid_frequency_hz - app_params->grid_frequency_tolerance;
    frequency_max = app_params->grid_frequency_hz + app_params->grid_frequency_tolerance;

    return (uint8_t)((measurement->valid != 0U) &&
                     (measurement->vac_locked != 0U) &&
                     (measurement->vac_rms >= vac_min) &&
                     (measurement->vac_rms <= vac_max) &&
                     (measurement->vac_frequency_hz >= frequency_min) &&
                     (measurement->vac_frequency_hz <= frequency_max) &&
                     (measurement->vbus >= app_params->vbus_start_min));
#endif
}

/**
 * @brief 读取低有效PD0并在释放沿生成一次短按事件。
 * @retval 1表示本次1 ms调用确认了一次50~1000 ms短按释放事件，否则返回0。
 * @note  上电后必须先连续释放30 ms才解锁；按住按键上电再释放不会产生启动事件。
 */
static uint8_t PFC_AppReadShortPress(void)
{
    /* raw_pressed是本次GPIO采样；short_press只在有效释放沿保持一个1 ms调用周期。 */
    uint8_t raw_pressed =
        (HAL_GPIO_ReadPin(PFC_KEY_GPIO_PORT, PFC_KEY_GPIO_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
    uint8_t short_press = 0U;

    if (key_startup_unlocked == 0U)
    {
        if (raw_pressed == 0U)
        {
            if (key_release_ms < PFC_KEY_DEBOUNCE_MS)
            {
                key_release_ms++;
            }
            if (key_release_ms >= PFC_KEY_DEBOUNCE_MS)
            {
                key_startup_unlocked = 1U;
                key_raw_last = 0U;
                key_stable = 0U;
                key_debounce_ms = 0U;
                key_pressed_ms = 0U;
            }
        }
        else
        {
            key_release_ms = 0U;
        }
        return 0U;
    }

    if (raw_pressed != key_raw_last)
    {
        key_raw_last = raw_pressed;
        key_debounce_ms = 0U;
    }
    else if (key_debounce_ms < PFC_KEY_DEBOUNCE_MS)
    {
        key_debounce_ms++;
        if (key_debounce_ms == PFC_KEY_DEBOUNCE_MS)
        {
            uint8_t old_stable = key_stable;
            key_stable = raw_pressed;
            if (key_stable != 0U)
            {
                key_pressed_ms = 0U;
            }
            else if (old_stable != 0U)
            {
                if ((key_pressed_ms >= PFC_KEY_SHORT_MIN_MS) &&
                    (key_pressed_ms <= PFC_KEY_SHORT_MAX_MS))
                {
                    short_press = 1U;
                }
                key_pressed_ms = 0U;
            }
        }
    }

    if ((key_stable != 0U) && (key_pressed_ms < 0xFFFFU))
    {
        key_pressed_ms++;
    }
    return short_press;
}

/**
 * @brief  进入READY并要求在READY之后重新完成一次按下-释放周期。
 * @note   仅由1 ms状态机调用，不开放HRTIM输出。
 */
static void PFC_AppEnterReady(void)
{
    app_state = PFC_READY;
    app_state_time_ms = 0U;
    key_ready_armed = 0U;
}

/**
 * @brief  人工停机并复位PI、PR、陷波器和调制状态。
 * @note   先关闭四路HRTIM输出，再复位控制器；Master/A/B计数器继续为ADC提供触发。
 */
static void PFC_AppStopPower(void)
{
    PFC_HRTIM_StopPower();
    PFC_Control_Reset();
}

/**
 * @brief  复核许可条件并进入PRIME，等待10 kHz路径在正向过零同步开放输出。
 * @param  measurement 当前1 ms状态机读取的一致性测量快照。
 * @note   仅在READY短按事件后调用；失败时锁存故障，不进行自动重试。
 */
static void PFC_AppStartPower(const PFC_Measurement *measurement)
{
    if ((PFC_AppInputReady(measurement) == 0U) ||
        (PFC_Measure_GetFault() != PFC_FAULT_NONE))
    {
        return;
    }

    if (PFC_Control_EnterMode(PFC_CONTROL_PRIME, measurement) != HAL_OK)
    {
        PFC_AppTrip(PFC_FAULT_CONTROL | PFC_FAULT_MODULATION);
        return;
    }
    /* A/B此时仍关闭；INV的C/D/E不受PRIME等待影响。 */
    app_state = PFC_PWM_PRIME;
    app_state_time_ms = 0U;
}

/**
 * @brief  初始化PFC状态、PD0消抖、IWDG复位记录和PI+PR控制模块。
 * @param  params 常驻只读参数档地址，初始化后必须始终有效。
 * @param  iwdg_reset_seen 1表示本次启动来自IWDG复位，0表示其他复位来源。
 * @note   在ADC/DMA启动前由main调用；会关闭HRTIM输出并写入中性Compare。
 */
void PFC_AppInit(const PFC_Params *params, uint8_t iwdg_reset_seen)
{
    app_params = params;
    app_iwdg_reset_seen = iwdg_reset_seen;
    app_state = PFC_SAFE;
    app_state_time_ms = 0U;
    app_precharge_stable_ms = 0U;
    app_last_adc1_sequence = 0U;
    app_last_adc2_sequence = 0U;
    app_last_fast_heartbeat = 0U;
    app_last_control_heartbeat = 0U;
    key_raw_last = 0U;
    key_stable = 0U;
    key_startup_unlocked = 0U;
    key_ready_armed = 0U;
    key_release_ms = 0U;
    key_debounce_ms = 0U;
    key_pressed_ms = 0U;
    PFC_HRTIM_StopPower();
    if ((PFC_Params_PowerAllowed(params) != 0U) &&
        (PFC_Control_Init(params) != HAL_OK))
    {
        PFC_AppTrip(PFC_FAULT_PARAM | PFC_FAULT_MODULATION);
    }
}

/**
 * @brief  使用最新一致性测量完成一次10 kHz PR内环及必要的1 kHz PI外环。
 * @note   只由ADC1完整DMA回调以10 kHz调用；禁止阻塞、OLED、USART和HAL_Delay()。
 */
void PFC_AppFastStep(void)
{
    PFC_Measurement measurement;

    PFC_Measure_GetSnapshot(&measurement);
    if ((measurement.valid == 0U) || (measurement.fault_bits != PFC_FAULT_NONE))
    {
        return;
    }
    if (app_state == PFC_PWM_PRIME)
    {
        PFC_ControlTelemetry telemetry;

        if (PFC_Control_Step10k(&measurement) != HAL_OK)
        {
            PFC_Control_GetTelemetry(&telemetry);
            PFC_AppTrip((telemetry.fault_bits != 0U) ? telemetry.fault_bits :
                                                       PFC_FAULT_CONTROL);
            return;
        }
        PFC_Control_GetTelemetry(&telemetry);
        if (telemetry.prime_waiting == 0U)
        {
            /*
             * 先提交本帧VAC/VBUS前馈Compare，再切入零电流PR斜坡并开放A/B。
             * 这样不会把旧的50%中性Compare暴露在36 V交流峰值附近。
             */
            if ((PFC_HRTIM_CommitCompare() != HAL_OK) ||
                (PFC_Control_EnterMode(PFC_CONTROL_CURRENT_RAMP,
                                       &measurement) != HAL_OK) ||
                (PFC_HRTIM_StartOutputs() != HAL_OK))
            {
                PFC_AppTrip(PFC_FAULT_HRTIM | PFC_FAULT_MODULATION);
                return;
            }
            app_state = PFC_CURRENT_LOOP_RAMP;
            app_state_time_ms = 0U;
        }
    }
    else if ((app_state == PFC_CURRENT_LOOP_RAMP) ||
        (app_state == PFC_VBUS_LOOP_RAMP) ||
        (app_state == PFC_VBUS_LOOP_RUN))
    {
        if (PFC_Control_Step10k(&measurement) != HAL_OK)
        {
            PFC_ControlTelemetry telemetry;
            PFC_Control_GetTelemetry(&telemetry);
            PFC_AppTrip((telemetry.fault_bits != 0U) ? telemetry.fault_bits :
                                                       PFC_FAULT_CONTROL);
        }
    }
}

/**
 * @brief  执行PD0消抖、启动许可检查和PFC状态迁移。
 * @note   由主循环按1 ms节拍调用；允许操作HRTIM启停，但不执行10 kHz调制计算。
 */
static void PFC_AppTick1msInternal(uint8_t short_press)
{
    PFC_Measurement measurement;
    /* 快照在本次状态判断期间保持不变；short_press仅对本次调用有效。 */

    PFC_Measure_GetSnapshot(&measurement);
    app_state_time_ms++;

    if (measurement.fault_bits != PFC_FAULT_NONE)
    {
        if (app_state != PFC_FAULT_LATCH)
        {
            PFC_AppStopPower();
        }
        app_state = PFC_FAULT_LATCH;
        return;
    }

    switch (app_state)
    {
    case PFC_SAFE:
        if (app_state_time_ms >= ((app_iwdg_reset_seen != 0U) ? 1000U : 100U))
        {
            if ((app_params != 0) &&
                (app_params->run_mode == PFC_RUN_MODE_RAW_ADC))
            {
                app_state = PFC_RAW_ADC_CALIBRATION;
            }
            else if ((app_params != 0) &&
                     (app_params->run_mode == PFC_RUN_MODE_ENGINEERING_CHECK) &&
                     (PFC_Params_CalibrationReady(app_params) != 0U))
            {
                app_state = PFC_ENGINEERING_CHECK;
            }
            else if ((PFC_Params_PowerAllowed(app_params) != 0U) &&
                     (measurement.offset_ready != 0U))
            {
                app_state = PFC_ADC_CHECK;
            }
            else
            {
                PFC_AppTrip(PFC_FAULT_PARAM);
                break;
            }
            app_state_time_ms = 0U;
        }
        break;

    case PFC_RAW_ADC_CALIBRATION:
        /* 只保持ADC统计、显示和通信；PD0事件在该模式下被明确忽略。 */
        PFC_HRTIM_StopPower();
        break;

    case PFC_ENGINEERING_CHECK:
        /* 工程量与保护阈值可复核，但任何PD0操作都不能开放PWM。 */
        PFC_HRTIM_StopPower();
        break;

    case PFC_ADC_CHECK:
        if ((measurement.valid != 0U) &&
            (measurement.raw_valid != 0U) &&
            (app_state_time_ms >= PFC_ADC_CHECK_TIME_MS))
        {
            app_state = PFC_PASSIVE_PRECHARGE;
            app_state_time_ms = 0U;
        }
        break;

    case PFC_PASSIVE_PRECHARGE:
        if (PFC_AppInputReady(&measurement) != 0U)
        {
            if (++app_precharge_stable_ms >= PFC_PRECHARGE_STABLE_MS)
            {
                PFC_AppEnterReady();
            }
        }
        else
        {
            app_precharge_stable_ms = 0U;
        }
        break;

    case PFC_READY:
        if (PFC_AppInputReady(&measurement) == 0U)
        {
            app_state = PFC_PASSIVE_PRECHARGE;
            app_precharge_stable_ms = 0U;
            app_state_time_ms = 0U;
            key_ready_armed = 0U;
        }
        else if (key_ready_armed == 0U)
        {
            /* 若进入READY时按键仍被按住，必须先释放，随后重新短按才允许启动。 */
            if (key_stable == 0U)
            {
                key_ready_armed = 1U;
            }
        }
        else if (short_press != 0U)
        {
            PFC_AppStartPower(&measurement);
        }
        break;

    case PFC_PWM_PRIME:
        if (short_press != 0U)
        {
            PFC_AppStopPower();
            app_state = PFC_STOP;
            app_state_time_ms = 0U;
        }
        break;

    case PFC_CURRENT_LOOP_RAMP:
        if (short_press != 0U)
        {
            PFC_AppStopPower();
            app_state = PFC_STOP;
            app_state_time_ms = 0U;
        }
        else
        {
            PFC_ControlTelemetry telemetry;
            PFC_Control_GetTelemetry(&telemetry);
#if PFC_USER_RELAXED_PWM_TEST != 0U
            if ((telemetry.current_loop_qualified != 0U) ||
                (app_state_time_ms >= app_params->current_probe_min_ms))
#else
            if (telemetry.current_loop_qualified != 0U)
#endif
            {
                if (PFC_Control_EnterMode(PFC_CONTROL_VBUS_RAMP,
                                          &measurement) != HAL_OK)
                {
                    PFC_AppTrip(PFC_FAULT_CONTROL);
                }
                else
                {
                    app_state = PFC_VBUS_LOOP_RAMP;
                    app_state_time_ms = 0U;
                }
            }
            else if (app_state_time_ms >= app_params->current_loop_timeout_ms)
            {
#if PFC_USER_RELAXED_PWM_TEST == 0U
                PFC_AppTrip(PFC_FAULT_CURRENT_TRACKING);
#endif
            }
        }
        break;

    case PFC_VBUS_LOOP_RAMP:
        if (short_press != 0U)
        {
            PFC_AppStopPower();
            app_state = PFC_STOP;
            app_state_time_ms = 0U;
        }
        else
        {
            PFC_ControlTelemetry telemetry;
            PFC_Control_GetTelemetry(&telemetry);
            if ((telemetry.vbus_reference_reached != 0U)
#if PFC_USER_RELAXED_PWM_TEST == 0U
                && (measurement.vbus >=
                    (app_params->vbus_target - app_params->vbus_target_tolerance))
                && (measurement.vbus <=
                    (app_params->vbus_target + app_params->vbus_target_tolerance))
#endif
               )
            {
                if (PFC_Control_EnterMode(PFC_CONTROL_VBUS_RUN,
                                          &measurement) != HAL_OK)
                {
                    PFC_AppTrip(PFC_FAULT_CONTROL);
                }
                else
                {
                    app_state = PFC_VBUS_LOOP_RUN;
                    app_state_time_ms = 0U;
                }
            }
            else if (app_state_time_ms >= app_params->vbus_build_timeout_ms)
            {
#if PFC_USER_RELAXED_PWM_TEST == 0U
                PFC_AppTrip(PFC_FAULT_TARGET_TIMEOUT);
#endif
            }
        }
        break;

    case PFC_VBUS_LOOP_RUN:
        if (short_press != 0U)
        {
            PFC_AppStopPower();
            app_state = PFC_STOP;
            app_state_time_ms = 0U;
        }
#if PFC_USER_RELAXED_PWM_TEST == 0U
        else if ((measurement.vac_locked == 0U) ||
                 (measurement.vbus < app_params->vbus_run_min))
        {
            PFC_AppTrip(PFC_FAULT_VAC_LOST);
        }
#endif
        break;

    case PFC_STOP:
        if (app_state_time_ms >= 100U)
        {
            if (PFC_AppInputReady(&measurement) != 0U)
            {
                PFC_AppEnterReady();
            }
            else
            {
                app_state = PFC_PASSIVE_PRECHARGE;
                app_state_time_ms = 0U;
                app_precharge_stable_ms = 0U;
            }
        }
        break;

    case PFC_FAULT_LATCH:
        break;

    default:
        PFC_AppTrip(PFC_FAULT_STATE);
        break;
    }
}

void PFC_AppTick1msWithCommand(uint8_t short_press)
{
    PFC_AppTick1msInternal((short_press != 0U) ? 1U : 0U);
}

void PFC_AppTick1ms(void)
{
    PFC_AppTick1msInternal(PFC_AppReadShortPress());
}

/**
 * @brief  锁存一个或多个PFC故障并立即进入FAULT_LATCH。
 * @param  fault_bits 一个或多个PFC_FaultMask位。
 * @note   可从主循环或ISR调用；底层关断不阻塞，故障后不允许软件自动恢复。
 */
void PFC_AppTrip(uint32_t fault_bits)
{
    PFC_Measure_Trip(fault_bits);
    PFC_Control_Reset();
    app_state = PFC_FAULT_LATCH;
}

/**
 * @brief  读取当前PFC状态。
 * @retval 当前PFC_State；供显示、VOFA和安全监督只读。
 */
PFC_State PFC_AppGetState(void)
{
    return app_state;
}

/**
 * @brief  检查当前参数档是否已通过完整板级标定和范围校验。
 * @retval 1表示允许继续评估带功率条件，0表示只能运行采样标定流程。
 */
uint8_t PFC_AppCalibrationValid(void)
{
    return PFC_Params_CalibrationReady(app_params);
}

/**
 * @brief  检查最近监督窗口内的ADC、控制心跳、状态机和HRTIM软件状态一致性。
 * @retval 1表示本次允许刷新IWDG，0表示至少一个健康条件不成立。
 * @note   主循环每100 ms调用一次；函数会更新内部基线，不能在同一周期重复调用。
 */
uint8_t PFC_AppWatchdogHealthy(void)
{
    PFC_Measurement measurement;
    PFC_ControlTelemetry control;
    uint8_t power_state; /* 1表示状态机当前要求PWM输出开放。 */
    uint8_t healthy;     /* 本次所有监督条件的合并结果。 */

    PFC_Measure_GetSnapshot(&measurement);
    PFC_Control_GetTelemetry(&control);
    power_state = (uint8_t)((app_state == PFC_CURRENT_LOOP_RAMP) ||
                            (app_state == PFC_VBUS_LOOP_RAMP) ||
                            (app_state == PFC_VBUS_LOOP_RUN));
    healthy = (uint8_t)((measurement.adc1_sequence != app_last_adc1_sequence) &&
                        (measurement.adc2_sequence != app_last_adc2_sequence) &&
                        (measurement.fast_heartbeat != app_last_fast_heartbeat) &&
                        (measurement.raw_valid != 0U) &&
                        (measurement.fault_bits == PFC_FAULT_NONE) &&
                        (app_state != PFC_FAULT_LATCH) &&
                        (PFC_HRTIM_CountersStarted() != 0U) &&
                        (PFC_HRTIM_OutputsEnabled() == power_state) &&
                        ((power_state == 0U) ||
                         ((control.fast_heartbeat != app_last_control_heartbeat) &&
                          (PFC_Control_IsHealthy() != 0U))));
    app_last_adc1_sequence = measurement.adc1_sequence;
    app_last_adc2_sequence = measurement.adc2_sequence;
    app_last_fast_heartbeat = measurement.fast_heartbeat;
    app_last_control_heartbeat = control.fast_heartbeat;
    return healthy;
}
