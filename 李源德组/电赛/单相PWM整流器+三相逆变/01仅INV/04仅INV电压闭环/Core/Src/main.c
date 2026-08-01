/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 三相逆变开环、ADC标定与α/β双PR电压闭环调试入口
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "hrtim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "CBSVPWM.h"
#include "OLED.h"
#include "inv_hrtim.h"
#include "inv_measure.h"
#include "inv_open_loop.h"
#include "inv_user_config.h"
#include "inv_voltage_pr.h"
#include "iwdg.h"
#include "vofa.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/** 应用状态只在本文件内部使用，状态码通过OLED和VOFA对外发布。 */
typedef enum
{
  INV_STATE_SAFE = 0U,
  INV_STATE_ADC_START,
  INV_STATE_ADC_READY,
  INV_STATE_CALIBRATION_BLOCK,
  INV_STATE_PR_RAMP,
  INV_STATE_PR_RUN,
  INV_STATE_FAULT_LATCH
} INV_AppState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#if (INV_USER_OUTPUT_FREQUENCY == 30U)
#define INV_SELECTED_FREQUENCY INV_FREQ_30HZ
#else
#define INV_SELECTED_FREQUENCY INV_FREQ_60HZ
#endif

#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
  #if (INV_USER_CALIBRATION_STEP == INV_CAL_STEP_ZERO)
    #define INV_SELECTED_MEASURE_MODE INV_MEASURE_CALIBRATE_ZERO
  #else
    #define INV_SELECTED_MEASURE_MODE INV_MEASURE_CALIBRATE_GAIN
  #endif
#else
  #define INV_SELECTED_MEASURE_MODE INV_MEASURE_RUN_AUTO_OFFSET
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static float vofa_data[VOFA_MAX_CHANNELS];
static CBSVPWM_t inv_svpwm;
static volatile INV_AppState inv_state = INV_STATE_SAFE;
static volatile uint32_t inv_control_heartbeat;

#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static const INV_OpenLoopConfig inv_open_loop_config =
{
  INV_USER_DC_BUS_V,
  INV_USER_LINE_RMS_V,
  INV_SELECTED_FREQUENCY,
  INV_USER_MODULATION_LIMIT,
  INV_USER_SOFT_START_MS,
  INV_USER_CONTROL_FREQUENCY_HZ
};
#endif

static const INV_MeasureConfig inv_measure_config =
{
  INV_SELECTED_MEASURE_MODE,
  INV_USER_OFFSET_SAMPLES,
  INV_USER_GAIN_SAMPLES,
  INV_USER_CAL_MIN_DELTA_COUNTS,
  {
    INV_USER_IU_OFFSET, INV_USER_IV_OFFSET, INV_USER_IW_OFFSET,
    INV_USER_VUV_OFFSET, INV_USER_VVW_OFFSET, INV_USER_VWU_OFFSET
  },
  {
    INV_USER_IU_SCALE_A_PER_COUNT,
    INV_USER_IV_SCALE_A_PER_COUNT,
    INV_USER_IW_SCALE_A_PER_COUNT,
    INV_USER_VUV_SCALE_V_PER_COUNT,
    INV_USER_VVW_SCALE_V_PER_COUNT,
    INV_USER_VWU_SCALE_V_PER_COUNT
  },
  INV_USER_CALIBRATION_CHANNEL,
  INV_USER_CALIBRATION_REFERENCE,
  INV_USER_ADC_MAX_SKEW_FRAMES,
  INV_USER_ADC_RAIL_LOW_COUNT,
  INV_USER_ADC_RAIL_HIGH_COUNT,
  INV_USER_ADC_RAIL_CONFIRM_FRAMES
};

