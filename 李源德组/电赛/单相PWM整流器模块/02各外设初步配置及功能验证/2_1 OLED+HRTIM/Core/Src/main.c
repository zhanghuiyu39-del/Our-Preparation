/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#include "SPWM.h"  /* HRTIM单相全桥单极性倍频SPWM模块。 */
#include "OLED.h"
#include <stdio.h>
#include "iwdg.h"
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


	//gpt_hrtim_fault3_test---------------
	volatile uint32_t hrtim_fault3_count = 0U;
	volatile uint8_t hrtim_fault3_seen = 0U;






/* ADC1循环DMA缓冲区：下标0为IPFC，下标1为VBUS。 */
static volatile uint16_t adc1_dma_buffer[2] = {0U, 0U};

/* ADC2循环DMA缓冲区：下标0为VAC。 */
static volatile uint16_t adc2_dma_buffer[1] = {0U};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 四路HRTIM输出掩码，统一用于启动和发生错误时关闭输出。 */
#define SPWM_TEST_OUTPUTS (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | \
                           HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)

/* Master、Timer A和Timer B必须用同一个掩码同步启动。 */
#define SPWM_TEST_TIMERS  (HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | \
                           HRTIM_TIMERID_TIMER_B)

/*
 * 无功率测试启动失败时执行统一安全收尾。
 * 本函数始终先拉低门极使能，再请求关闭全部HRTIM输出。
 */
static void SPWM_TestFailSafe(void)
{
  HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET); /* 禁止外部门极驱动。 */
  (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, SPWM_TEST_OUTPUTS);            /* 关闭四路MCU PWM输出。 */
  Error_Handler();                                                           /* 停止程序并等待调试。 */
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
  /* USER CODE BEGIN 2 */


	HAL_Delay(100);
	OLED_Init();
	//此处用来放需要显示的名称
//	OLED_ShowNum(1,7,1234,4);
//	OLED_ShowNum(4,7,1234,4);


  /* 无功率示波器测试期间始终保持外部门极驱动关闭。 */
  HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);

  /* 先校准单通道VAC使用的ADC2；校准期间HRTIM尚未启动。 */
  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
  {
    SPWM_TestFailSafe(); /* ADC2校准失败时禁止继续启动PWM资源。 */
  }

  /* 再校准IPFC和VBUS使用的ADC1。 */
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    SPWM_TestFailSafe(); /* ADC1校准失败时禁止继续启动PWM资源。 */
  }

  /* 先让ADC2进入等待HRTIM触发状态，保证VAC序列先完成。 */
  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buffer, 1U) != HAL_OK)
  {
    SPWM_TestFailSafe(); /* ADC2 DMA启动失败时保持安全态。 */
  }

  /* 再让ADC1进入等待HRTIM触发状态；ADC1完成回调是唯一SPWM更新入口。 */
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_dma_buffer, 2U) != HAL_OK)
  {
    SPWM_TestFailSafe(); /* ADC1 DMA启动失败时保持安全态。 */
  }

  /* 首版控制只使用完整转换回调，关闭HAL默认打开的DMA半传输中断。 */
  __HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);

  /* 使用10%调制度初始化50 Hz开环正弦，初始比较值保持两个桥臂均为50%。 */
  SPWM_Init(0.10f);

  /* 同步启动Master、Timer A和Timer B，使两桥臂与ADC触发保持同一时基。 */
  if (HAL_HRTIM_WaveformCounterStart(&hhrtim1, SPWM_TEST_TIMERS) != HAL_OK)
  {
    SPWM_TestFailSafe(); /* 任一计数器启动失败时关闭全部输出。 */
  }

  /* 只开放MCU的四路HRTIM引脚供示波器观察，PE0仍保持低电平。 */
  if (HAL_HRTIM_WaveformOutputStart(&hhrtim1, SPWM_TEST_OUTPUTS) != HAL_OK)
  {
    SPWM_TestFailSafe(); /* 四路输出未完整启动时立即回到安全态。 */
  }

  MX_IWDG_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t oled_tick = 0;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  
	  //此处用来显示需要实时刷新的数据   eg：ADC
	if ((HAL_GetTick() - oled_tick) >= 100U)
    {
        oled_tick = HAL_GetTick();
//		OLED_ShowNum(1, 7, adc_value, 4);
        OLED_ShowString(2, 1, "OLED OK");

    }
	  
    (void)HAL_IWDG_Refresh(&hiwdg); /* 无功率试波阶段持续喂狗，避免约500 ms后复位。 */

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
}

/* USER CODE BEGIN 4 */


//gpt_hrtim_fault3_test-----------------------------
void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
    if ((hhrtim != NULL) && (hhrtim->Instance == HRTIM1))
    {
        /* 即使硬件已经关闭PWM，软件仍再次确保驱动使能为低。 */
        HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port,
                          PFC_GATE_EN_Pin,
                          GPIO_PIN_RESET);

        hrtim_fault3_seen = 1U;
        hrtim_fault3_count++;
    }
}




/*
 * 两路ADC都由HRTIM Master CMP2以10 kHz触发。
 * 只有ADC1完整序列完成时推进一次SPWM，ADC2完成回调不执行调制更新。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && (hadc->Instance == ADC1)) /* 确认本次是ADC1两通道序列完成。 */
  {
    SPWM_OpenLoopStep(); /* 每100 us推进两个查表索引，得到50 Hz正弦包络。 */
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
