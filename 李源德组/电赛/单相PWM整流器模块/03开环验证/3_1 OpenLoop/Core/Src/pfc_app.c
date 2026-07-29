#include "pfc_app.h"
#include "SPWM.h"
#include "pfc_hrtim.h"
#include "main.h"

/*
 * 本模块负责人工启停、状态迁移、启动条件和Gate Enable顺序，不负责ADC换算和调制计算。
 * 数据流如下：
 * 1 ms主循环 -> PD0消抖/状态机 -> PWM输出与PE0启停
 * ADC1 DMA ISR -> PFC_AppFastStep() -> VAC同步与开环Compare更新
 * 驱动器nFAULT -> PB10/HRTIM Fault 3先硬件关断 -> ISR随后锁存软件故障
 *
 * “HRTIM计数器运行”“四路MCU PWM输出开放”“PE0 Gate Enable拉高”是三个独立状态。
 * PFC_ADC_CHECK阶段只运行计数器产生ADC触发；只有PFC_OPEN_LOOP_RAMP/RUN允许PE0为高。
 */
#define PFC_KEY_GPIO_PORT              PFC_START_KEY_GPIO_Port
#define PFC_KEY_GPIO_PIN               PFC_START_KEY_Pin
#define PFC_KEY_DEBOUNCE_MS            30U    /* 原始电平连续稳定30 ms才承认改变。 */
#define PFC_KEY_SHORT_MIN_MS           50U    /* 释放时按下时间位于50~1000 ms才生成一次短按事件。 */
#define PFC_KEY_SHORT_MAX_MS           1000U
#define PFC_ADC_CHECK_TIME_MS           200U  /* DMA和测量连续有效至少200 ms。 */
#define PFC_PRECHARGE_STABLE_MS         500U  /* VAC/VBUS启动条件连续成立500 ms。 */
#define PFC_VAC_RMS_TOLERANCE           0.10f /* 当前参数档额定VAC RMS的正负10%。 */
#define PFC_GRID_FREQUENCY_TOLERANCE    3.0f  /* Hz */

/* 主循环与ADC ISR共享app_state；volatile保证每次读取内存，但状态迁移仍由接口约束串行化。 */
static const PFC_Params *app_params = 0;       /* 初始化后只读，参数对象必须常驻。 */
static volatile PFC_State app_state = PFC_SAFE;

/* 下列状态计数均只由1 ms主循环更新。 */
static uint32_t app_state_time_ms = 0U;
static uint32_t app_precharge_stable_ms = 0U;
static uint32_t app_last_adc1_sequence = 0U;
static uint32_t app_last_adc2_sequence = 0U;
static uint8_t app_iwdg_reset_seen = 0U;

/* PD0消抖状态只由PFC_AppTick1ms()读写。1表示按键按下，即引脚为低电平。 */
static uint8_t key_raw_last = 0U;
static uint8_t key_stable = 0U;
static uint16_t key_debounce_ms = 0U;
static uint16_t key_pressed_ms = 0U;

/* 检查被动预充和人工启动前必须持续成立的输入条件。 */
static uint8_t PFC_AppInputReady(const PFC_Measurement *measurement)
{
    float vac_min;
    float vac_max;
    float frequency_min;
    float frequency_max;

    if ((measurement == 0) || (app_params == 0))
    {
        return 0U;
    }

    vac_min = app_params->vac_nominal_rms * (1.0f - PFC_VAC_RMS_TOLERANCE);
    vac_max = app_params->vac_nominal_rms * (1.0f + PFC_VAC_RMS_TOLERANCE);
    frequency_min = app_params->grid_frequency_hz - PFC_GRID_FREQUENCY_TOLERANCE;
    frequency_max = app_params->grid_frequency_hz + PFC_GRID_FREQUENCY_TOLERANCE;

    return (uint8_t)((measurement->valid != 0U) &&
                     (measurement->vac_locked != 0U) &&
                     (measurement->vac_rms >= vac_min) &&
                     (measurement->vac_rms <= vac_max) &&
                     (measurement->vac_frequency_hz >= frequency_min) &&
                     (measurement->vac_frequency_hz <= frequency_max) &&
                     (measurement->vbus >= app_params->vbus_start_min));
}

/*
 * 低有效按键在释放沿生成一次短按事件，避免按住期间每1 ms重复启停。
 * 超过1 s的长按首版不执行任何动作；故障状态也不会借按键自动清除。
 */