#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
static const INV_VoltagePRConfig inv_voltage_pr_config =
{
  INV_USER_PR_KP,
  INV_USER_PR_KR,
  (float)INV_USER_OUTPUT_FREQUENCY,
  INV_USER_PR_BANDWIDTH_RAD_S,
  (float)INV_USER_CONTROL_FREQUENCY_HZ,
  INV_USER_PR_CORRECTION_LIMIT_V,
  INV_USER_PR_RECOVERY_VALID_FRAMES,
  (INV_USER_PR_TAKEOVER_MS * INV_USER_CONTROL_FREQUENCY_HZ) / 1000U
};
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void INV_AppLatchFault3(void);
static void INV_AppHandleDiagnostic(uint32_t diagnostic_bits);
static HAL_StatusTypeDef INV_AppWaitForSampling(void);
#if ((INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION) && \
     !((INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR) && \
       (INV_USER_VOLTAGE_CALIBRATION_CONFIRMED == 0U)))
static HAL_StatusTypeDef INV_AppEnableOutputs(void);
#endif
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static void INV_AppRunFastControl(const INV_Measurement *measurement);
#endif
#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
static uint32_t INV_DisplayUnsignedCenti(float value);
static int32_t INV_DisplaySignedCenti(float value);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief 记录一个调试诊断，并按编译开关决定是否升级为软件关断。
 * @param diagnostic_bits INV_DiagnosticMask位组合。
 * @note 默认INV_USER_RELAXED_PR_TEST=1时只记录，PWM继续运行或回退开环。
 */
static void INV_AppHandleDiagnostic(uint32_t diagnostic_bits)
{
  INV_Measure_LatchDiagnostic(diagnostic_bits);
#if (INV_USER_RELAXED_PR_TEST == 0U)
  INV_HRTIM_DisableOutputs();
  INV_Measure_LatchFault(INV_FAULT_SYSTEM);
  INV_OpenLoop_Reset();
  INV_VoltagePR_Reset();
  CBSVPWM_Reset(&inv_svpwm);
  inv_state = INV_STATE_FAULT_LATCH;
#endif
}

/**
 * @brief HRTIM硬件已关断后锁存FLT3，并禁止软件自动重新开放PWM。
 * @note 可从Fault ISR调用；不执行OLED、USART、HAL_Delay或其他阻塞操作。
 */
static void INV_AppLatchFault3(void)
{
  INV_HRTIM_DisableOutputs();
  INV_Measure_LatchFault(INV_FAULT_FLT3);
  INV_OpenLoop_Reset();
  INV_VoltagePR_Reset();
  CBSVPWM_Reset(&inv_svpwm);
  inv_state = INV_STATE_FAULT_LATCH;
}

/** 等待ZERO/GAIN或运行模式自动offset完成，等待期间六路PWM保持关闭。 */
static HAL_StatusTypeDef INV_AppWaitForSampling(void)
{
  uint32_t start_tick = HAL_GetTick();
  INV_Measurement measurement;
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
  INV_CalibrationResult calibration;
#endif

  while ((HAL_GetTick() - start_tick) < INV_USER_STARTUP_TIMEOUT_MS)
  {
    INV_Measure_GetSnapshot(&measurement);
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
    INV_Measure_GetCalibrationResult(&calibration);
    if (calibration.complete != 0U)
    {
      return HAL_OK;
    }
#else
    if ((measurement.offset_ready != 0U) && (measurement.valid != 0U))
    {
      return HAL_OK;
    }
#endif
  }
  INV_AppHandleDiagnostic(INV_DIAG_ADC_SYNC);
  return HAL_TIMEOUT;
}

/** 在50%初始占空比、PB10正常时按用户输出开关开放六路PWM。 */
#if ((INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION) && \
     !((INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR) && \
       (INV_USER_VOLTAGE_CALIBRATION_CONFIRMED == 0U)))
static HAL_StatusTypeDef INV_AppEnableOutputs(void)
{
  if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
      (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET))
  {
    INV_AppLatchFault3();
    return HAL_ERROR;
  }
  if (INV_HRTIM_SetDuty(0.5f, 0.5f, 0.5f) != HAL_OK)
  {
    INV_AppHandleDiagnostic(INV_DIAG_PWM_COMMAND);
    return HAL_ERROR;
  }
#if (INV_USER_ENABLE_PWM_OUTPUT != 0U)
  return INV_HRTIM_EnableOutputs();
#else
  return HAL_OK;
#endif
}
#endif

