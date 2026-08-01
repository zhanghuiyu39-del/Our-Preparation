/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : PFC双闭环 + 三相INV开环联合验证主程序
  ******************************************************************************
  */
/*
 * 本文件只负责CubeMX外设装配、五路ADC/DMA回调转发、低速显示通信和IWDG监督，
 * 不在主循环中直接读取DMA原始数组，也不在ISR中执行OLED、阻塞USART或HAL_Delay。
 * 联合数据流：HRTIM Master CMP2 -> ADC1/2和ADC3/4/5 -> 各自一致性快照 ->
 * PFC 10 kHz PR/1 kHz PI、INV 10 kHz CBSVPWM -> A~E Compare预装载。
 * 默认pfc_inv_user_config.h为RAW_ADC且PWM=0，标定前所有功率输出均被禁止。
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "hrtim.h"
#include "iwdg.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "OLED.h"
#include "vofa.h"
#include "pfc_inv_user_config.h"
#include "pfc_inv_app.h"
#include "pfc_params.h"
#include "pfc_measure.h"
#include "pfc_control.h"
#include "pfc_hrtim.h"
#include "inv_measure.h"
#include "inv_hrtim.h"
#include "inv_open_loop.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* VOFA为阻塞发送，只由主循环每10 ms填充和发送；ISR不得访问该缓存。 */
static float vofa_data[VOFA_MAX_CHANNELS] = {0.0f};
static uint8_t vofa_raw_page = 0U;
static const PFC_Params *active_pfc_params = NULL;
/* 复位来源快照只在初始化阶段写入，供PFC状态机区分IWDG复位后的等待时间。 */
static uint8_t iwdg_reset_seen = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void JointOledInitLabels(void)
{
  OLED_ShowString(1U, 1U, "P:     I:     ");
  OLED_ShowString(2U, 1U, "B:     U:     ");
  OLED_ShowString(3U, 1U, "S:     D:     ");
  OLED_ShowString(4U, 1U, "F:     H:     ");
}

/**
 * @brief 将完整PFC/INV快照转换为8通道VOFA数据。
 * @note 只由主循环调用；RAW_ADC模式每次在PFC统计页和INV六通道页之间切换，
 *       避免在8通道协议中丢失关键校准量。CH0页号为0或1，便于VOFA识别当前页。
 */
static void JointFillVofa(const PFC_Measurement *pfc,
                          const PFC_AdcCalibrationStats *pfc_stats,
                          const INV_Measurement *inv,
                          const PFC_INV_Telemetry *joint)
{
  INV_OpenLoopOutput reference;
  if ((pfc == NULL) || (pfc_stats == NULL) || (inv == NULL) || (joint == NULL)) { return; }

  if (PFC_INV_STAGE == PFC_INV_STAGE_RAW_ADC)
  {
    if (vofa_raw_page == 0U)
    {
      vofa_data[0] = 0.0f;
      vofa_data[1] = (float)pfc_stats->ipfc_mean;
      vofa_data[2] = (float)pfc_stats->ipfc_min;
      vofa_data[3] = (float)pfc_stats->ipfc_max;
      vofa_data[4] = (float)pfc_stats->vac_mean;
      vofa_data[5] = (float)pfc_stats->vac_min;
      vofa_data[6] = (float)pfc_stats->vac_max;
      vofa_data[7] = (float)pfc_stats->vbus_mean;
    }
    else
    {
      vofa_data[0] = 1.0f;
      vofa_data[1] = (float)inv->iu_raw;
      vofa_data[2] = (float)inv->iv_raw;
      vofa_data[3] = (float)inv->iw_raw;
      vofa_data[4] = (float)inv->vuv_raw;
      vofa_data[5] = (float)inv->vvw_raw;
      vofa_data[6] = (float)inv->vwu_raw;
      vofa_data[7] = (float)inv->fault_bits;
    }
    vofa_raw_page ^= 1U;
  }
  else
  {
    INV_OpenLoop_GetSnapshot(&reference);
    vofa_data[0] = pfc->vac;
    vofa_data[1] = pfc->ipfc;
    vofa_data[2] = pfc->vbus;
    vofa_data[3] = inv->vuv;
    vofa_data[4] = reference.sine_u;
    vofa_data[5] = joint->inv_modulation;
    vofa_data[6] = (float)joint->state;
    vofa_data[7] = (float)(joint->joint_fault_bits | joint->pfc_fault_bits | joint->inv_fault_bits);
  }
}

