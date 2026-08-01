/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 5 V单相PWM整流器PI+PR双闭环验证主程序
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
/*
 * 本文件负责CubeMX外设初始化、PFC应用模块装配、低速显示/通信调度以及HAL回调转发，
 * 不在主循环中直接计算PWM调制，也不直接解释DMA缓冲区中的半更新数据。
 *
 * 启动数据流：GPIO安全态 -> ADC校准/AWD -> 双ADC循环DMA -> HRTIM采样时基 -> IWDG。
 * 运行数据流：HRTIM TRG1 -> ADC/DMA回调 -> 测量快照/10 kHz控制；主循环只处理
 * 1 ms状态机、10 ms VOFA、100 ms OLED和100 ms安全监督。
 *
 * 当前板卡没有MCU可控Gate Enable。打开TA1/TA2/TB1/TB2即可能驱动功率器件；
 * 默认RAW_ADC模式只发布原始统计且禁止PWM；工程量复核和闭环许可由用户配置分层控制。
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
#include "pfc_params.h"
#include "pfc_app.h"
#include "pfc_control.h"
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
 * VOFA JustFloat发送缓存，由主循环每10 ms写入并阻塞发送，固定使用8个通道。
 * 三种运行模式的通道映射由PFC_DebugFillVofa()统一生成，避免现场调试时散改main。
 * CH7始终编码PFC状态与完整故障位；CH8/CH9用于统计或输入功率诊断。
 * 详细通道定义见项目Markdown使用说明。
 */
static float vofa_data[VOFA_MAX_CHANNELS] = {0.0f};

/* VOFA发送结果累计值，仅由主循环写，供Keil Watch判断调试串口是否持续工作。 */
static uint32_t vofa_tx_ok_count = 0U;
static uint32_t vofa_tx_error_count = 0U;

/* 当前编译期参数档的常驻只读地址；初始化后由main和各PFC模块共同读取。 */
static const PFC_Params *pfc_params = NULL;

/* 上电复位来源快照：1表示IWDG复位；只在初始化时写入并传给状态机。 */
static uint8_t pfc_iwdg_reset_seen = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void PFC_DebugInitOledLabels(PFC_RunMode mode);
static void PFC_DebugFillVofa(const PFC_Measurement *measurement,
                              const PFC_AdcCalibrationStats *stats,
                              const PFC_ControlTelemetry *control);
static void PFC_DebugRefreshOled(const PFC_Measurement *measurement,
                                 const PFC_AdcCalibrationStats *stats,
                                 const PFC_ControlTelemetry *control);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/** @brief 按编译期运行模式写入OLED固定标签；只在上电初始化阶段调用一次。 */
static void PFC_DebugInitOledLabels(PFC_RunMode mode)
{
  if (mode == PFC_RUN_MODE_RAW_ADC)
  {
    OLED_ShowString(1, 1, "I:    /    /    ");
    OLED_ShowString(2, 1, "V:    /    /    ");
    OLED_ShowString(3, 1, "D:    W:     ");
    OLED_ShowString(4, 1, "S:  F:     ");
  }
  else if (mode == PFC_RUN_MODE_ENGINEERING_CHECK)
  {
    OLED_ShowString(1, 1, "VM:    VR:    ");
    OLED_ShowString(2, 1, "VN:    VX:    ");
    OLED_ShowString(3, 1, "VB:     B:   ");
    OLED_ShowString(4, 1, "E:  F:     ");
  }
  else
  {
    /*
     * 闭环页面只显示适合低刷新率观察的慢变量。IR、VR分别表示输入电流和输入电压RMS，
     * 避免100 ms刷新周期恰好等于50 Hz的5个周期而把瞬时量固定在近似相同相位。
     */
    OLED_ShowString(1, 1, "IR:    VR:      ");
    OLED_ShowString(2, 1, "VB:    BR:      ");
    OLED_ShowString(3, 1, "M:     S:  B:   ");
    OLED_ShowString(4, 1, "F:     PF:    ");
  }
}

/**
 * @brief 依据运行模式填充10路VOFA JustFloat缓存。
 * @note  只由主循环每10 ms调用；函数不发送串口，也不直接读取DMA数组。
 */