/**
 * @brief 在一个完整ADC同步帧后运行一次DDS、双PR、CBSVPWM和Compare更新。
 * @param measurement 六通道一致性快照；波形模式可传NULL。
 * @note 运行于DMA ISR，目标频率10 kHz；禁止OLED、USART及阻塞HAL调用。
 */
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static void INV_AppRunFastControl(const INV_Measurement *measurement)
{
  INV_OpenLoopOutput reference;
  float vu_command;
  float vv_command;
  float vw_command;

  if ((inv_state != INV_STATE_PR_RAMP) && (inv_state != INV_STATE_PR_RUN))
  {
    return;
  }
  if (!INV_OpenLoop_Step(&reference))
  {
    INV_AppHandleDiagnostic(INV_DIAG_PARAMETER);
    return;
  }

  vu_command = reference.vu_command;
  vv_command = reference.vv_command;
  vw_command = reference.vw_command;

#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
  {
    INV_VoltagePRTelemetry voltage_control;
    if (!INV_VoltagePR_Step(&reference, measurement, &voltage_control))
    {
      INV_VoltagePR_Reset();
      INV_AppHandleDiagnostic(INV_DIAG_PR_INVALID);
    }
    else
    {
      vu_command = voltage_control.vu_command;
      vv_command = voltage_control.vv_command;
      vw_command = voltage_control.vw_command;
      if (voltage_control.feedback_valid == 0U)
      {
        INV_AppHandleDiagnostic(INV_DIAG_PR_INVALID);
      }
      if (voltage_control.limited != 0U)
      {
        INV_AppHandleDiagnostic(INV_DIAG_LIMITED);
      }
      if ((reference.ramp >= 1.0f) && (voltage_control.takeover >= 1.0f) &&
          (voltage_control.fallback_active == 0U))
      {
        inv_state = INV_STATE_PR_RUN;
      }
    }
  }
#else
  if (reference.ramp >= 1.0f)
  {
    inv_state = INV_STATE_PR_RUN;
  }
#endif

  if (CBSVPWM_Calc3Leg(&inv_svpwm, vu_command, vv_command, vw_command,
                       reference.dc_bus_v) == 0U)
  {
    /* 闭环命令异常时再试一次纯前馈，保证不会把非法数写入HRTIM。 */
    INV_AppHandleDiagnostic(INV_DIAG_SVPWM);
    CBSVPWM_Reset(&inv_svpwm);
    if (CBSVPWM_Calc3Leg(&inv_svpwm,
                         reference.vu_command,
                         reference.vv_command,
                         reference.vw_command,
                         reference.dc_bus_v) == 0U)
    {
      return;
    }
  }
  if (inv_svpwm.limited != 0U)
  {
    INV_AppHandleDiagnostic(INV_DIAG_LIMITED);
  }
  if (INV_HRTIM_SetDuty(inv_svpwm.duty_u, inv_svpwm.duty_v,
                        inv_svpwm.duty_w) != HAL_OK)
  {
    INV_AppHandleDiagnostic(INV_DIAG_PWM_COMMAND);
    return;
  }
  inv_control_heartbeat++;
}
#endif

/** 把非负电压转换为0.01V整数并限制到OLED四位区域。 */
#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
static uint32_t INV_DisplayUnsignedCenti(float value)
{
  float magnitude = (value < 0.0f) ? -value : value;
  if (magnitude > 99.99f)
  {
    magnitude = 99.99f;
  }
  return (uint32_t)(magnitude * 100.0f + 0.5f);
}

/** 把有符号PR校正转换为0.01V整数并限制到OLED三位区域。 */
static int32_t INV_DisplaySignedCenti(float value)
{
  if (value > 9.99f)
  {
    value = 9.99f;
  }
  else if (value < -9.99f)
  {
    value = -9.99f;
  }
  return (int32_t)(value * 100.0f);
}
#endif

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_HRTIM1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART2_UART_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_ADC5_Init();
  /* USER CODE BEGIN 2 */
  INV_HRTIM_DisableOutputs();
  inv_state = INV_STATE_SAFE;

  /* OLED上电等待只执行一次；主循环和10kHz ISR中禁止HAL_Delay()。 */
  HAL_Delay(100U);
  OLED_Init();