static uint8_t PFC_AppReadShortPress(void)
{
    uint8_t raw_pressed = (HAL_GPIO_ReadPin(PFC_KEY_GPIO_PORT, PFC_KEY_GPIO_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
    uint8_t short_press = 0U;

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
            if ((key_stable != 0U) && (raw_pressed == 0U) &&
                (key_pressed_ms >= PFC_KEY_SHORT_MIN_MS) &&
                (key_pressed_ms <= PFC_KEY_SHORT_MAX_MS))
            {
                short_press = 1U;
            }
            key_stable = raw_pressed;
            if (key_stable == 0U)
            {
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

/* 人工停机和软件故障都先拉低PE0，再关闭四路PWM；HRTIM计数器继续供ADC采样。 */
static void PFC_AppStopPower(void)
{
    PFC_HRTIM_StopPower();
    SPWM_Reset();
}

/*
 * 启动顺序固定为：复核输入 -> 预写首个Compare -> 开放MCU PWM -> 拉高PE0。
 * PB10为低有效HRTIM Fault 3，PE5为PFC驱动器诊断输入；任一路为低都拒绝启动。
 */
static void PFC_AppStartPower(const PFC_Measurement *measurement)
{
    if ((PFC_Params_IsValid(app_params) == 0U) ||
        (PFC_AppInputReady(measurement) == 0U) ||
        (SPWM_IsSynchronized() == 0U) ||
        (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
        (HAL_GPIO_ReadPin(PFC_NFAULT_DIAG_GPIO_Port, PFC_NFAULT_DIAG_Pin) == GPIO_PIN_RESET))
    {
        return;
    }

    SPWM_OpenLoopRampStep();
    if ((SPWM_IsHealthy() == 0U) || (PFC_HRTIM_StartOutputs() != HAL_OK))
    {
        PFC_AppTrip(PFC_FAULT_HRTIM | PFC_FAULT_MODULATION);
        return;
    }

    HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_SET);
    app_state = PFC_OPEN_LOOP_RAMP;
    app_state_time_ms = 0U;
}

/**
 * @brief  初始化单相PFC状态机、按键消抖和开环调制器。
 * @param  params 常驻只读参数地址，不能在运行期间释放或修改。
 * @param  iwdg_reset_seen 非0表示本次启动来自IWDG复位，SAFE状态延长到1 s。
 * @note   在HRTIM采样计数器启动后调用；函数会强制关闭PE0和PWM，不会启动功率级。
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
    key_raw_last = 0U;
    key_stable = 0U;
    key_debounce_ms = 0U;
    key_pressed_ms = 0U;
    PFC_HRTIM_StopPower();
    SPWM_Init(params);
}

/**
 * @brief  执行一次10 kHz同步和开环调制更新。
 * @note   只能从ADC1完整DMA回调调用；不阻塞、不访问OLED/USART，仅在RAMP/RUN写Compare。
 */
void PFC_AppFastStep(void)
{
    PFC_Measurement measurement;

    PFC_Measure_GetSnapshot(&measurement);
    if ((measurement.valid == 0U) || (measurement.fault_bits != PFC_FAULT_NONE))
    {
        return;
    }

    SPWM_SyncUpdate(&measurement);
    if ((app_state == PFC_OPEN_LOOP_RAMP) || (app_state == PFC_OPEN_LOOP_RUN))
    {
        SPWM_OpenLoopRampStep();
        if (SPWM_IsHealthy() == 0U)
        {
            PFC_AppTrip(PFC_FAULT_MODULATION);
        }
    }
}

/**
 * @brief  执行1 ms按键、启动条件和状态迁移。
 * @note   仅由主循环的1 ms调度调用；FAULT_LATCH不接受按键清故障。
 */
void PFC_AppTick1ms(void)
{
    PFC_Measurement measurement;
    uint8_t short_press = PFC_AppReadShortPress();

    PFC_Measure_GetSnapshot(&measurement);
    app_state_time_ms++;

    if (measurement.fault_bits != PFC_FAULT_NONE)
    {
        app_state = PFC_FAULT_LATCH;
        PFC_AppStopPower();
        return;
    }

    switch (app_state)
    {
    case PFC_SAFE:
        if (app_state_time_ms >= ((app_iwdg_reset_seen != 0U) ? 1000U : 100U))
        {
            app_state = PFC_CALIBRATION;
            app_state_time_ms = 0U;
        }
        break;

    case PFC_CALIBRATION:
        if ((PFC_Params_IsValid(app_params) != 0U) && (measurement.offset_ready != 0U))
        {
            app_state = PFC_ADC_CHECK;
            app_state_time_ms = 0U;
        }
        break;

    case PFC_ADC_CHECK:
        if ((measurement.valid != 0U) && (app_state_time_ms >= PFC_ADC_CHECK_TIME_MS))
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
                app_state = PFC_READY;
                app_state_time_ms = 0U;
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
        }
        else if (short_press != 0U)
        {
            PFC_AppStartPower(&measurement);
        }
        break;

    case PFC_OPEN_LOOP_RAMP:
        if ((short_press != 0U) || (measurement.vac_locked == 0U))
        {
            PFC_AppStopPower();
            app_state = (short_press != 0U) ? PFC_STOP : PFC_FAULT_LATCH;
            if (short_press == 0U)
            {
                PFC_Measure_Trip(PFC_FAULT_VAC_LOST);
            }
            app_state_time_ms = 0U;
        }
        else if ((measurement.vbus >= app_params->vbus_target) ||
                 (SPWM_GetCurrentCommand() >= app_params->current_command_max))
        {
            app_state = PFC_OPEN_LOOP_RUN;
            app_state_time_ms = 0U;
        }
        break;

    case PFC_OPEN_LOOP_RUN:
        if ((short_press != 0U) || (measurement.vac_locked == 0U))
        {
            PFC_AppStopPower();
            app_state = (short_press != 0U) ? PFC_STOP : PFC_FAULT_LATCH;
            if (short_press == 0U)
            {
                PFC_Measure_Trip(PFC_FAULT_VAC_LOST);
            }
            app_state_time_ms = 0U;
        }
        break;

    case PFC_STOP:
        if (app_state_time_ms >= 100U)
        {
            app_state = (PFC_AppInputReady(&measurement) != 0U) ?
                        PFC_READY : PFC_PASSIVE_PRECHARGE;
            app_precharge_stable_ms = 0U;
            app_state_time_ms = 0U;
        }
        break;

    case PFC_FAULT_LATCH:
        /* 进入故障时已经关断，锁存期间不再每1 ms重复写HAL和HRTIM寄存器。 */
        break;

    default:
        PFC_AppTrip(PFC_FAULT_STATE);
        break;
    }
}

/**
 * @brief  锁存应用故障并立即关闭Gate和PWM。
 * @param  fault_bits 一个或多个PFC_FaultMask位。
 * @note   可从ISR或主循环调用；不会自动清故障或重新启动。
 */
void PFC_AppTrip(uint32_t fault_bits)
{
    PFC_Measure_Trip(fault_bits);
    app_state = PFC_FAULT_LATCH;
}

/** @brief 返回当前状态；可供OLED、VOFA和调试器只读。 */
PFC_State PFC_AppGetState(void)
{
    return app_state;
}

/**
 * @brief  检查100 ms监督周期内ADC序列和Gate状态是否与状态机一致。
 * @retval 1允许本周期刷新IWDG，0禁止刷新并等待IWDG复位。
 * @note   只在主循环每100 ms调用一次；调用本身会更新上次ADC序列基准。
 */
uint8_t PFC_AppWatchdogHealthy(void)
{
    PFC_Measurement measurement;
    uint8_t healthy;

    PFC_Measure_GetSnapshot(&measurement);
    healthy = (uint8_t)((measurement.adc1_sequence != app_last_adc1_sequence) &&
                        (measurement.adc2_sequence != app_last_adc2_sequence) &&
                        (measurement.fault_bits == PFC_FAULT_NONE) &&
                        (app_state != PFC_FAULT_LATCH));
    app_last_adc1_sequence = measurement.adc1_sequence;
    app_last_adc2_sequence = measurement.adc2_sequence;

    if ((app_state == PFC_OPEN_LOOP_RAMP) || (app_state == PFC_OPEN_LOOP_RUN))
    {
        healthy = (uint8_t)(healthy &&
            (HAL_GPIO_ReadPin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin) == GPIO_PIN_SET));
    }
    else
    {
        healthy = (uint8_t)(healthy &&
            (HAL_GPIO_ReadPin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin) == GPIO_PIN_RESET));
    }
    return healthy;
}