static void PFC_DebugFillVofa(const PFC_Measurement *measurement,
                              const PFC_AdcCalibrationStats *stats,
                              const PFC_ControlTelemetry *control)
{
  if ((measurement == NULL) || (stats == NULL) || (control == NULL) ||
      (pfc_params == NULL))
  {
    return;
  }

  if (pfc_params->run_mode == PFC_RUN_MODE_RAW_ADC)
  {
    vofa_data[0] = (float)stats->ipfc_mean;
    vofa_data[1] = (float)stats->ipfc_min;
    vofa_data[2] = (float)stats->ipfc_max;
    vofa_data[3] = (float)stats->vac_mean;
    vofa_data[4] = (float)stats->vac_min;
    vofa_data[5] = (float)stats->vac_max;
    vofa_data[6] = (float)stats->vbus_mean;
    /* CH8=窗口序号+valid小数标志，CH9保留为0，便于确认1秒统计持续发布。 */
    vofa_data[8] = (float)stats->window_sequence +
                   ((stats->valid != 0U) ? 0.5f : 0.0f);
    vofa_data[9] = 0.0f;
  }
  else if (pfc_params->run_mode == PFC_RUN_MODE_ENGINEERING_CHECK)
  {
    vofa_data[0] = measurement->vac;
    vofa_data[1] = measurement->vac_rms;
    vofa_data[2] = measurement->ipfc;
    vofa_data[3] = measurement->ipfc_rms;
    vofa_data[4] = measurement->vbus;
    vofa_data[5] = measurement->vac_frequency_hz;
    /* CH6=参数错误码*1000+READY阻断位，可在VOFA中用整数除法分别解码。 */
    vofa_data[6] =
        (float)(PFC_Params_GetValidationError(pfc_params) * 1000U +
                PFC_AppGetReadyBlockReason(measurement));
    vofa_data[8] = measurement->input_active_power_w;
    vofa_data[9] = measurement->input_power_factor;
  }
  else
  {
    vofa_data[0] = measurement->vac;
    vofa_data[1] = measurement->ipfc;
    vofa_data[2] = control->current_reference;
    vofa_data[3] = measurement->vbus;
    vofa_data[4] = control->vbus_reference;
    vofa_data[5] = control->current_rms_command;
    vofa_data[6] = control->modulation;
    vofa_data[8] = measurement->input_active_power_w;
    vofa_data[9] = measurement->input_power_factor;
  }
  vofa_data[7] = (float)((uint32_t)PFC_AppGetState() * 1000000U +
                         measurement->fault_bits);
}

/**
 * @brief 按运行模式刷新OLED数字区域。
 * @note  主循环每100 ms调用；所有输入均为一致性快照，禁止从ADC/HRTIM ISR调用。
 */
