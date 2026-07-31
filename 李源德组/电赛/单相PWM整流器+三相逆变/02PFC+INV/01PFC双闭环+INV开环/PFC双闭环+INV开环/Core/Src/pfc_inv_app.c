#include "pfc_inv_app.h"

#include "main.h"
#include "pfc_inv_user_config.h"
#include "pfc_app.h"
#include "pfc_control.h"
#include "pfc_hrtim.h"
#include "SPWM.h"
#include "inv_hrtim.h"
#include "inv_open_loop.h"

/*
 * 本模块负责联合状态编排、公共HRTIM生命周期、PD0短按、VBUS稳定窗口和故障仲裁。
 * 它不直接读取DMA数组，也不在ISR中执行OLED、VOFA、HAL_Delay或阻塞串口。
 * 数据流：ADC回调 -> pfc_measure/inv_measure快照 -> PFC/INV快速控制 -> Compare；
 *         1 ms主循环 -> PD0和状态迁移 -> 只在许可边界开放对应HRTIM输出。
 */

volatile CBSVPWM_t pfc_inv_svpwm;
volatile PFC_INV_State pfc_inv_state = PFC_INV_STATE_SAFE;
volatile uint32_t pfc_inv_fault_bits = PFC_INV_FAULT_NONE;
volatile uint32_t pfc_inv_control_heartbeat = 0U;

static const PFC_Params *joint_pfc_params;
static uint32_t joint_vbus_stable_ms;
static uint32_t joint_last_pfc_sequence;
static uint32_t joint_last_inv_heartbeat;
static uint32_t joint_fast_vbus_sequence;
static uint8_t joint_vbus_stale_frames;
static uint8_t joint_started;
/*
 * 当前联合测试阶段。配置头在初始化时写入一次，后续只读；用运行对象保存可避免编译器将
 * RAW_ADC默认配置下的PFC-only/INV-only分支直接判成不可达，也保证全部阶段共用同一状态机。
 */
static uint8_t joint_stage;

static uint8_t key_raw_last;
static uint8_t key_stable;
static uint8_t key_unlocked;
static uint16_t key_release_ms;
static uint16_t key_debounce_ms;
static uint16_t key_pressed_ms;

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
    5.0f,
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

/* RAW_ADC要监督两套采样；INV-only虽然不驱动A/B，仍依赖ADC1的VBUS快照。 */
static uint8_t JointPfcDomainMonitored(void)
{
    return (uint8_t)((joint_stage == PFC_INV_STAGE_RAW_ADC) ||
                     JointPfcStageActive() ||
                     (joint_stage == PFC_INV_STAGE_INV_ONLY));
}

/* PFC-only不使用C/D/E和INV反馈，未接逆变采样板时的诊断故障不能阻断PFC验证。 */
static uint8_t JointInvDomainMonitored(void)
{
    return (uint8_t)((joint_stage == PFC_INV_STAGE_RAW_ADC) ||
                     JointInvStageActive());
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

static HAL_StatusTypeDef JointStartInv(const PFC_Measurement *pfc_measurement)
{
    if ((pfc_measurement == NULL) || (pfc_measurement->valid == 0U) ||
        (pfc_measurement->vbus < PFC_INV_INV_MINIMUM_DC_V) ||
        (joint_pfc_params == NULL) ||
        (pfc_measurement->vbus >= joint_pfc_params->vbus_overvoltage_trip) ||
        (PFC_INV_PWM_ENABLE == 0U) ||
        (PFC_INV_INV_CALIBRATION_CONFIRMED == 0U))
    {
        return HAL_ERROR;
    }

    INV_OpenLoop_Reset();
    if (!INV_OpenLoop_SetVoltageCommand(pfc_measurement->vbus,
#if (PFC_INV_ACTIVE_PROFILE == PFC_INV_PROFILE_36V)
                                         PFC_INV_INV_LINE_RMS_V_36V
#else
                                         PFC_INV_INV_LINE_RMS_V_5V
#endif
                                         ) ||
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
    joint_pfc_params = pfc_params;
    joint_vbus_stable_ms = 0U;
    joint_last_pfc_sequence = 0U;
    joint_last_inv_heartbeat = 0U;
    joint_fast_vbus_sequence = 0U;
    joint_vbus_stale_frames = 0U;
    joint_started = 0U;
    joint_stage = (uint8_t)PFC_INV_STAGE;
    key_raw_last = 0U;
    key_stable = 0U;
    key_unlocked = 0U;
    key_release_ms = 0U;
    key_debounce_ms = 0U;
    key_pressed_ms = 0U;
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
        INV_HRTIM_AttachSharedTimeBase();
        joint_started = 1U;
    }
    return status;
}

uint8_t PFC_INV_AppSamplingReady(void)
{
    PFC_Measurement pfc;
    INV_Measurement inv;
    PFC_Measure_GetSnapshot(&pfc);
    INV_Measure_GetSnapshot(&inv);
    if ((!joint_started) || (pfc.adc1_sequence <= 5U) || (pfc.adc2_sequence <= 5U))
    {
        return 0U;
    }
    if ((JointInvDomainMonitored() != 0U) &&
        ((inv.adc3_sequence <= 5U) || (inv.adc4_sequence <= 5U) ||
         (inv.adc5_sequence <= 5U)))
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
            PFC_INV_AppTrip(PFC_INV_FAULT_PFC, PFC_Measure_GetFault(), 0U);
        }
    }
}

