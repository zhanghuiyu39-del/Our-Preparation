#include "pfc_inv_app.h"

#include "main.h"
#include "pfc_inv_user_config.h"
#include "pfc_app.h"
#include "pfc_control.h"
#include "pfc_hrtim.h"
#include "SPWM.h"
#include "hrtim.h"
#include "inv_hrtim.h"
#include "inv_open_loop.h"

/*
 * 本模块负责联合状态编排、公共HRTIM生命周期、PD0启停、PD1频率切换、
 * VBUS稳定窗口和故障仲裁。
 * 它不直接读取DMA数组，也不在ISR中执行OLED、VOFA、HAL_Delay或阻塞串口。
 * 数据流：ADC回调 -> pfc_measure/inv_measure快照；HRTIM Master REP 10 kHz事件 ->
 *         PFC/INV快速控制 -> Compare；
 *         1 ms主循环 -> PD0和状态迁移 -> 只在许可边界开放对应HRTIM输出。
 */

static volatile CBSVPWM_t pfc_inv_svpwm;
static volatile PFC_INV_State pfc_inv_state = PFC_INV_STATE_SAFE;
static volatile uint32_t pfc_inv_fault_bits = PFC_INV_FAULT_NONE;
static volatile uint32_t pfc_inv_control_heartbeat = 0U;

#if PFC_USER_RELAXED_PWM_TEST == 0U
/* 完整保护模式下用于母线稳定窗口；默认放宽模式不保留未使用指针。 */
static const PFC_Params *joint_pfc_params;
#endif
static uint32_t joint_vbus_stable_ms;
static uint32_t joint_last_inv_heartbeat;
static uint8_t joint_started;
static uint8_t joint_pwm_enable;
/*
 * 当前联合测试阶段。配置头在初始化时写入一次，后续只读；用运行对象保存可避免编译器将
 * 其他编译阶段的PFC-only/INV-only分支直接判成不可达，也保证全部阶段共用同一状态机。
 */
static uint8_t joint_stage;

static uint8_t key_raw_last;
static uint8_t key_stable;
static uint8_t key_unlocked;
static uint16_t key_release_ms;
static uint16_t key_debounce_ms;
static uint16_t key_pressed_ms;

/* PD1频率键拥有独立消抖状态，不能因PD0同时按下而丢失或重复产生切换事件。 */
static uint8_t frequency_key_raw_last;
static uint8_t frequency_key_stable;
static uint8_t frequency_key_unlocked;
static uint16_t frequency_key_release_ms;
static uint16_t frequency_key_debounce_ms;
static uint16_t frequency_key_pressed_ms;

/*
 * RAW_ADC阶段只累计零点；确认INV标定后直接使用用户头文件的固定零点、正比例和极性。
 * 这样带功率时交流电压/电流不会被重新平均到零点中。若只做PFC-only且尚未确认INV标定，
 * 则保留自动零点模式供低风险观察，但该模式不构成INV PWM的开放许可。
 */
