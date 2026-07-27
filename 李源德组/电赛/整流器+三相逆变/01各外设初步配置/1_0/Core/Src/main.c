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
#include "OLED.h"
#include "pfc_measure.h"
#include "pfc_hrtim.h"
#include "iwdg.h"
#include "vofa.h"
#include "SPWM.h"  /* HRTIM单相全桥单极性倍频SPWM模块。 */
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

/*
 * VOFA发送数组与VOFA+通道顺序一一对应，固定为8通道：
 * 测试锯齿、三路ADC原始值、三路换算值和故障位。
 */
static float vofa_data[VOFA_MAX_CHANNELS] = {0.0f};

/* 0~99循环计数，经乘以0.01后形成0.00~0.99的1 Hz测试锯齿波。 */
static uint16_t vofa_ramp_index = 0U;

/* 供Keil Watch观察串口HAL调用结果；通信失败不触发功率故障。 */
static uint32_t vofa_tx_ok_count = 0U;
static uint32_t vofa_tx_error_count = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 无功率开环试波使用的四路HRTIM输出；PE0在整个测试过程中保持低电平。 */
#define PFC_OPEN_LOOP_OUTPUTS (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | \
                               HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)

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

	/*
	 * 上电后先等待OLED内部电源和电荷泵稳定。该延时只在初始化阶段执行，
	 * 不能放入主循环或快速采样回调，否则会阻塞后台任务或控制节拍。
	 */
	HAL_Delay(100);
	OLED_Init();

	/* 初始化测量模块的软件状态、DMA样本快照、序列号和故障标志。 */
	PFC_Measure_Init();

	/*
	 * 固定标签只写一次，避免软件I2C在主循环中重复传输不变内容：
	 * I：IPFC输入电感电流；V：VAC交流输入电压；
	 * D：VBUS直流母线电压；S：快速采样心跳，行末显示故障位。
	 * 各测量行左侧为ADC原始码，右侧为换算后的工程量。
	 * 采样心跳 是周期递增或周期变化的计数，用于确认采样、通信和主循环没有卡死。
	 * 故障码 是当前或历史保护状态，例如过流、母线过压/欠压、输入欠压、过温、驱动异常等。
	 *	具体每个数值或位的含义，需要看该项目的故障码定义表。
	 */
	OLED_ShowString(1, 1, "I:");
	OLED_ShowString(2, 1, "V:");
	OLED_ShowString(3, 1, "D:");
	OLED_ShowString(4, 1, "S:");


  /*
   * 无功率采样阶段只验证HRTIM触发、ADC和DMA链路。
   * PE0保持低电平，防止仅启动HRTIM计数器时误使能外部门极驱动器。
   */
  HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);

  /* ADC校准必须在启动规则组DMA之前完成，此时HRTIM尚未产生ADC触发。 */
  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /* 再校准IPFC和VBUS使用的ADC1。 */
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * ADC2规则组只有VAC一个Rank。先启动ADC2 DMA，使其提前进入等待
   * HRTIM_TRG1的状态；之后每次触发完成都更新VAC及adc2_sequence。
   */
  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)PFC_Adc2Dma, 1U) != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * ADC1规则组依次采集IPFC和VBUS，DMA长度2必须与Rank数量一致。
   * ADC1完成回调在两个Rank均搬运结束后发生，并作为唯一10 kHz快速入口；
   * ADC2回调只发布VAC序列，不能再次执行同一周期的控制或测量处理。
   */
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)PFC_Adc1Dma, 2U) != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * 循环DMA默认会产生Half Transfer和Transfer Complete两类中断。
   * 本工程只在一整个规则序列完成后处理样本，因此关闭HT中断，
   * 避免ADC1 Rank 1完成时提前读取尚未更新的VBUS并重复进入回调。
   */
  __HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);

  /* 使用10%调制度初始化50 Hz开环正弦，初始比较值保持两个桥臂均为50%。 */
  SPWM_Init(0.10f);

  /*
   * 启动Master、Timer A/B计数器以产生10 kHz ADC Trigger。
   * 此函数只建立采样时基，不开放TA1/TA2/TB1/TB2输出，也不拉高PE0。
   */
  if (PFC_HRTIM_StartSampling() != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_HRTIM);
    Error_Handler();
  }

  /*
   * 只开放MCU的四路HRTIM引脚供示波器观察，PE0仍保持低电平，
   * 因此本阶段不会使能外部门极驱动器或接入实际交流功率级。
   */
  if (HAL_HRTIM_WaveformOutputStart(&hhrtim1, PFC_OPEN_LOOP_OUTPUTS) != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_HRTIM);
    Error_Handler();
  }

  /*
   * IWDG放在可能耗时或失败的初始化之后启动。启动后不能关闭，只有在
   * ADC序列、测量快照和故障状态均正常时才刷新，异常时允许其复位系统。
   */
  MX_IWDG_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  /* OLED属于低速后台任务，不参与10 kHz采样回调。 */
  uint32_t oled_tick = HAL_GetTick();

  /* VOFA以10 ms为发送周期，即每秒发送约100帧。 */
  uint32_t vofa_tick = HAL_GetTick();

  /* 保存上一次监督时刻的序列号，用来判断两路DMA是否仍在持续更新。 */
  uint32_t last_adc1_sequence = 0U;
  uint32_t last_adc2_sequence = 0U;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	/*
	 * 每10 ms发送一次VOFA数据。阻塞式USART只允许放在主循环，
	 * 不能放进10 kHz ADC DMA回调，否则会破坏快速采样和后续控制节拍。
	 */
	if ((HAL_GetTick() - vofa_tick) >= 10U)
	{
		PFC_Measurement vofa_measurement;

		/* 以当前时刻为新基准，主循环偶尔延迟时不连续补发旧帧。 */
		vofa_tick = HAL_GetTick();

		/* 读取ISR已经发布的完整快照，不直接读取正在由DMA改写的数组。 */
		PFC_Measure_GetSnapshot(&vofa_measurement);

		/* CH0：独立于ADC的已知锯齿波，用来直接判断USART和VOFA链路。 */
		vofa_data[0] = (float)vofa_ramp_index * 0.01f;
		vofa_ramp_index++;
		if (vofa_ramp_index >= 100U)
		{
			vofa_ramp_index = 0U;
		}

		/* CH1~CH3：原始码值，便于先验证ADC数据是否随输入变化。 */
		vofa_data[1] = (float)vofa_measurement.ipfc_raw;
		vofa_data[2] = (float)vofa_measurement.vac_raw;
		vofa_data[3] = (float)vofa_measurement.vbus_raw;

		/* CH4~CH6：换算后的电流/电压，比例系数后续按模拟前端实测修改。 */
		vofa_data[4] = vofa_measurement.ipfc;
		vofa_data[5] = vofa_measurement.vac;
		vofa_data[6] = vofa_measurement.vbus;

		/* CH7：锁存故障位，正常为0；转为float仅用于JustFloat显示。 */
		vofa_data[7] = (float)vofa_measurement.fault_bits;

		/*
		 * HAL_OK只能说明MCU已完成串口发送，最终链路仍需观察VOFA曲线确认。
		 * 调试通信不是保护条件，因此发送失败只计数，不停ADC/HRTIM也不喂故障。
		 */
		if (VOFA_Send(&huart2, vofa_data, VOFA_MAX_CHANNELS) == HAL_OK)
		{
			vofa_tx_ok_count++;
		}
		else
		{
			vofa_tx_error_count++;
		}
	}
	  
	/*
	 * 每100 ms执行一次低速显示和安全监督。这里使用无阻塞的Tick差值，
	 * 而不是HAL_Delay()，以便主循环后续继续加入状态机和通信任务。
	 */
	if ((HAL_GetTick() - oled_tick) >= 100U)
    {
		/* 记录本次实际执行时刻；OLED写屏超时后不连续追赶历史周期。 */
		oled_tick = HAL_GetTick();
        PFC_Measurement measurement;
        int32_t ipfc_ma;
        int32_t vac_cv;
        uint32_t vbus_cv;

        /*
         * 主循环不直接读取正在被DMA改写的PFC_Adc1Dma/PFC_Adc2Dma。
         * 通过测量模块取得同一发布时刻的完整快照，避免显示半更新数据。
         */
        PFC_Measure_GetSnapshot(&measurement);

        /*
         * 屏幕显示使用整数，避免在软件I2C显示函数中处理浮点格式化：
         * ipfc_ma单位为mA，vac_cv和vbus_cv单位均为0.01 V。
         */
        ipfc_ma = (int32_t)(measurement.ipfc * 1000.0f);
        vac_cv = (int32_t)(measurement.vac * 100.0f);
        vbus_cv = (uint32_t)(measurement.vbus * 100.0f);

        /* 固定标签已在初始化阶段写入，此处只更新会变化的数字区域。 */
        OLED_ShowNum(1, 3, measurement.ipfc_raw, 4);
        OLED_ShowSignedNum(1, 8, ipfc_ma, 5);

        OLED_ShowNum(2, 3, measurement.vac_raw, 4);
        OLED_ShowSignedNum(2, 8, vac_cv, 5);

        OLED_ShowNum(3, 3, measurement.vbus_raw, 4);
        OLED_ShowNum(3, 8, vbus_cv, 5);

        OLED_ShowNum(4, 3, measurement.fast_heartbeat % 100000U, 5);
        OLED_ShowHexNum(4, 10, measurement.fault_bits, 2);

        /*
         * 两个序列号在相邻监督周期之间都必须前进。任一路停滞说明ADC、
         * DMA或HRTIM触发链路异常，锁存同步故障并由测量模块执行安全停机。
         */
        if ((measurement.adc1_sequence == last_adc1_sequence) ||
            (measurement.adc2_sequence == last_adc2_sequence))
        {
            PFC_Measure_Trip(PFC_FAULT_ADC_SYNC);
        }
        else if ((measurement.valid != 0U) &&
                 (measurement.fault_bits == PFC_FAULT_NONE))
        {
            /* 仅健康状态喂狗；不要在中断中或无条件刷新IWDG。 */
            (void)HAL_IWDG_Refresh(&hiwdg);
        }

        /* 保存本次序列号，供下一个100 ms监督周期比较。 */
        last_adc1_sequence = measurement.adc1_sequence;
        last_adc2_sequence = measurement.adc2_sequence;

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
}