#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
  OLED_ShowString(1U, 1U, "UV:");
  OLED_ShowString(1U, 9U, "VW:");
  OLED_ShowString(2U, 1U, "WU:");
  OLED_ShowString(2U, 9U, "RF:");
  OLED_ShowString(3U, 1U, "CA:");
  OLED_ShowString(3U, 9U, "CB:");
  OLED_ShowString(4U, 1U, "S:");
  OLED_ShowString(4U, 5U, "D:");
  OLED_ShowString(4U, 11U, "F:");
#else
  OLED_ShowString(1U, 1U, "IU:");
  OLED_ShowString(1U, 9U, "UV:");
  OLED_ShowString(2U, 1U, "IV:");
  OLED_ShowString(2U, 9U, "VW:");
  OLED_ShowString(3U, 1U, "IW:");
  OLED_ShowString(3U, 9U, "WU:");
  OLED_ShowString(4U, 1U, "S:");
  OLED_ShowString(4U, 5U, "D:");
  OLED_ShowString(4U, 11U, "F:");
#endif

  if (!INV_Measure_Init(&inv_measure_config))
  {
    Error_Handler();
  }

#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  if (!INV_OpenLoop_Init(&inv_open_loop_config) ||
      (CBSVPWM_Init(&inv_svpwm, INV_USER_MODULATION_LIMIT,
                    INV_USER_MINIMUM_DC_V) == 0U))
  {
    Error_Handler();
  }
#endif
#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
  if (!INV_VoltagePR_Init(&inv_voltage_pr_config))
  {
    Error_Handler();
  }
#endif

  inv_state = INV_STATE_ADC_START;
  if ((HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK))
  {
    Error_Handler();
  }

  /* 三个ADC均为双Rank循环DMA；Master启动前先全部进入等待Trigger2状态。 */
  if ((HAL_ADC_Start_DMA(&hadc5, (uint32_t *)INV_Adc5Dma, 2U) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)INV_Adc4Dma, 2U) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)INV_Adc3Dma, 2U) != HAL_OK))
  {
    Error_Handler();
  }
  __HAL_DMA_DISABLE_IT(hadc5.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc4.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc3.DMA_Handle, DMA_IT_HT);

  if (INV_HRTIM_StartTimeBase() != HAL_OK)
  {
    Error_Handler();
  }

#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
  if (INV_AppWaitForSampling() != HAL_OK)
  {
    Error_Handler();
  }
#endif
  inv_state = INV_STATE_ADC_READY;

  /* IWDG在全部阻塞初始化结束后启动；低速任务每100ms固定刷新。 */
  MX_IWDG_Init();

#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
  INV_HRTIM_DisableOutputs();
#elif ((INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR) && \
       (INV_USER_VOLTAGE_CALIBRATION_CONFIRMED == 0U))
  /* 标定未确认时明确停在阻断态，ADC/OLED/VOFA继续运行。 */
  inv_state = INV_STATE_CALIBRATION_BLOCK;
  INV_Measure_LatchDiagnostic(INV_DIAG_PARAMETER);
  INV_HRTIM_DisableOutputs();
#else
  if (INV_AppEnableOutputs() != HAL_OK)
  {
    Error_Handler();
  }
  inv_state = INV_STATE_PR_RAMP;
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t vofa_tick = HAL_GetTick();
  uint32_t oled_tick = HAL_GetTick();
  uint32_t supervisor_tick = HAL_GetTick();
  uint32_t last_adc3_sequence = 0U;
  uint32_t last_adc4_sequence = 0U;
  uint32_t last_adc5_sequence = 0U;
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  uint32_t last_control_heartbeat = 0U;
#endif

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if ((HAL_GetTick() - vofa_tick) >= INV_USER_VOFA_PERIOD_MS)
    {
      INV_Measurement measurement;
      vofa_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);

#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      {
        INV_CalibrationResult calibration;
        INV_Measure_GetCalibrationResult(&calibration);