/** @brief 主循环每100 ms更新数字区域；所有输入均为已发布快照。 */
static void JointRefreshOled(const PFC_Measurement *pfc,
                             const INV_Measurement *inv,
                             const PFC_INV_Telemetry *joint)
{
  if ((pfc == NULL) || (inv == NULL) || (joint == NULL)) { return; }
  OLED_ShowNum(1U, 3U, pfc->vac_raw, 4U);
  OLED_ShowNum(1U, 11U, pfc->ipfc_raw, 4U);
  OLED_ShowNum(2U, 3U, pfc->vbus_raw, 4U);
  OLED_ShowNum(2U, 11U, inv->iu_raw, 4U);
  OLED_ShowNum(3U, 3U, (uint32_t)joint->state, 2U);
  OLED_ShowNum(3U, 11U, (uint32_t)(joint->inv_modulation * 1000.0f), 4U);
  OLED_ShowHexNum(4U, 3U, joint->joint_fault_bits | joint->pfc_fault_bits | joint->inv_fault_bits, 5U);
  OLED_ShowNum(4U, 11U, joint->inv_heartbeat % 100000U, 5U);
}
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
  /*
   * CubeMX初始化结束后立即关闭A~E全部输出。计数器、PWM引脚和外部驱动工作是三个
   * 不同状态；下面只允许在五路DMA和联合状态机准备完成后开放对应PWM引脚。
   */
  PFC_INV_AppEmergencyOff();
  PFC_HRTIM_StopPower();
  INV_HRTIM_DisableOutputs();

  iwdg_reset_seen = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ? 1U : 0U;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  active_pfc_params = PFC_Params_GetActive();

  /* OLED软件I2C需要上电稳定时间；延时只允许出现在启动阶段。 */
  HAL_Delay(100U);
  OLED_Init();
  JointOledInitLabels();

  if ((active_pfc_params == NULL) ||
      (PFC_INV_AppInit(active_pfc_params, iwdg_reset_seen) != HAL_OK))
  {
    Error_Handler();
  }

  /* ADC校准必须早于DMA启动，顺序与功率域无关；此时所有PWM仍保持关闭。 */
  if ((HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) ||
      (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK))
  {
    PFC_INV_AppTrip(PFC_INV_FAULT_SYSTEM, PFC_FAULT_ADC_ERROR,
                    INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /* 标定未确认时两个接口保留CubeMX宽窗口；确认后才写入运行期AWD阈值。 */
  if ((PFC_Measure_ConfigureWatchdogs(&hadc1, &hadc2) != HAL_OK) ||
      (INV_Measure_ConfigureWatchdogs(&hadc3, &hadc4, &hadc5) != HAL_OK))
  {
    PFC_INV_AppTrip(PFC_INV_FAULT_SYSTEM, PFC_FAULT_ADC_ERROR,
                    INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * 先启动所有DMA，让ADC在HRTIM触发到来前进入等待状态。每个数组长度必须与规则组
   * Rank数一致；ADC1完整帧是PFC 10 kHz入口，ADC3/4/5凑齐后是INV 10 kHz入口。
   */
  if ((HAL_ADC_Start_DMA(&hadc5, (uint32_t *)INV_Adc5Dma, 2U) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)INV_Adc4Dma, 2U) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)INV_Adc3Dma, 2U) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)PFC_Adc2Dma, 1U) != HAL_OK) ||
      (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)PFC_Adc1Dma, 2U) != HAL_OK))
  {
    PFC_INV_AppTrip(PFC_INV_FAULT_SYSTEM, PFC_FAULT_ADC_ERROR,
                    INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /* Half Transfer不是完整Rank帧，必须关闭，防止同一PWM周期重复执行控制。 */
  __HAL_DMA_DISABLE_IT(hadc5.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc4.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc3.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);

  /* 联合层一次性启动Master和A~E计数器；该调用仍不开放任何PWM输出引脚。 */
  if (PFC_INV_AppStartSharedTimebase() != HAL_OK)
  {
    PFC_INV_AppTrip(PFC_INV_FAULT_HRTIM, PFC_FAULT_HRTIM, INV_FAULT_HRTIM);
    Error_Handler();
  }

  /* 给五路DMA最多200 ms建立序列；失败表示触发、DMA或Rank配置不可用。 */
  {
    uint32_t sampling_start_tick = HAL_GetTick();
    while (PFC_INV_AppSamplingReady() == 0U)
    {
      if ((HAL_GetTick() - sampling_start_tick) >= 200U)
      {
        PFC_INV_AppTrip(PFC_INV_FAULT_SEQUENCE, PFC_FAULT_ADC_SYNC,
                        INV_FAULT_ADC_SYNC);
        Error_Handler();
      }
    }
  }

  /* IWDG最后初始化；之后只有联合监督确认活动域均前进时才刷新。 */
  MX_IWDG_Init();
  __HAL_DBGMCU_FREEZE_IWDG();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t app_tick = HAL_GetTick();
  uint32_t vofa_tick = HAL_GetTick();
  uint32_t oled_tick = HAL_GetTick();
  uint32_t supervisor_tick = HAL_GetTick();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    while ((HAL_GetTick() - app_tick) >= 1U)
    {
      app_tick++;
      PFC_INV_AppTick1ms();
    }

    if ((HAL_GetTick() - vofa_tick) >= PFC_INV_VOFA_PERIOD_MS)
    {
      PFC_Measurement pfc;
      PFC_AdcCalibrationStats pfc_stats;
      INV_Measurement inv;
      PFC_INV_Telemetry joint;

      vofa_tick = HAL_GetTick();
      PFC_Measure_GetSnapshot(&pfc);
      PFC_Measure_GetCalibrationStats(&pfc_stats);
      INV_Measure_GetSnapshot(&inv);
      PFC_INV_AppGetTelemetry(&joint);
      JointFillVofa(&pfc, &pfc_stats, &inv, &joint);
      /* USART发送失败不参与功率许可，避免调试线断开导致PWM停机。 */
      (void)VOFA_Send(&huart2, vofa_data, VOFA_MAX_CHANNELS);
    }

    if ((HAL_GetTick() - oled_tick) >= PFC_INV_OLED_PERIOD_MS)
    {
      PFC_Measurement pfc;
      INV_Measurement inv;
      PFC_INV_Telemetry joint;

      oled_tick = HAL_GetTick();
      PFC_Measure_GetSnapshot(&pfc);
      INV_Measure_GetSnapshot(&inv);
      PFC_INV_AppGetTelemetry(&joint);
      JointRefreshOled(&pfc, &inv, &joint);
    }

    if ((HAL_GetTick() - supervisor_tick) >= PFC_INV_SUPERVISOR_PERIOD_MS)
    {
      supervisor_tick = HAL_GetTick();
      if (PFC_INV_AppWatchdogHealthy() != 0U)
      {
        (void)HAL_IWDG_Refresh(&hiwdg);
      }
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
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance == NULL)) { return; }
  if (hadc->Instance == ADC2)
  {
    PFC_Measure_OnAdc2Complete();
  }
  else if (hadc->Instance == ADC1)
  {
    PFC_Measure_OnAdc1Complete();
    if (PFC_Measure_GetFault() == PFC_FAULT_NONE) { PFC_INV_AppFastPfcStep(); }
  }
  else if (hadc->Instance == ADC3)
  {
    INV_Measurement frame;
    if (INV_Measure_OnAdc3Complete(&frame)) { PFC_INV_AppFastInvStep(&frame); }
  }
  else if (hadc->Instance == ADC4)
  {
    INV_Measurement frame;
    if (INV_Measure_OnAdc4Complete(&frame)) { PFC_INV_AppFastInvStep(&frame); }
  }
  else if (hadc->Instance == ADC5)
  {
    INV_Measurement frame;
    if (INV_Measure_OnAdc5Complete(&frame)) { PFC_INV_AppFastInvStep(&frame); }
  }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance == ADC1) || (hadc->Instance == ADC2))
  {
    PFC_INV_AppReportPfcFault(PFC_FAULT_ADC_ERROR);
  }
  else
  {
    PFC_INV_AppReportInvFault(INV_FAULT_ADC_ERROR);
  }
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance == ADC1))
  {
#if PFC_USER_RELAXED_PWM_TEST == 0U
    PFC_INV_AppReportPfcFault(PFC_FAULT_AWD_IPFC | PFC_FAULT_OVERCURRENT);
#endif
  }
  else if (hadc->Instance == ADC2)
  {
#if PFC_USER_RELAXED_PWM_TEST == 0U
    PFC_INV_AppReportPfcFault(PFC_FAULT_AWD_VAC | PFC_FAULT_VAC_LOST);
#endif
  }
  else
  {
#if INV_USER_RELAXED_MONITORING == 0U
    PFC_INV_AppReportInvFault(INV_FAULT_ADC_WATCHDOG);
#endif
  }
}

void HAL_ADCEx_LevelOutOfWindow2Callback(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance == ADC1))
  {
#if PFC_USER_RELAXED_PWM_TEST == 0U
    PFC_INV_AppReportPfcFault(PFC_FAULT_AWD_VBUS | PFC_FAULT_VBUS_OV);
#endif
  }
  else
  {
#if INV_USER_RELAXED_MONITORING == 0U
    PFC_INV_AppReportInvFault(INV_FAULT_ADC_WATCHDOG);
#endif
  }
}

void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
  (void)hhrtim;
  PFC_INV_AppTrip(PFC_INV_FAULT_HRTIM, PFC_FAULT_HRTIM, INV_FAULT_HRTIM);
}

void HAL_RCC_CSSCallback(void)
{
  PFC_INV_AppTrip(PFC_INV_FAULT_SYSTEM, PFC_FAULT_HRTIM, INV_FAULT_CLOCK);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 不可恢复HAL错误先通过寄存器关闭A~E，再停止中断并等待IWDG复位。 */
  PFC_INV_AppEmergencyOff();
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