/* USER CODE BEGIN 4 */

/**
 * @brief ADC规则组DMA完整传输回调。
 * @param hadc 发生完整转换事件的ADC句柄。
 *
 * 两路ADC均由HRTIM Master CMP2通过HRTIM_TRG1以10 kHz触发。
 * ADC2先完成单通道VAC采样并发布序列号；ADC1完成IPFC、VBUS两个Rank后，
 * 检查ADC2的新样本并形成一次同步测量快照。控制算法后续也应只从ADC1
 * 分支进入，避免一个PWM周期执行两次。
 *
 * 该函数运行在DMA中断上下文，禁止调用OLED、阻塞式USART或HAL_Delay()。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && (hadc->Instance == ADC2))
  {
    PFC_Measure_OnAdc2Complete();
  }
  else if ((hadc != NULL) && (hadc->Instance == ADC1))
  {
    PFC_Measure_OnAdc1Complete();

    /* ADC1完整序列是唯一10 kHz更新入口；锁存故障后不得继续推进SPWM。 */
    if (PFC_Measure_GetFault() == PFC_FAULT_NONE)
    {
      SPWM_OpenLoopStep();
    }
  }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  /*
   * ADC发生Overrun、DMA或内部状态错误时进入。当前不区分具体ADC，
   * 统一交给测量模块锁存ADC故障；功率控制中不能尝试自动恢复输出。
   */
  (void)hadc;
  PFC_Measure_AdcError();
}

/**
 * @brief GPIO外部中断公共回调。
 * @param GPIO_Pin 触发EXTI的GPIO引脚掩码。
 *
 * PE5是驱动器nFAULT诊断信号。该回调用于记录和锁存故障；真正的快速
 * 关断应由驱动器DESAT/OCP到HRTIM Fault 3的硬件链路完成，不依赖CPU。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == PFC_NFAULT_DIAG_Pin)
  {
    PFC_Measure_Trip(PFC_FAULT_DRIVER);
  }
}

void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
  /*
   * Fault 3硬件逻辑已先把HRTIM输出置为Inactive；ISR随后记录故障状态，
   * 供主循环显示和状态机处理。清除中断标志不代表允许自动恢复PWM。
   */
  (void)hhrtim;
  PFC_Measure_Trip(PFC_FAULT_HRTIM);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /*
   * 不可恢复的初始化或HAL错误统一进入安全态：先关闭全部HRTIM计数和
   * 输出，再禁止中断并停留；若IWDG已经启动，最终由IWDG复位系统。
   * 板级Gate Enable还应保持默认低。
   */
  PFC_HRTIM_StopAll();
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