#if (INV_USER_CALIBRATION_STEP == INV_CAL_STEP_ZERO)
        if (calibration.complete != 0U)
        {
          /* ZERO完成后CH0~5固定发布六路平均offset，用户无需依赖调试器读取。 */
          vofa_data[0] = (float)calibration.offset[0];
          vofa_data[1] = (float)calibration.offset[1];
          vofa_data[2] = (float)calibration.offset[2];
          vofa_data[3] = (float)calibration.offset[3];
          vofa_data[4] = (float)calibration.offset[4];
          vofa_data[5] = (float)calibration.offset[5];
        }
        else
#endif
        {
        vofa_data[0] = (float)measurement.iu_raw;
        vofa_data[1] = (float)measurement.iv_raw;
        vofa_data[2] = (float)measurement.iw_raw;
        vofa_data[3] = (float)measurement.vuv_raw;
        vofa_data[4] = (float)measurement.vvw_raw;
        vofa_data[5] = (float)measurement.vwu_raw;
        }
        vofa_data[6] = calibration.recommended_scale;
        vofa_data[7] = (float)((uint32_t)calibration.complete * 2U +
                              calibration.valid);
      }
#elif (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
      {
        INV_VoltagePRTelemetry control;
        INV_VoltagePR_GetSnapshot(&control);
        vofa_data[0] = control.alpha_reference;
        vofa_data[1] = control.alpha_feedback;
        vofa_data[2] = control.beta_reference;
        vofa_data[3] = control.beta_feedback;
        vofa_data[4] = control.alpha_correction;
        vofa_data[5] = control.beta_correction;
        vofa_data[6] = control.vuv_rms;
        vofa_data[7] = (float)(measurement.diagnostic_bits +
                              (measurement.fault_bits << 16));
      }
#else
      {
        INV_OpenLoopOutput reference;
        INV_OpenLoop_GetSnapshot(&reference);
        vofa_data[0] = reference.sine_u;
        vofa_data[1] = (float)measurement.iu_raw;
        vofa_data[2] = (float)measurement.iv_raw;
        vofa_data[3] = (float)measurement.iw_raw;
        vofa_data[4] = (float)measurement.vuv_raw;
        vofa_data[5] = (float)measurement.vvw_raw;
        vofa_data[6] = (float)measurement.vwu_raw;
        vofa_data[7] = (float)(measurement.diagnostic_bits +
                              (measurement.fault_bits << 16));
      }
#endif
      (void)VOFA_Send(&huart2, vofa_data, VOFA_MAX_CHANNELS);
    }

    if ((HAL_GetTick() - supervisor_tick) >= INV_USER_SUPERVISOR_PERIOD_MS)
    {
      INV_Measurement measurement;
      supervisor_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);

#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
      if ((measurement.adc3_sequence == last_adc3_sequence) ||
          (measurement.adc4_sequence == last_adc4_sequence) ||
          (measurement.adc5_sequence == last_adc5_sequence))
      {
        INV_AppHandleDiagnostic(INV_DIAG_ADC_SYNC);
      }
      last_adc3_sequence = measurement.adc3_sequence;
      last_adc4_sequence = measurement.adc4_sequence;
      last_adc5_sequence = measurement.adc5_sequence;
#endif
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
      if (((inv_state == INV_STATE_PR_RAMP) ||
           (inv_state == INV_STATE_PR_RUN)) &&
          (inv_control_heartbeat == last_control_heartbeat))
      {
        INV_AppHandleDiagnostic(INV_DIAG_CONTROL_TIMEOUT);
      }
      last_control_heartbeat = inv_control_heartbeat;
#endif

      /* 放宽档的诊断和FLT3锁存都继续喂狗，避免自动复位后重新开放PWM。 */
      if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
      {
        Error_Handler();
      }
    }

    if ((HAL_GetTick() - oled_tick) >= INV_USER_OLED_PERIOD_MS)
    {
      INV_Measurement measurement;
      oled_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);
