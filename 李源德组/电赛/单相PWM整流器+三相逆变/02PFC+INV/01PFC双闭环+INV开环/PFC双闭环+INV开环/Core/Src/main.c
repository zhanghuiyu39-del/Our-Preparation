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
static uint32_t vofa_tx_ok_count = 0U;
static uint32_t vofa_tx_error_count = 0U;
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

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
    PFC_INV_AppReportPfcFault(PFC_FAULT_AWD_IPFC | PFC_FAULT_OVERCURRENT);
  }
  else if (hadc->Instance == ADC2)
  {
    PFC_INV_AppReportPfcFault(PFC_FAULT_AWD_VAC | PFC_FAULT_VAC_LOST);
  }
  else
  {
    PFC_INV_AppReportInvFault(INV_FAULT_ADC_WATCHDOG);
  }
}

void HAL_ADCEx_LevelOutOfWindow2Callback(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance == ADC1))
  {
    PFC_INV_AppReportPfcFault(PFC_FAULT_AWD_VBUS | PFC_FAULT_VBUS_OV);
  }
  else
  {
    PFC_INV_AppReportInvFault(INV_FAULT_ADC_WATCHDOG);
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
  /* User can add his own implementation to report the HAL error return state */
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