static const INV_MeasureConfig joint_inv_measure_config =
{
#if (PFC_INV_STAGE == PFC_INV_STAGE_RAW_ADC) || (PFC_INV_STAGE == PFC_INV_STAGE_PFC_ONLY)
    INV_MEASURE_CALIBRATE_ZERO,
#elif (PFC_INV_INV_CALIBRATION_CONFIRMED == 1U)
    INV_MEASURE_RUN_FIXED_CALIBRATION,
#else
    INV_MEASURE_RUN_AUTO_OFFSET,
#endif
    PFC_INV_INV_OFFSET_SAMPLES,
    PFC_INV_INV_GAIN_SAMPLES,
    PFC_INV_INV_CAL_MIN_DELTA_COUNTS,
    {PFC_INV_INV_IU_OFFSET, PFC_INV_INV_IV_OFFSET, PFC_INV_INV_IW_OFFSET,
     PFC_INV_INV_VUV_OFFSET, PFC_INV_INV_VVW_OFFSET, PFC_INV_INV_VWU_OFFSET},
    {PFC_INV_INV_IU_SCALE, PFC_INV_INV_IV_SCALE, PFC_INV_INV_IW_SCALE,
     PFC_INV_INV_VUV_SCALE, PFC_INV_INV_VVW_SCALE, PFC_INV_INV_VWU_SCALE},
    {PFC_INV_INV_IU_POLARITY, PFC_INV_INV_IV_POLARITY, PFC_INV_INV_IW_POLARITY,
     PFC_INV_INV_VUV_POLARITY, PFC_INV_INV_VVW_POLARITY, PFC_INV_INV_VWU_POLARITY},
    PFC_INV_INV_CALIBRATION_CHANNEL,
    PFC_INV_INV_CALIBRATION_REFERENCE,
    PFC_INV_INV_ADC_MAX_SKEW_FRAMES,
    PFC_INV_INV_ADC_RAIL_LOW_COUNT,
    PFC_INV_INV_ADC_RAIL_HIGH_COUNT,
    PFC_INV_INV_ADC_RAIL_CONFIRM_FRAMES
};

static const INV_OpenLoopConfig joint_inv_open_loop_config =
{
    PFC_INV_INV_FIXED_DC_BUS_V,
#if (PFC_INV_ACTIVE_PROFILE == PFC_INV_PROFILE_36V)
    PFC_INV_INV_LINE_RMS_V_36V,
#else
    PFC_INV_INV_LINE_RMS_V_5V,
#endif
    (PFC_INV_INV_OUTPUT_FREQUENCY == 30U) ? INV_FREQ_30HZ : INV_FREQ_60HZ,
    PFC_INV_INV_MODULATION_LIMIT,
    PFC_INV_INV_SOFT_START_MS,
    PFC_INV_INV_CONTROL_FREQUENCY_HZ
};