#if (INV_USER_WORK_MODE == INV_MODE_VOLTAGE_PR)
      {
        INV_VoltagePRTelemetry control;
        INV_VoltagePR_GetSnapshot(&control);
        OLED_ShowNum(1U, 4U, INV_DisplayUnsignedCenti(control.vuv_rms), 4U);
        OLED_ShowNum(1U, 12U, INV_DisplayUnsignedCenti(control.vvw_rms), 4U);
        OLED_ShowNum(2U, 4U, INV_DisplayUnsignedCenti(control.vwu_rms), 4U);
        OLED_ShowNum(2U, 12U, INV_DisplayUnsignedCenti(INV_USER_LINE_RMS_V), 4U);
        OLED_ShowSignedNum(3U, 4U,
                           INV_DisplaySignedCenti(control.alpha_correction), 3U);
        OLED_ShowSignedNum(3U, 12U,
                           INV_DisplaySignedCenti(control.beta_correction), 3U);
        if (inv_state == INV_STATE_CALIBRATION_BLOCK)
        {
          /* 当前默认状态：明确提示标定未确认，而不是让用户猜测PWM为何没有输出。 */
          OLED_ShowString(3U, 1U, "CALIBRATION OFF");
        }
      }
#else
      OLED_ShowNum(1U, 4U, measurement.iu_raw, 4U);
      OLED_ShowNum(1U, 12U, measurement.vuv_raw, 4U);
      OLED_ShowNum(2U, 4U, measurement.iv_raw, 4U);
      OLED_ShowNum(2U, 12U, measurement.vvw_raw, 4U);
      OLED_ShowNum(3U, 4U, measurement.iw_raw, 4U);
      OLED_ShowNum(3U, 12U, measurement.vwu_raw, 4U);
#endif
      OLED_ShowNum(4U, 3U, (uint32_t)inv_state, 1U);
      OLED_ShowHexNum(4U, 7U, measurement.diagnostic_bits, 3U);
      OLED_ShowHexNum(4U, 13U, measurement.fault_bits, 2U);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

/**
 * @brief ADC3/4/5规则组DMA完整传输回调。
 * @note 三路序列凑成一个新快照后只运行一次控制；ISR禁止OLED、VOFA和延时。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
#if (INV_USER_WORK_MODE == INV_MODE_PWM_WAVEFORM)
  if ((hadc != NULL) && (hadc->Instance == ADC3))
  {
    INV_AppRunFastControl(NULL);
  }
#else
  INV_Measurement measurement;
  bool frame_ready = false;

  if ((hadc != NULL) && (hadc->Instance == ADC3))
  {
    frame_ready = INV_Measure_OnAdc3Complete(&measurement);
  }
  else if ((hadc != NULL) && (hadc->Instance == ADC4))
  {
    frame_ready = INV_Measure_OnAdc4Complete(&measurement);
  }
  else if ((hadc != NULL) && (hadc->Instance == ADC5))
  {
    frame_ready = INV_Measure_OnAdc5Complete(&measurement);
  }

#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  if (frame_ready)
  {
    INV_AppRunFastControl(&measurement);
  }
#else
  (void)frame_ready;
#endif
#endif
}

/** ADC或DMA错误在默认放宽档只记录；完整保护档会由统一入口关闭PWM。 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && ((hadc->Instance == ADC3) ||
                         (hadc->Instance == ADC4) ||
                         (hadc->Instance == ADC5)))
  {
    INV_AppHandleDiagnostic(INV_DIAG_ADC_ERROR);
  }
}

/** ADC模拟看门狗事件只记录诊断；当前IOC宽窗口本身不构成有效保护。 */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && ((hadc->Instance == ADC3) ||
                         (hadc->Instance == ADC4) ||
                         (hadc->Instance == ADC5)))
  {
    INV_AppHandleDiagnostic(INV_DIAG_ADC_WATCHDOG);
  }
}

/** Fault3硬件先置输出Inactive，本回调随后锁存软件状态。 */
void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
  if ((hhrtim != NULL) && (hhrtim->Instance == HRTIM1))
  {
    INV_AppLatchFault3();
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  INV_HRTIM_DisableOutputs();
  INV_Measure_LatchFault(INV_FAULT_SYSTEM);
  INV_OpenLoop_Reset();
  INV_VoltagePR_Reset();
  CBSVPWM_Reset(&inv_svpwm);
  INV_HRTIM_StopInverterTimers();
  inv_state = INV_STATE_FAULT_LATCH;
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