static void PFC_DebugRefreshOled(const PFC_Measurement *measurement,
                                 const PFC_AdcCalibrationStats *stats,
                                 const PFC_ControlTelemetry *control)
{
  if ((measurement == NULL) || (stats == NULL) || (control == NULL) ||
      (pfc_params == NULL))
  {
    return;
  }

  if (pfc_params->run_mode == PFC_RUN_MODE_RAW_ADC)
  {
    OLED_ShowNum(1, 3, stats->ipfc_mean, 4);
    OLED_ShowNum(1, 8, stats->ipfc_min, 4);
    OLED_ShowNum(1, 13, stats->ipfc_max, 4);
    OLED_ShowNum(2, 3, stats->vac_mean, 4);
    OLED_ShowNum(2, 8, stats->vac_min, 4);
    OLED_ShowNum(2, 13, stats->vac_max, 4);
    OLED_ShowNum(3, 3, stats->vbus_mean, 4);
    OLED_ShowNum(3, 9, stats->window_sequence % 100000U, 5);
    OLED_ShowNum(4, 3, (uint32_t)PFC_AppGetState(), 2);
    OLED_ShowHexNum(4, 7, measurement->fault_bits, 5);
  }
  else if (pfc_params->run_mode == PFC_RUN_MODE_ENGINEERING_CHECK)
  {
    /* VM/VN/VX为VAC原始mean/min/max；VR为换算后的0.01 V RMS。 */
    OLED_ShowNum(1, 4, stats->vac_mean, 4);
    OLED_ShowNum(1, 12, (uint32_t)(measurement->vac_rms * 100.0f), 4);
    OLED_ShowNum(2, 4, stats->vac_min, 4);
    OLED_ShowNum(2, 12, stats->vac_max, 4);
    /* VB单位0.01 V；B是READY阻断位，便于直接定位VAC比例或母线门槛问题。 */
    OLED_ShowNum(3, 4, (uint32_t)(measurement->vbus * 100.0f), 5);
    OLED_ShowHexNum(3, 12, PFC_AppGetReadyBlockReason(measurement), 3);
    OLED_ShowHexNum(4, 3, PFC_Params_GetValidationError(pfc_params), 2);
    OLED_ShowHexNum(4, 8, measurement->fault_bits, 5);
  }
  else
  {
    /*
     * 第1行：IR单位为mA RMS，VR单位为0.01 V RMS。两者均为非负慢变量，
     * 4位显示分别覆盖0～9.999 A和0～99.99 V，兼容当前5 V档及预留36 V档。
     */
    OLED_ShowNum(1, 4, (uint32_t)(measurement->ipfc_rms * 1000.0f), 4);
    OLED_ShowNum(1, 11, (uint32_t)(measurement->vac_rms * 100.0f), 4);

    /* 第2行：VB为实测母线、BR为母线参考，单位均为0.01 V。 */
    OLED_ShowNum(2, 4, (uint32_t)(measurement->vbus * 100.0f), 4);
    OLED_ShowNum(2, 11, (uint32_t)(control->vbus_reference * 100.0f), 4);

    /*
     * 第3行：M为调制度乘10000，范围约-9000～+9000。
     * OLED_ShowSignedNum()会额外显示1个符号，因此Length=4总共占5列（第3～7列），
     * 不会覆盖第8列的状态标签S；S显示PFC应用状态机的十进制编号。
     * B显示三位十六进制READY阻断位。尤其在S=04时，B可直接指出VAC锁相、
     * VAC有效值、频率或VBUS门槛中的哪一项尚未满足，不再只能看到状态号猜原因。
     */
    OLED_ShowSignedNum(3, 3, (int32_t)(control->modulation * 10000.0f), 4);
    OLED_ShowNum(3, 10, (uint32_t)PFC_AppGetState(), 2);
    OLED_ShowHexNum(3, 14, PFC_AppGetReadyBlockReason(measurement), 3);

    /*
     * 第4行：F为5位故障码；PF为一个完整工频窗口的软件估算值乘100。
     * PF无效时显示0；该数只用于判断控制极性和趋势，赛题验收必须使用功率分析仪。
     */
    OLED_ShowHexNum(4, 3, measurement->fault_bits, 5);
    OLED_ShowSignedNum(4, 12,
                       (measurement->input_power_valid != 0U) ?
                       (int32_t)(measurement->input_power_factor * 100.0f) : 0,
                       3);
  }
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

  /* CubeMX初始化结束后第一时间保持四路输出关闭；当前板卡没有独立Gate Enable。 */
  PFC_HRTIM_StopPower();

  pfc_iwdg_reset_seen = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ? 1U : 0U;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  pfc_params = PFC_Params_GetActive();

	/*
	 * 上电后先等待OLED内部电源和电荷泵稳定。该延时只在初始化阶段执行，
	 * 不能放入主循环或快速采样回调，否则会阻塞后台任务或控制节拍。
	 */
	HAL_Delay(100);
	OLED_Init();

	/* 初始化测量模块的软件状态、DMA样本快照、序列号和故障标志。 */
	PFC_Measure_Init(pfc_params);
	PFC_AppInit(pfc_params, pfc_iwdg_reset_seen);

	/* 固定标签只写一次；运行模式由pfc_user_config.h在编译期选择。 */
	PFC_DebugInitOledLabels(pfc_params->run_mode);


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
   * 标定有效时按参数档重写IPFC、VBUS和VAC看门狗阈值；标定无效时函数保留
   * CubeMX宽窗口。AWD配置必须早于HAL_ADC_Start_DMA()，避免转换期间改模式。
   */
  if (PFC_Measure_ConfigureWatchdogs(&hadc1, &hadc2) != HAL_OK)
  {
    PFC_Measure_Trip(PFC_FAULT_PARAM | PFC_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * ADC2规则组只有VAC一个Rank。先启动ADC2 DMA，使其提前进入等待
   * HRTIM_TRG1的状态。DMA硬件更新VAC原始数组，完整传输回调只推进
   * adc2_sequence，供随后完成的ADC1回调判断两个ADC是否属于同一采样节拍。
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

  /*
   * 启动Master、Timer A/B计数器以产生10 kHz ADC Trigger。
   * 此函数只建立采样时基，不开放TA1/TA2/TB1/TB2输出。
   */
  if (PFC_HRTIM_StartSampling() != HAL_OK)
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
  
  /* ms Tick；主循环独占写入，用于把OLED刷新限制为100 ms一次。 */
  uint32_t oled_tick = HAL_GetTick();

  /* ms Tick；主循环独占写入，VOFA约每秒发送100帧且不追补过期帧。 */
  uint32_t vofa_tick = HAL_GetTick();

  /* ms Tick；安全监督与OLED独立计时，显示任务不能改变喂狗判据。 */
  uint32_t safety_tick = HAL_GetTick();
  /* 上一监督窗口的ADC序列快照；每100 ms更新，用于检测任一路DMA停滞。 */
  uint32_t safety_last_adc1_sequence = 0U;
  uint32_t safety_last_adc2_sequence = 0U;
  /* ms Tick；通过逐毫秒追赶调用状态机，短暂主循环延迟不会跳过消抖计时。 */
  uint32_t app_tick = HAL_GetTick();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    while ((HAL_GetTick() - app_tick) >= 1U)
    {
      app_tick++;
      PFC_AppTick1ms();
    }

	/*
	 * 每10 ms发送一次VOFA数据。阻塞式USART只允许放在主循环，
	 * 不能放进10 kHz ADC DMA回调，否则会破坏快速采样和后续控制节拍。
	 */
	if ((HAL_GetTick() - vofa_tick) >= 10U)
	{
		PFC_Measurement vofa_measurement;
		PFC_AdcCalibrationStats calibration_stats;
		PFC_ControlTelemetry control_telemetry;

		/* 以当前时刻为新基准，主循环偶尔延迟时不连续补发旧帧。 */
		vofa_tick = HAL_GetTick();

		/* 读取ISR已经发布的完整快照，不直接读取正在由DMA改写的数组。 */
		PFC_Measure_GetSnapshot(&vofa_measurement);
		PFC_Measure_GetCalibrationStats(&calibration_stats);
		PFC_Control_GetTelemetry(&control_telemetry);
		PFC_DebugFillVofa(&vofa_measurement, &calibration_stats,
		                  &control_telemetry);

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
        PFC_AdcCalibrationStats calibration_stats;
        PFC_ControlTelemetry control_telemetry;

        /*
         * 主循环不直接读取正在被DMA改写的PFC_Adc1Dma/PFC_Adc2Dma。
         * 通过测量模块取得同一发布时刻的完整快照，避免显示半更新数据。
        */
        PFC_Measure_GetSnapshot(&measurement);
        PFC_Measure_GetCalibrationStats(&calibration_stats);
        PFC_Control_GetTelemetry(&control_telemetry);
        PFC_DebugRefreshOled(&measurement, &calibration_stats,
                             &control_telemetry);
    }

    if ((HAL_GetTick() - safety_tick) >= 100U)
    {
      PFC_Measurement safety_measurement;

      safety_tick = HAL_GetTick();
      PFC_Measure_GetSnapshot(&safety_measurement);
      if ((safety_measurement.adc1_sequence == safety_last_adc1_sequence) ||
          (safety_measurement.adc2_sequence == safety_last_adc2_sequence))
      {
        /* 任一路DMA停滞都先锁存同步故障，再停止刷新IWDG。 */
        PFC_Measure_Trip(PFC_FAULT_ADC_SYNC);
      }
      else if (PFC_AppWatchdogHealthy() != 0U)
      {
        (void)HAL_IWDG_Refresh(&hiwdg);
      }
      safety_last_adc1_sequence = safety_measurement.adc1_sequence;
      safety_last_adc2_sequence = safety_measurement.adc2_sequence;
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

    /* ADC1完整序列是唯一10 kHz控制入口；锁存故障后不得继续推进PI/PR。 */
    if (PFC_Measure_GetFault() == PFC_FAULT_NONE)
    {
      PFC_AppFastStep();
    }
  }
}

/**
 * @brief  将任一ADC的HAL错误统一转换为锁存故障。
 * @param  hadc 发生错误的ADC句柄；当前策略不按ADC实例区分恢复方式。
 * @note   运行在ADC/DMA错误中断上下文，只允许执行非阻塞安全关断和故障记录。
 */
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
 * @brief ADC1/2模拟看门狗1越窗回调。
 * @param hadc 发生AWD1事件的ADC句柄；ADC1对应IPFC，ADC2对应VAC。
 * @note 运行在ADC1_2中断中，只执行故障锁存和HRTIM快速关断。
 */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && (hadc->Instance == ADC1))
  {
    PFC_Measure_AnalogWatchdog(PFC_AWD_SOURCE_IPFC);
  }
  else if ((hadc != NULL) && (hadc->Instance == ADC2))
  {
    PFC_Measure_AnalogWatchdog(PFC_AWD_SOURCE_VAC);
  }
}

/**
 * @brief ADC1模拟看门狗2越窗回调，专门记录VBUS过压。
 * @param hadc 预期为ADC1句柄。
 * @note 运行在ADC1_2中断中；AWD2是硬件阈值检测，回调负责软件锁存和再次关断。
 */
void HAL_ADCEx_LevelOutOfWindow2Callback(ADC_HandleTypeDef *hadc)
{
  if ((hadc != NULL) && (hadc->Instance == ADC1))
  {
    PFC_Measure_AnalogWatchdog(PFC_AWD_SOURCE_VBUS);
  }
}

/**
 * @brief  记录HRTIM Fault 3事件并锁存PFC故障。
 * @param  hhrtim 产生Fault 3中断的HRTIM句柄；当前工程只有HRTIM1。
 * @note   HRTIM硬件输出无效逻辑是第一关断路径，本ISR不是短路保护的首响应路径。
 */
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
   * 当前驱动板没有MCU Gate Enable，因此HRTIM输出禁止就是软件关断边界。
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