void PFC_INV_AppFastInvStep(const INV_Measurement *measurement)
{
    PFC_Measurement pfc_measurement;
    INV_OpenLoopOutput reference;

    if (!JointInvStageActive() || (measurement == NULL)) { return; }
    if ((pfc_inv_state != PFC_INV_STATE_INV_SOFT_START) &&
        (pfc_inv_state != PFC_INV_STATE_JOINT_RUN))
    {
        return;
    }
    if ((measurement->valid == 0U) || (measurement->fault_bits != INV_FAULT_NONE))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_INV, 0U,
                        (measurement->fault_bits != 0U) ? measurement->fault_bits : INV_FAULT_CONTROL_TIMEOUT);
        return;
    }
    PFC_Measure_GetSnapshot(&pfc_measurement);
    if (pfc_measurement.adc1_sequence == joint_fast_vbus_sequence)
    {
        if (joint_vbus_stale_frames < PFC_INV_VBUS_STALE_FRAMES)
        {
            joint_vbus_stale_frames++;
        }
    }
    else
    {
        joint_fast_vbus_sequence = pfc_measurement.adc1_sequence;
        joint_vbus_stale_frames = 0U;
    }
    if ((pfc_measurement.valid == 0U) ||
        (joint_vbus_stale_frames >= PFC_INV_VBUS_STALE_FRAMES) ||
        (pfc_measurement.vbus < PFC_INV_INV_MINIMUM_DC_V) ||
        (joint_pfc_params == NULL) ||
        (pfc_measurement.vbus >= joint_pfc_params->vbus_overvoltage_trip))
    {
        uint32_t pfc_fault = ((joint_pfc_params != NULL) &&
                              (pfc_measurement.vbus >= joint_pfc_params->vbus_overvoltage_trip)) ?
                             PFC_FAULT_VBUS_OV : 0U;
        PFC_INV_AppTrip(PFC_INV_FAULT_VBUS_STALE, pfc_fault,
                        INV_FAULT_CONTROL_TIMEOUT);
        return;
    }
    if (!INV_OpenLoop_Step(&reference) ||
        (reference.valid == 0U) ||
        (CBSVPWM_Calc3Leg((CBSVPWM_t *)&pfc_inv_svpwm,
                          reference.vu_command, reference.vv_command,
                          reference.vw_command, pfc_measurement.vbus) == 0U) ||
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
    PFC_Measurement pfc;
    PFC_State pfc_state;

    if (JointPfcStageActive())
    {
        PFC_AppTick1msWithCommand((PFC_INV_PWM_ENABLE != 0U) ? short_press : 0U);
    }
    PFC_Measure_GetSnapshot(&pfc);
    pfc_state = PFC_AppGetState();

    if (((JointPfcDomainMonitored() != 0U) &&
         (PFC_Measure_GetFault() != PFC_FAULT_NONE)) ||
        ((JointInvDomainMonitored() != 0U) &&
         (INV_Measure_GetFault() != INV_FAULT_NONE)))
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_PFC | PFC_INV_FAULT_INV,
                        PFC_Measure_GetFault(), INV_Measure_GetFault());
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
        if (pfc.valid == 0U)
        {
            if ((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
                (pfc_inv_state == PFC_INV_STATE_JOINT_RUN))
            {
                PFC_INV_AppTrip(PFC_INV_FAULT_VBUS_STALE, 0U,
                                INV_FAULT_CONTROL_TIMEOUT);
            }
            else
            {
                INV_HRTIM_DisableOutputs();
                pfc_inv_state = PFC_INV_STATE_ADC_CALIBRATION;
            }
            return;
        }
        if ((pfc_inv_state == PFC_INV_STATE_SAFE) ||
            (pfc_inv_state == PFC_INV_STATE_ADC_CALIBRATION) ||
            (pfc_inv_state == PFC_INV_STATE_STOP))
        {
            pfc_inv_state = PFC_INV_STATE_READY;
        }
        if ((short_press != 0U) && (pfc_inv_state == PFC_INV_STATE_READY))
        {
            if (JointStartInv(&pfc) != HAL_OK)
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

    if (pfc_state == PFC_FAULT_LATCH)
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
            /* 投入后由10 kHz快速路径按欠压、过压和快照停滞执行锁存关断。 */
        }
        else if ((pfc.valid != 0U) &&
            (pfc.vbus >= (joint_pfc_params->vbus_target - joint_pfc_params->vbus_target_tolerance)) &&
            (pfc.vbus <= (joint_pfc_params->vbus_target + joint_pfc_params->vbus_target_tolerance)))
        {
            if (joint_vbus_stable_ms < PFC_INV_VBUS_STABLE_MS) { joint_vbus_stable_ms++; }
            if ((joint_vbus_stable_ms >= PFC_INV_VBUS_STABLE_MS) &&
                (pfc_inv_state != PFC_INV_STATE_INV_SOFT_START) &&
                (pfc_inv_state != PFC_INV_STATE_JOINT_RUN))
            {
                if (JointStartInv(&pfc) != HAL_OK)
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
    if (JointPfcDomainMonitored() != 0U)
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_PFC, pfc_fault_bits, INV_FAULT_NONE);
    }
}