static uint8_t JointKeyPoll(void)
{
    uint8_t raw_pressed = (HAL_GPIO_ReadPin(START_KEY_GPIO_Port, START_KEY_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    uint8_t short_press = 0U;

    if (key_unlocked == 0U)
    {
        if (raw_pressed == 0U)
        {
            if (key_release_ms < 30U) { key_release_ms++; }
            if (key_release_ms >= 30U)
            {
                key_unlocked = 1U;
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
    else if (key_debounce_ms < 30U)
    {
        key_debounce_ms++;
        if (key_debounce_ms == 30U)
        {
            uint8_t old_stable = key_stable;
            key_stable = raw_pressed;
            if (key_stable != 0U) { key_pressed_ms = 0U; }
            else if (old_stable != 0U)
            {
                if ((key_pressed_ms >= 50U) && (key_pressed_ms <= 1000U)) { short_press = 1U; }
                key_pressed_ms = 0U;
            }
        }
    }
    if ((key_stable != 0U) && (key_pressed_ms < 0xFFFFU)) { key_pressed_ms++; }
    return short_press;
}

/**
 * @brief 每1 ms轮询一次PD1，完成释放解锁、30 ms消抖和短按识别。
 * @retval 1表示本周期完成一次50~1000 ms按下后释放；其余返回0。
 * @note 只读取GPIO，不改变DDS、状态机或PWM；长按和上电按住均不产生事件。
 */
static uint8_t JointFrequencyKeyPoll(void)
{
    uint8_t raw_pressed =
        (HAL_GPIO_ReadPin(SWITCH_FRQ_GPIO_Port, SWITCH_FRQ_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    uint8_t short_press = 0U;

    if (frequency_key_unlocked == 0U)
    {
        if (raw_pressed == 0U)
        {
            if (frequency_key_release_ms < 30U) { frequency_key_release_ms++; }
            if (frequency_key_release_ms >= 30U)
            {
                frequency_key_unlocked = 1U;
                frequency_key_raw_last = 0U;
                frequency_key_stable = 0U;
                frequency_key_debounce_ms = 0U;
                frequency_key_pressed_ms = 0U;
            }
        }
        else
        {
            frequency_key_release_ms = 0U;
        }
        return 0U;
    }

    if (raw_pressed != frequency_key_raw_last)
    {
        frequency_key_raw_last = raw_pressed;
        frequency_key_debounce_ms = 0U;
    }
    else if (frequency_key_debounce_ms < 30U)
    {
        frequency_key_debounce_ms++;
        if (frequency_key_debounce_ms == 30U)
        {
            uint8_t old_stable = frequency_key_stable;
            frequency_key_stable = raw_pressed;
            if (frequency_key_stable != 0U)
            {
                frequency_key_pressed_ms = 0U;
            }
            else if (old_stable != 0U)
            {
                if ((frequency_key_pressed_ms >= 50U) &&
                    (frequency_key_pressed_ms <= 1000U))
                {
                    short_press = 1U;
                }
                frequency_key_pressed_ms = 0U;
            }
        }
    }
    if ((frequency_key_stable != 0U) &&
        (frequency_key_pressed_ms < 0xFFFFU))
    {
        frequency_key_pressed_ms++;
    }
    return short_press;
}

/** 保持DDS相位和软启动进度，只把当前30/60 Hz切换到另一档。 */
static uint8_t JointToggleInvFrequency(void)
{
    INV_OutputFrequency current_frequency;
    INV_OutputFrequency target_frequency;

    if (!INV_OpenLoop_GetFrequency(&current_frequency)) { return 0U; }
    target_frequency = (current_frequency == INV_FREQ_60HZ) ?
                       INV_FREQ_30HZ : INV_FREQ_60HZ;
    return INV_OpenLoop_SetFrequency(target_frequency) ? 1U : 0U;
}

static uint8_t JointPfcStageActive(void)
{
    return (uint8_t)((joint_stage == PFC_INV_STAGE_PFC_ONLY) ||
                     (joint_stage == PFC_INV_STAGE_JOINT_LOW_POWER) ||
                     (joint_stage == PFC_INV_STAGE_JOINT_CONTEST));
}

static uint8_t JointInvStageActive(void)
{
    return (uint8_t)((joint_stage == PFC_INV_STAGE_INV_ONLY) ||
                     (joint_stage == PFC_INV_STAGE_JOINT_LOW_POWER) ||
                     (joint_stage == PFC_INV_STAGE_JOINT_CONTEST));
}

/* INV运行时，PFC只作为可选前端；PFC异常不得覆盖INV状态或停止C/D/E。 */
static uint8_t JointInvRunning(void)
{
    return (uint8_t)((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
                     (pfc_inv_state == PFC_INV_STATE_JOINT_RUN));
}

/* RAW_ADC/PFC-only需要PFC采样；INV-only和联合运行不把INV线电压采样作为发波门槛。 */
static uint8_t JointPfcDomainMonitored(void)
{
    return (uint8_t)((joint_stage == PFC_INV_STAGE_RAW_ADC) ||
                     (joint_stage == PFC_INV_STAGE_PFC_ONLY));
}

/** 复制由INV DMA ISR写入的CBSVPWM结果，避免OLED/VOFA读到半个控制周期。 */
static void JointGetSvpwmSnapshot(CBSVPWM_t *snapshot)
{
    uint32_t primask;

    if (snapshot == NULL) { return; }
    primask = __get_PRIMASK();
    __disable_irq();
    *snapshot = pfc_inv_svpwm;
    __set_PRIMASK(primask);
}

static void JointDisableAllOutputs(void)
{
    PFC_HRTIM_EmergencyOff();
    INV_HRTIM_EmergencyOff();
    pfc_inv_svpwm.duty_u = 0.5f;
    pfc_inv_svpwm.duty_v = 0.5f;
    pfc_inv_svpwm.duty_w = 0.5f;
}

static HAL_StatusTypeDef JointStartInv(void)
{
    float line_rms_command;

    if (joint_pwm_enable == 0U)
    {
        return HAL_ERROR;
    }

#if (PFC_INV_ACTIVE_PROFILE == PFC_INV_PROFILE_36V)
    line_rms_command = (PFC_INV_INV_CURRENT_PRIORITY_MODE != 0U) ?
                       PFC_INV_INV_CURRENT_PRIORITY_LINE_RMS_V_36V :
                       PFC_INV_INV_LINE_RMS_V_36V;
#else
    line_rms_command = (PFC_INV_INV_CURRENT_PRIORITY_MODE != 0U) ?
                       PFC_INV_INV_CURRENT_PRIORITY_LINE_RMS_V_5V :
                       PFC_INV_INV_LINE_RMS_V_5V;
#endif

    INV_OpenLoop_Reset();
    if (!INV_OpenLoop_SetVoltageCommand(PFC_INV_INV_FIXED_DC_BUS_V,
                                         line_rms_command) ||
        CBSVPWM_Init((CBSVPWM_t *)&pfc_inv_svpwm,
                     PFC_INV_INV_MODULATION_LIMIT,
                     PFC_INV_INV_MINIMUM_DC_V) == 0U ||
        INV_HRTIM_SetDuty(0.5f, 0.5f, 0.5f) != HAL_OK ||
        INV_HRTIM_EnableOutputs() != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef PFC_INV_AppInit(const PFC_Params *pfc_params,
                                  uint8_t iwdg_reset_seen)
{
#if PFC_USER_RELAXED_PWM_TEST == 0U
    joint_pfc_params = pfc_params;
#endif
    joint_vbus_stable_ms = 0U;
    joint_last_inv_heartbeat = 0U;
    joint_started = 0U;
    joint_pwm_enable = (uint8_t)PFC_INV_PWM_ENABLE;
    joint_stage = (uint8_t)PFC_INV_STAGE;
    key_raw_last = 0U;
    key_stable = 0U;
    key_unlocked = 0U;
    key_release_ms = 0U;
    key_debounce_ms = 0U;
    key_pressed_ms = 0U;
    frequency_key_raw_last = 0U;
    frequency_key_stable = 0U;
    frequency_key_unlocked = 0U;
    frequency_key_release_ms = 0U;
    frequency_key_debounce_ms = 0U;
    frequency_key_pressed_ms = 0U;
    pfc_inv_state = PFC_INV_STATE_SAFE;
    pfc_inv_fault_bits = PFC_INV_FAULT_NONE;
    pfc_inv_control_heartbeat = 0U;

    PFC_Measure_Init(pfc_params);
    PFC_AppInit(pfc_params, iwdg_reset_seen);
    if (!INV_Measure_Init(&joint_inv_measure_config))
    {
        return HAL_ERROR;
    }
    if (JointInvStageActive() && !INV_OpenLoop_Init(&joint_inv_open_loop_config))
    {
        return HAL_ERROR;
    }
    if (CBSVPWM_Init((CBSVPWM_t *)&pfc_inv_svpwm,
                     PFC_INV_INV_MODULATION_LIMIT,
                     PFC_INV_INV_MINIMUM_DC_V) == 0U)
    {
        return HAL_ERROR;
    }
    JointDisableAllOutputs();
    return HAL_OK;
}

HAL_StatusTypeDef PFC_INV_AppStartSharedTimebase(void)
{
    HAL_StatusTypeDef status = PFC_HRTIM_StartSampling();
    if (status == HAL_OK)
    {
        /*
         * INV开环的控制节拍必须来自正在运行的HRTIM，而不是ADC3 DMA完成回调。
         * ADC3/4/5是线电压观察通道，可能未接采样板；把DDS/CBSVPWM绑定到它们
         * 会在ADC异常时停止心跳，随后IWDG复位整机。Master重复事件每个10 kHz
         * 周期产生一次，实际回调在stm32g4xx_it.c中转发到主程序。
         */
        __HAL_HRTIM_MASTER_CLEAR_IT(&hhrtim1, HRTIM_MASTER_IT_MREP);
        HAL_NVIC_ClearPendingIRQ(HRTIM1_Master_IRQn);
        HAL_NVIC_SetPriority(HRTIM1_Master_IRQn, 1U, 1U);
        HAL_NVIC_EnableIRQ(HRTIM1_Master_IRQn);
        __HAL_HRTIM_MASTER_ENABLE_IT(&hhrtim1, HRTIM_MASTER_IT_MREP);
        INV_HRTIM_AttachSharedTimeBase();
        joint_started = 1U;
    }
    return status;
}

uint8_t PFC_INV_AppSamplingReady(void)
{
    PFC_Measurement pfc;
    PFC_Measure_GetSnapshot(&pfc);
    if (joint_started == 0U)
    {
        return 0U;
    }
    if (((joint_stage == PFC_INV_STAGE_RAW_ADC) ||
         (joint_stage == PFC_INV_STAGE_PFC_ONLY)) &&
        ((pfc.adc1_sequence <= 5U) || (pfc.adc2_sequence <= 5U)))
    {
        return 0U;
    }
    return 1U;
}

void PFC_INV_AppFastPfcStep(void)
{
    if (JointPfcStageActive())
    {
        PFC_AppFastStep();
        if (PFC_Measure_GetFault() != PFC_FAULT_NONE)
        {
            if (JointPfcDomainMonitored() == 0U)
            {
                /*
                 * 联合档把PFC视为可选前端。无论INV是否已经启动，PFC故障都只关闭A/B，
                 * 不能锁死PD0的INV启动路径，也不能关闭正在运行的C/D/E。
                 */
                PFC_HRTIM_EmergencyOff();
                PFC_Control_Reset();
            }
            else
            {
                PFC_INV_AppTrip(PFC_INV_FAULT_PFC, PFC_Measure_GetFault(), 0U);
            }
        }
    }
}

void PFC_INV_AppFastInvStep(void)
{
    INV_OpenLoopOutput reference;
    float modulation_bus_v = PFC_INV_INV_FIXED_DC_BUS_V;

    if (!JointInvStageActive()) { return; }
    if ((pfc_inv_state != PFC_INV_STATE_INV_SOFT_START) &&
        (pfc_inv_state != PFC_INV_STATE_JOINT_RUN))
    {
        return;
    }
    /* ADC3/4/5只用于线电压观察，valid、贴轨、AWD和失步均不参与开环PWM许可。 */
    /* 电流优先模式使用最近一次有效VBUS作为调制分母，低母线时尽量争取线电流。 */
#if (PFC_INV_INV_CURRENT_PRIORITY_MODE != 0U)
    {
        PFC_Measurement pfc;
        PFC_Measure_GetSnapshot(&pfc);
        if ((pfc.valid != 0U) &&
            (pfc.vbus >= PFC_INV_INV_MINIMUM_DC_V))
        {
            modulation_bus_v = pfc.vbus;
        }
    }
#endif
    if (!INV_OpenLoop_Step(&reference) ||
        (reference.valid == 0U) ||
        (CBSVPWM_Calc3Leg((CBSVPWM_t *)&pfc_inv_svpwm,
                          reference.vu_command, reference.vv_command,
                          reference.vw_command, modulation_bus_v) == 0U) ||
        (INV_HRTIM_SetDuty(pfc_inv_svpwm.duty_u,
                           pfc_inv_svpwm.duty_v,
                           pfc_inv_svpwm.duty_w) != HAL_OK))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_INV, 0U, INV_FAULT_CBSVPWM | INV_FAULT_PWM_COMMAND);
        return;
    }
    pfc_inv_control_heartbeat++;
    if ((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) && (reference.ramp >= 1.0f))
    {
        pfc_inv_state = PFC_INV_STATE_JOINT_RUN;
    }
}

void PFC_INV_AppTick1ms(void)
{
    uint8_t short_press = JointKeyPoll();
    uint8_t frequency_short_press = JointFrequencyKeyPoll();
    PFC_Measurement pfc;
    PFC_State pfc_state;

    /* RAW_ADC/PFC-only没有初始化INV参考模块，PD1在这些阶段只完成消抖、不切换。 */
    if ((frequency_short_press != 0U) &&
        (JointInvStageActive() != 0U) &&
        (pfc_inv_state != PFC_INV_STATE_FAULT_LATCH) &&
        (JointToggleInvFrequency() == 0U))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_INV, PFC_FAULT_NONE,
                        INV_FAULT_PARAMETER);
        return;
    }

    if (JointPfcStageActive())
    {
        PFC_AppTick1msWithCommand((PFC_INV_PWM_ENABLE != 0U) ? short_press : 0U);
    }
    PFC_Measure_GetSnapshot(&pfc);
    pfc_state = PFC_AppGetState();

    /* 联合阶段PD0直接启动INV，不要求PFC测量有效、母线达标或A/B已经发波。 */
    if ((short_press != 0U) && (JointInvStageActive() != 0U) &&
        (pfc_inv_state != PFC_INV_STATE_FAULT_LATCH))
    {
        if ((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
            (pfc_inv_state == PFC_INV_STATE_JOINT_RUN))
        {
            INV_HRTIM_DisableOutputs();
            INV_OpenLoop_Reset();
            PFC_HRTIM_EmergencyOff();
            pfc_inv_state = PFC_INV_STATE_STOP;
            return;
        }
        if (JointStartInv() != HAL_OK)
        {
            PFC_INV_AppTrip(PFC_INV_FAULT_INV, PFC_FAULT_NONE, INV_FAULT_HRTIM);
            return;
        }
        pfc_inv_state = PFC_INV_STATE_INV_SOFT_START;
        /* 本次调度已经完成INV直接投入，不能再被PFC状态映射覆盖。 */
        return;
    }

    if ((JointPfcDomainMonitored() != 0U) &&
         (pfc_inv_state != PFC_INV_STATE_INV_SOFT_START) &&
         (pfc_inv_state != PFC_INV_STATE_JOINT_RUN) &&
         (PFC_Measure_GetFault() != PFC_FAULT_NONE))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_PFC,
                        PFC_Measure_GetFault(), INV_FAULT_NONE);
        return;
    }

    if (joint_stage == PFC_INV_STAGE_RAW_ADC)
    {
        pfc_inv_state = PFC_INV_STATE_ADC_CALIBRATION;
        JointDisableAllOutputs();
        return;
    }

    if (joint_stage == PFC_INV_STAGE_INV_ONLY)
    {
        if ((pfc_inv_state == PFC_INV_STATE_SAFE) ||
            (pfc_inv_state == PFC_INV_STATE_ADC_CALIBRATION) ||
            (pfc_inv_state == PFC_INV_STATE_STOP))
        {
            pfc_inv_state = PFC_INV_STATE_READY;
        }
        if ((short_press != 0U) && (pfc_inv_state == PFC_INV_STATE_READY))
        {
            if (JointStartInv() != HAL_OK)
            {
                PFC_INV_AppTrip(PFC_INV_FAULT_INV, 0U, INV_FAULT_HRTIM);
                return;
            }
            pfc_inv_state = PFC_INV_STATE_INV_SOFT_START;
        }
        else if ((short_press != 0U) &&
                 ((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
                  (pfc_inv_state == PFC_INV_STATE_JOINT_RUN)))
        {
            INV_HRTIM_DisableOutputs();
            INV_OpenLoop_Reset();
            pfc_inv_state = PFC_INV_STATE_STOP;
        }
        return;
    }

    if ((joint_stage == PFC_INV_STAGE_PFC_ONLY) &&
        (pfc_state == PFC_FAULT_LATCH) && (JointInvRunning() == 0U))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_PFC, PFC_Measure_GetFault(), 0U);
        return;
    }
    if (joint_stage == PFC_INV_STAGE_PFC_ONLY)
    {
        pfc_inv_state = (pfc_state == PFC_READY) ? PFC_INV_STATE_READY :
                        (pfc_state == PFC_CURRENT_LOOP_RAMP) ? PFC_INV_STATE_PFC_CURRENT_LOOP :
                        (pfc_state == PFC_VBUS_LOOP_RAMP) ? PFC_INV_STATE_PFC_VBUS_RAMP :
                        (pfc_state == PFC_VBUS_LOOP_RUN) ? PFC_INV_STATE_PFC_VBUS_STABLE :
                        PFC_INV_STATE_SAFE;
        return;
    }

    /* 联合阶段INV已运行后，PFC状态不能覆盖INV软启动/运行状态。 */
    if ((JointInvStageActive() != 0U) && (JointInvRunning() != 0U))
    {
        return;
    }

    if (pfc_state == PFC_CURRENT_LOOP_RAMP)
    {
        pfc_inv_state = PFC_INV_STATE_PFC_CURRENT_LOOP;
    }
    else if (pfc_state == PFC_VBUS_LOOP_RAMP)
    {
        pfc_inv_state = PFC_INV_STATE_PFC_VBUS_RAMP;
    }
    else if (pfc_state == PFC_VBUS_LOOP_RUN)
    {
        if ((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
            (pfc_inv_state == PFC_INV_STATE_JOINT_RUN))
        {
            /* INV投入后保持PFC双闭环运行，C/D/E由独立10 kHz快速路径更新。 */
        }
        else if ((pfc.valid != 0U)
#if PFC_USER_RELAXED_PWM_TEST == 0U
            && (pfc.vbus >= (joint_pfc_params->vbus_target - joint_pfc_params->vbus_target_tolerance))
            && (pfc.vbus <= (joint_pfc_params->vbus_target + joint_pfc_params->vbus_target_tolerance))
#endif
               )
        {
            if (joint_vbus_stable_ms < PFC_INV_VBUS_STABLE_MS) { joint_vbus_stable_ms++; }
            if ((joint_vbus_stable_ms >= PFC_INV_VBUS_STABLE_MS) &&
                (pfc_inv_state != PFC_INV_STATE_INV_SOFT_START) &&
                (pfc_inv_state != PFC_INV_STATE_JOINT_RUN))
            {
                if (JointStartInv() != HAL_OK)
                {
                    PFC_INV_AppTrip(PFC_INV_FAULT_INV, 0U, INV_FAULT_HRTIM);
                    return;
                }
                pfc_inv_state = PFC_INV_STATE_INV_SOFT_START;
            }
            else if (pfc_inv_state != PFC_INV_STATE_INV_SOFT_START &&
                     pfc_inv_state != PFC_INV_STATE_JOINT_RUN)
            {
                pfc_inv_state = PFC_INV_STATE_PFC_VBUS_STABLE;
            }
        }
        else
        {
            joint_vbus_stable_ms = 0U;
            pfc_inv_state = PFC_INV_STATE_PFC_VBUS_STABLE;
            INV_HRTIM_DisableOutputs();
        }
    }
    else if ((pfc_state == PFC_READY) || (pfc_state == PFC_STOP))
    {
        joint_vbus_stable_ms = 0U;
        INV_HRTIM_DisableOutputs();
        INV_OpenLoop_Reset();
        pfc_inv_state = (pfc_state == PFC_READY) ? PFC_INV_STATE_READY : PFC_INV_STATE_STOP;
    }
}

void PFC_INV_AppTrip(uint32_t joint_fault_bits,
                     uint32_t pfc_fault_bits,
                     uint32_t inv_fault_bits)
{
    pfc_inv_fault_bits |= joint_fault_bits;
    JointDisableAllOutputs();
    if (pfc_fault_bits != 0U) { PFC_AppTrip(pfc_fault_bits); }
    else { PFC_Control_Reset(); }
    if (inv_fault_bits != 0U) { INV_Measure_LatchFault(inv_fault_bits); }
    INV_OpenLoop_Reset();
    CBSVPWM_Reset((CBSVPWM_t *)&pfc_inv_svpwm);
    pfc_inv_state = PFC_INV_STATE_FAULT_LATCH;
}

void PFC_INV_AppReportPfcFault(uint32_t pfc_fault_bits)
{
    if (pfc_fault_bits == PFC_FAULT_NONE) { return; }
    PFC_Measure_Trip(pfc_fault_bits);
    if ((JointPfcDomainMonitored() != 0U) && (JointInvRunning() == 0U))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_PFC, pfc_fault_bits, INV_FAULT_NONE);
    }
}

void PFC_INV_AppReportInvFault(uint32_t inv_fault_bits)
{
    if (inv_fault_bits == INV_FAULT_NONE) { return; }
    /* INV ADC诊断只锁存供OLED/VOFA查看，不关闭C/D/E。 */
    INV_Measure_LatchFault(inv_fault_bits);
}

uint8_t PFC_INV_AppWatchdogHealthy(void)
{
    uint8_t healthy = 1U;

    if (pfc_inv_state == PFC_INV_STATE_FAULT_LATCH || pfc_inv_fault_bits != 0U) { healthy = 0U; }
    if ((JointPfcDomainMonitored() != 0U) && (JointInvRunning() == 0U) &&
        (PFC_AppWatchdogHealthy() == 0U)) { healthy = 0U; }
    if ((JointInvRunning() != 0U) &&
        (pfc_inv_control_heartbeat == joint_last_inv_heartbeat)) { healthy = 0U; }
    if (JointInvStageActive() &&
        (((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
          (pfc_inv_state == PFC_INV_STATE_JOINT_RUN)) !=
         (INV_HRTIM_OutputsEnabled() != 0U))) { healthy = 0U; }
    joint_last_inv_heartbeat = pfc_inv_control_heartbeat;
    return healthy;
}

void PFC_INV_AppGetTelemetry(PFC_INV_Telemetry *telemetry)
{
    PFC_Measurement pfc;
    INV_OpenLoopOutput reference;
    CBSVPWM_t svpwm;
    if (telemetry == NULL) { return; }
    PFC_Measure_GetSnapshot(&pfc);
    INV_OpenLoop_GetSnapshot(&reference);
    JointGetSvpwmSnapshot(&svpwm);
    telemetry->state = pfc_inv_state;
    telemetry->joint_fault_bits = pfc_inv_fault_bits;
    telemetry->pfc_fault_bits = PFC_Measure_GetFault();
    telemetry->inv_fault_bits = INV_Measure_GetFault();
    telemetry->pfc_vbus = pfc.vbus;
    telemetry->inv_modulation = svpwm.modulation_u;
    telemetry->inv_ramp = reference.ramp;
    telemetry->inv_duty_u = svpwm.duty_u;
    telemetry->inv_duty_v = svpwm.duty_v;
    telemetry->inv_duty_w = svpwm.duty_w;
    telemetry->pfc_heartbeat = pfc.fast_heartbeat;
    telemetry->inv_heartbeat = pfc_inv_control_heartbeat;
    {
        INV_OutputFrequency frequency;
        telemetry->inv_frequency_hz = INV_OpenLoop_GetFrequency(&frequency) ?
                                      (uint16_t)frequency : 0U;
    }
    telemetry->pfc_outputs_enabled = PFC_HRTIM_OutputsEnabled();
    telemetry->inv_outputs_enabled = INV_HRTIM_OutputsEnabled();
    telemetry->vbus_stable = ((pfc_inv_state == PFC_INV_STATE_PFC_VBUS_STABLE) ||
                              (pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
                              (pfc_inv_state == PFC_INV_STATE_JOINT_RUN)) ? 1U : 0U;
    telemetry->healthy = ((pfc_inv_fault_bits == 0U) &&
                          ((JointPfcDomainMonitored() == 0U) ||
                           (telemetry->pfc_fault_bits == 0U))) ? 1U : 0U;
}

PFC_INV_State PFC_INV_AppGetState(void)
{
    return pfc_inv_state;
}

void PFC_INV_AppEmergencyOff(void)
{
    PFC_HRTIM_EmergencyOff();
    INV_HRTIM_EmergencyOff();
}