void PFC_INV_AppReportInvFault(uint32_t inv_fault_bits)
{
    if (inv_fault_bits == INV_FAULT_NONE) { return; }
    INV_Measure_LatchFault(inv_fault_bits);
    if (JointInvDomainMonitored() != 0U)
    {
        PFC_INV_AppTrip(PFC_INV_FAULT_INV, PFC_FAULT_NONE, inv_fault_bits);
    }
}

uint8_t PFC_INV_AppWatchdogHealthy(void)
{
    PFC_Measurement pfc;
    INV_Measurement inv;
    uint8_t healthy = 1U;
    PFC_Measure_GetSnapshot(&pfc);
    INV_Measure_GetSnapshot(&inv);

    if (pfc_inv_state == PFC_INV_STATE_FAULT_LATCH || pfc_inv_fault_bits != 0U) { healthy = 0U; }
    if ((JointPfcDomainMonitored() != 0U) &&
        (PFC_AppWatchdogHealthy() == 0U)) { healthy = 0U; }
    if ((JointInvDomainMonitored() != 0U) &&
        ((inv.fast_heartbeat == joint_last_inv_heartbeat) ||
         (inv.adc3_sequence == 0U) || (inv.adc4_sequence == 0U) ||
         (inv.adc5_sequence == 0U))) { healthy = 0U; }
    if (JointInvStageActive() &&
        (((pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
          (pfc_inv_state == PFC_INV_STATE_JOINT_RUN)) !=
         (INV_HRTIM_OutputsEnabled() != 0U))) { healthy = 0U; }
    if ((joint_stage == PFC_INV_STAGE_INV_ONLY) &&
        (pfc.adc1_sequence == joint_last_pfc_sequence)) { healthy = 0U; }
    joint_last_pfc_sequence = pfc.adc1_sequence;
    joint_last_inv_heartbeat = inv.fast_heartbeat;
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
    telemetry->pfc_outputs_enabled = PFC_HRTIM_OutputsEnabled();
    telemetry->inv_outputs_enabled = INV_HRTIM_OutputsEnabled();
    telemetry->vbus_stable = ((pfc_inv_state == PFC_INV_STATE_PFC_VBUS_STABLE) ||
                              (pfc_inv_state == PFC_INV_STATE_INV_SOFT_START) ||
                              (pfc_inv_state == PFC_INV_STATE_JOINT_RUN)) ? 1U : 0U;
    telemetry->healthy = ((pfc_inv_fault_bits == 0U) &&
                          ((JointPfcDomainMonitored() == 0U) ||
                           (telemetry->pfc_fault_bits == 0U)) &&
                          ((JointInvDomainMonitored() == 0U) ||
                           (telemetry->inv_fault_bits == 0U))) ? 1U : 0U;
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
