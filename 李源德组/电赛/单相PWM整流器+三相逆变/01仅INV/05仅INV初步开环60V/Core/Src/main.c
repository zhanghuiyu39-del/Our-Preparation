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
#include "CBSVPWM.h"
#include "inv_hrtim.h"
#include "inv_measure.h"
#include "inv_open_loop.h"
#include "inv_user_config.h"
#include "iwdg.h"
#include "vofa.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/**
 * @brief 逆变开环验证状态。
 * @note 状态由初始化主流程和10 kHz控制回调更新，Keil Watch只读观察。
 */
typedef enum
{
  INV_STATE_SAFE = 0U,
  INV_STATE_ADC_CALIBRATION,
  INV_STATE_ADC_OFFSET,
  INV_STATE_READY,
  INV_STATE_SOFT_START,
  INV_STATE_RUNNING,
  INV_STATE_FAULT_LATCH
} INV_AppState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * 所有日常模式、开环命令和ADC标定参数均集中在inv_user_config.h。
 * main.c只消费已经命名的用户参数，不再保留第二套可调常量。
 */

#if ((INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION) && \
     (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM) && \
     (INV_USER_WORK_MODE != INV_MODE_OPEN_LOOP))
#error "INV_USER_WORK_MODE must select one INV_MODE_* value"
#endif

#if ((INV_USER_OUTPUT_FREQUENCY != 30U) && (INV_USER_OUTPUT_FREQUENCY != 60U))
#error "INV_USER_OUTPUT_FREQUENCY must be 30 or 60 Hz"
#endif

#if ((INV_USER_CALIBRATION_STEP != INV_CAL_STEP_ZERO) && \
     (INV_USER_CALIBRATION_STEP != INV_CAL_STEP_GAIN))
#error "INV_USER_CALIBRATION_STEP must be ZERO or GAIN"
#endif

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

/*
 * VOFA发送固定为8通道。开环/波形模式发送U相参考、六路ADC快照和故障；
 * 标定模式发送六路原始码、推荐比例和故障，具体映射见主循环分支。
 */
static float vofa_data[VOFA_MAX_CHANNELS] = {0.0f};

/* 供Keil Watch观察串口HAL调用结果；通信失败不触发功率故障。 */
static uint32_t vofa_tx_ok_count = 0U;
static uint32_t vofa_tx_error_count = 0U;

/*
 * 由inv_user_config.h构造的开环参数对象。后续改为赛题32 Vrms时，应同时
 * 引入实测VBUS并重新校核调制度，不能只修改目标线电压一个字段。
 */
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

/*
 * ADC配置由唯一用户头文件构造。数组固定顺序为
 * IU、IV、IW、VUV、VVW、VWU，与INV_CalibrationResult完全一致。
 */
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
  INV_USER_ADC_RAIL_CONFIRM_FRAMES,
  INV_USER_LINE_VOLTAGE_ONLY
};

/* 以下非static调试量可直接加入Keil Watch；ISR写入，调试器只读观察。 */
/* CBSVPWM运行对象保留归一化、零序注入、限幅和最终占空比中间量。 */
CBSVPWM_t inv_svpwm;

/*
 * U相参考、当前频率、控制心跳和最坏周期数由10 kHz DMA控制路径更新。
 * volatile只阻止编译器省略访问，不提供跨ISR结构体一致性；后台任务仍读取模块快照。
 */
volatile float inv_sine_u = 0.0f;
volatile uint16_t inv_output_frequency_hz = 60U;
volatile uint32_t inv_control_heartbeat = 0U;
uint32_t inv_control_max_cycles = 0U;
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static uint32_t inv_limited_frames = 0U;
#endif

/* 当前编译模式可直接加入Keil Watch，值0/1/2分别对应配置头文件三种模式。 */
const uint8_t inv_work_mode = INV_USER_WORK_MODE;

/* 应用状态可直接加入Keil Watch；只有SOFT_START/RUNNING允许更新PWM比较值。 */
volatile INV_AppState inv_state = INV_STATE_SAFE;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void INV_AppTrip(uint32_t fault_bits);
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static void INV_AppRunFastControl(const INV_Measurement *measurement);
#endif
#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
static HAL_StatusTypeDef INV_TestWaitForSamplingReady(void);
#endif
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static HAL_StatusTypeDef INV_OpenLoopStartOutputs(void);
#endif
static void INV_TestDwtInit(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief 统一锁存故障并立即撤销六路PWM输出。
 * @param fault_bits INV_FaultMask位组合，0输入不会清除已有故障。
 * @note 允许从ISR调用；只执行寄存器/HAL非阻塞关断和软件状态复位，
 *       不发送USART、不刷新OLED，也不允许自动重新开放PWM。
 */
static void INV_AppTrip(uint32_t fault_bits)
{
  INV_HRTIM_DisableOutputs();
  INV_Measure_LatchFault(fault_bits);
  INV_OpenLoop_Reset();
  CBSVPWM_Reset(&inv_svpwm);
  inv_state = INV_STATE_FAULT_LATCH;
}

/**
 * @brief 等待ADC3/4/5完成当前模式要求的零点或单通道增益采集。
 * @retval HAL_OK表示同步采样和当前标定步骤完成；超时或故障返回HAL_ERROR。
 *
 * 此时Master和Timer C/D/E只产生HRTIM Trigger 2，六路PWM输出保持关闭。
 * 等待上限由INV_USER_STARTUP_TIMEOUT_MS配置；10 kHz下256帧约25.6 ms。
 */
#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
static HAL_StatusTypeDef INV_TestWaitForSamplingReady(void)
{
  /* 两个快照均由ISR发布，本函数只在PWM关闭的启动阶段轮询。 */
  uint32_t start_tick = HAL_GetTick();
  INV_Measurement measurement;
  INV_CalibrationResult calibration;

  while ((HAL_GetTick() - start_tick) < INV_USER_STARTUP_TIMEOUT_MS)
  {
    INV_Measure_GetSnapshot(&measurement);
    INV_Measure_GetCalibrationResult(&calibration);

    if (measurement.fault_bits != INV_FAULT_NONE)
    {
      return HAL_ERROR;
    }

#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
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

  INV_AppTrip(INV_FAULT_ADC_SYNC);
  return HAL_TIMEOUT;
}
#endif

/**
 * @brief 检查PB10/FLT3，并按编译开关决定是否开放C/D/E六路PWM。
 * @retval HAL_OK 启动条件满足；HAL_ERROR表示Fault有效或HRTIM启动失败。
 * @note 当前板没有PE Gate Enable或PE6 nFAULT输入。PB10若未连接真实故障源，
 *       只能用于人工拉低验证，不能等同于驱动器DESAT/OCP保护。
 */
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static HAL_StatusTypeDef INV_OpenLoopStartOutputs(void)
{
  if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
      (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET))
  {
    return HAL_ERROR;
  }

  /* 三相先写入50%中性占空比，再统一开放输出，避免启动瞬间出现旧比较值。 */
  if (INV_HRTIM_SetDuty(0.5f, 0.5f, 0.5f) != HAL_OK)
  {
    return HAL_ERROR;
  }

#if (INV_USER_ENABLE_PWM_OUTPUT != 0U)
  return INV_HRTIM_EnableOutputs();
#else
  /* 示波器安全模式仍运行ADC、DDS和比较值更新，但物理PWM引脚保持关闭。 */
  return HAL_OK;
#endif
}
#endif

/** @brief 开启DWT周期计数器，用于测量10 kHz控制路径最坏执行时间。 */
static void INV_TestDwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 在一个新六通道同步帧到达后执行一次DDS、CBSVPWM和HRTIM更新。
 * @param measurement 本次ADC同步帧，只用于有效性检查，不直接读取DMA数组。
 * @note  本函数运行于DMA中断上下文，禁止阻塞HAL、OLED、USART和HAL_Delay()。
 */
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
static void INV_AppRunFastControl(const INV_Measurement *measurement)
{
  INV_OpenLoopOutput reference;
  uint32_t start_cycles;
  uint32_t elapsed_cycles;

#if (INV_USER_WORK_MODE == INV_MODE_PWM_WAVEFORM)
  /* 波形模式不使用采样值，ADC3 DMA只提供10 kHz确定节拍。 */
  (void)measurement;
#else
  if ((measurement == NULL) ||
      (measurement->valid == 0U) ||
      (measurement->fault_bits != INV_FAULT_NONE))
  {
    return;
  }
#endif

  if ((inv_state != INV_STATE_SOFT_START) &&
      (inv_state != INV_STATE_RUNNING))
  {
    return;
  }

  start_cycles = DWT->CYCCNT;
  if (!INV_OpenLoop_Step(&reference))
  {
    INV_AppTrip(INV_FAULT_PARAMETER);
    return;
  }

  inv_sine_u = reference.sine_u;
  inv_output_frequency_hz = reference.frequency_hz;

  if (CBSVPWM_Calc3Leg(&inv_svpwm,
                       reference.vu_command,
                       reference.vv_command,
                       reference.vw_command,
                       reference.dc_bus_v) == 0U)
  {
    INV_AppTrip(INV_FAULT_CBSVPWM);
    return;
  }

  if (inv_svpwm.limited != 0U) {
    inv_limited_frames++;
    if (inv_limited_frames >= INV_USER_LIMITED_MAX_FRAMES) {
      INV_AppTrip(INV_FAULT_CBSVPWM);
      return;
    }
  } else {
    inv_limited_frames = 0U;
  }

  /* Fault 0级中断可能抢占本控制ISR；恢复后必须复查，禁止继续写比较值。 */
  if ((inv_state == INV_STATE_FAULT_LATCH) ||
      (INV_Measure_GetFault() != INV_FAULT_NONE))
  {
    return;
  }

  if (INV_HRTIM_SetDuty(inv_svpwm.duty_u,
                        inv_svpwm.duty_v,
                        inv_svpwm.duty_w) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_PWM_COMMAND);
    return;
  }

  inv_control_heartbeat++;
  if (reference.ramp >= 1.0f)
  {
    inv_state = INV_STATE_RUNNING;
  }
  elapsed_cycles = DWT->CYCCNT - start_cycles;
  if (elapsed_cycles > inv_control_max_cycles) {
    inv_control_max_cycles = elapsed_cycles;
  }
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

  /* 任何模块初始化前先撤销C/D/E输出；当前IOC中PE0～PE6均不参与控制。 */
  INV_HRTIM_DisableOutputs();
  inv_state = INV_STATE_SAFE;

  /*
   * 上电后只在初始化阶段等待一次，使OLED内部电源和电荷泵稳定。
   * 禁止把HAL_Delay()放入主循环或ADC DMA回调，否则会破坏后台调度
   * 或10 kHz开环控制节拍。等待期间HRTIM六路PWM保持关闭。
   */
  HAL_Delay(100U);
  OLED_Init();

  /*
   * 固定标签只写一次，主循环后续只更新数字区域，避免整屏清除造成闪烁：
    * 当前只显示VUV/VVW/VWU三路线电压ADC原始码；电流Rank由IOC完成转换但软件不使用；
   * 第4行在开环/波形模式显示频率和故障，在标定模式显示通道、完成状态和故障。
   */
  OLED_ShowString(1U, 1U, "VUV:");
  OLED_ShowString(2U, 1U, "VVW:");
  OLED_ShowString(3U, 1U, "VWU:");
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
  OLED_ShowString(4U, 1U, "C:");
  OLED_ShowString(4U, 5U, "D:");
  OLED_ShowString(4U, 9U, "E:");
#else
  OLED_ShowString(4U, 1U, "F:");
  OLED_ShowString(4U, 5U, "Hz");
  OLED_ShowString(4U, 9U, "E:");
#endif

  /* 从唯一用户配置构造测量模式，并清空DMA序列、标定状态和历史故障。 */
  if (!INV_Measure_Init(&inv_measure_config))
  {
    Error_Handler();
  }
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  inv_limited_frames = 0U;
#endif
  INV_TestDwtInit();

#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  /*
   * 60 V母线和32 Vrms线电压由开环模块换算成约0.871调制度。
   * 默认60 Hz；后续需要题目第5项时可调用SetFrequency(INV_FREQ_30HZ)，
   * DDS只改变相位步进，不清零当前相位，因此切换时不会产生相位跳变。
   */
  if (!INV_OpenLoop_Init(&inv_open_loop_config))
  {
    INV_AppTrip(INV_FAULT_PARAMETER);
    Error_Handler();
  }

  /* CBSVPWM使用60 V开环母线参数；本阶段仍未配置独立VBUS ADC反馈。 */
  if (CBSVPWM_Init(&inv_svpwm,
                   inv_open_loop_config.modulation_limit,
                   INV_USER_MINIMUM_DC_V) == 0U)
  {
    INV_AppTrip(INV_FAULT_CBSVPWM);
    Error_Handler();
  }
#endif

  /* ADC校准必须在规则组DMA和HRTIM Trigger 2启动之前完成。 */
  inv_state = INV_STATE_ADC_CALIBRATION;
  if (HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * 三个ADC均包含两个规则Rank，DMA长度必须固定为2。先启动ADC5、ADC4，
   * 最后启动ADC3，使三路ADC在Master尚未运行时全部进入等待Trigger 2状态。
   */
  if (HAL_ADC_Start_DMA(&hadc5, (uint32_t *)INV_Adc5Dma, 2U) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)INV_Adc4Dma, 2U) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)INV_Adc3Dma, 2U) != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * 循环DMA默认同时产生Half Transfer和Transfer Complete中断。本测试只在
   * 两个Rank全部搬运完成后处理样本，因此关闭HT，防止读取半更新缓冲区。
   */
  __HAL_DMA_DISABLE_IT(hadc5.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc4.DMA_Handle, DMA_IT_HT);
  __HAL_DMA_DISABLE_IT(hadc3.DMA_Handle, DMA_IT_HT);

  /*
   * 启动Master与Timer C/D/E计数器，Master CMP2通过HRTIM Trigger 2
   * 以10 kHz触发ADC3/4/5；该函数不会开放六路PWM。
   */
  inv_state = INV_STATE_ADC_OFFSET;
  if (INV_HRTIM_StartTimeBase() != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_HRTIM);
    Error_Handler();
  }

#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
  /*
   * OPEN_LOOP等待自动零点；ADC_CALIBRATION等待ZERO或GAIN样本完成。
   * PWM_WAVEFORM不使用采样值，因此跳过等待并由ADC3 DMA直接提供节拍。
   */
  if (INV_TestWaitForSamplingReady() != HAL_OK)
  {
    Error_Handler();
  }
#endif

  /* 进入READY时物理PWM仍未开放；标定模式会永久停留在安全输出关闭状态。 */
  inv_state = INV_STATE_READY;

#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
      (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET))
  {
    INV_AppTrip(INV_FAULT_HRTIM);
    Error_Handler();
  }
#endif

  /*
   * IWDG在所有阻塞式初始化完成后、PWM开放前启动。此后任一启动失败均不再
   * 刷新看门狗，约500 ms后复位，且复位期间HRTIM输出保持默认关闭。
   */
  MX_IWDG_Init();

#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  if (INV_OpenLoopStartOutputs() != HAL_OK)
  {
    INV_AppTrip(INV_FAULT_HRTIM);
    Error_Handler();
  }

  /* 下一完整ADC同步帧开始推进DDS和1 s软启动，不在主循环伪造控制节拍。 */
  inv_state = INV_STATE_SOFT_START;
#else
  /* 标定模式不调用EnableOutputs，用户误把输出开关设为1也不会开放PWM。 */
  INV_HRTIM_DisableOutputs();
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  /* VOFA以10 ms为发送周期，即每秒发送约100帧。 */
  uint32_t vofa_tick = HAL_GetTick();

  /* OLED属于100 ms低速后台任务，不参与ADC、HRTIM或故障中断。 */
  uint32_t oled_tick = HAL_GetTick();

  /* 保存上次监督快照，用于判断三路DMA和10 kHz控制是否持续推进。 */
  /*
   * 三个tick分别是VOFA、OLED和安全监督任务的毫秒时间基准；各任务独立计时，
   * 某个低速任务偶尔延迟不会改变另外两个任务的周期判断。
   */
  uint32_t supervisor_tick = HAL_GetTick();

  /*
   * 以下值保存上一次100 ms监督时看到的序列号/心跳。当前值必须与它们不同，
   * 才能证明三路DMA、六通道拼帧以及CBSVPWM控制入口均在持续运行。
   */
#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
  uint32_t last_adc3_sequence = 0U;
  uint32_t last_adc4_sequence = 0U;
  uint32_t last_adc5_sequence = 0U;
  uint32_t last_fast_heartbeat = 0U;
#endif
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
  uint32_t last_control_heartbeat = 0U;
#endif
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /*
     * 每10 ms发送一次VOFA数据。阻塞式USART只允许位于主循环，禁止放入
     * 10 kHz DMA回调，否则约0.78 ms的串口发送会破坏实时控制节拍。
     */
    if ((HAL_GetTick() - vofa_tick) >= INV_USER_VOFA_PERIOD_MS)
    {
      /* 局部副本在短暂关中断期间一次性取得，后续串口发送不再访问ISR共享对象。 */
      INV_Measurement measurement;
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      INV_CalibrationResult calibration;
#else
      INV_OpenLoopOutput reference;
#endif

      vofa_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      INV_Measure_GetCalibrationResult(&calibration);

      /* 标定模式CH0~5为六路原始码，CH6为推荐比例，CH7为锁存故障位。 */
      vofa_data[0] = (float)measurement.iu_raw;
      vofa_data[1] = (float)measurement.iv_raw;
      vofa_data[2] = (float)measurement.iw_raw;
      vofa_data[3] = (float)measurement.vuv_raw;
      vofa_data[4] = (float)measurement.vvw_raw;
      vofa_data[5] = (float)measurement.vwu_raw;
      vofa_data[6] = calibration.recommended_scale;
      vofa_data[7] = (float)measurement.fault_bits;
#else
      INV_OpenLoop_GetSnapshot(&reference);

      /* CH0显示当前30/60 Hz单位正弦，用来同时验证DDS节拍和USART数据顺序。 */
      vofa_data[0] = reference.sine_u;

      /* 当前只发送三路线电压；电流Rank仍在DMA中搬运，但不进入控制和故障判断。 */
      vofa_data[1] = (float)measurement.vuv_raw;
      vofa_data[2] = (float)measurement.vvw_raw;
      vofa_data[3] = (float)measurement.vwu_raw;
      vofa_data[4] = 0.0f;
      vofa_data[5] = 0.0f;
      vofa_data[6] = 0.0f;
      vofa_data[7] = (float)measurement.fault_bits;
#endif

      if (VOFA_Send(&huart2, vofa_data, VOFA_MAX_CHANNELS) == HAL_OK)
      {
        vofa_tx_ok_count++;
      }
      else
      {
        /* 串口仅用于观测，发送失败计数但不直接触发功率故障。 */
        vofa_tx_error_count++;
      }
    }

    /*
     * 每100 ms进行一次安全监督。任一DMA、同步快照或控制入口停止推进，
     * 均立即关闭逆变输出并停止喂狗；故障锁存后禁止自动重新启动PWM。
     */
    if ((HAL_GetTick() - supervisor_tick) >= INV_USER_SUPERVISOR_PERIOD_MS)
    {
      /* 本轮监督使用同一份完整快照比较所有ADC序列、有效标志和故障位。 */
      INV_Measurement measurement;

      supervisor_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);

#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      if ((measurement.adc3_sequence == last_adc3_sequence) ||
          (measurement.adc4_sequence == last_adc4_sequence) ||
          (measurement.adc5_sequence == last_adc5_sequence) ||
          (measurement.fast_heartbeat == last_fast_heartbeat))
      {
        INV_AppTrip(INV_FAULT_ADC_SYNC);
      }
      else if (measurement.fault_bits != INV_FAULT_NONE)
      {
        INV_AppTrip(measurement.fault_bits);
      }
      else if (inv_state != INV_STATE_READY)
      {
        INV_AppTrip(INV_FAULT_SYSTEM);
      }
      else if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
      {
        INV_AppTrip(INV_FAULT_SYSTEM);
      }
#elif (INV_USER_WORK_MODE == INV_MODE_PWM_WAVEFORM)
      if (inv_control_heartbeat == last_control_heartbeat)
      {
        INV_AppTrip(INV_FAULT_CONTROL_TIMEOUT);
      }
      else if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
               (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET) ||
               ((inv_state != INV_STATE_SOFT_START) &&
                (inv_state != INV_STATE_RUNNING)))
      {
        INV_AppTrip(INV_FAULT_HRTIM);
      }
      else if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
      {
        INV_AppTrip(INV_FAULT_SYSTEM);
      }
#else
      if ((measurement.adc3_sequence == last_adc3_sequence) ||
          (measurement.adc4_sequence == last_adc4_sequence) ||
          (measurement.adc5_sequence == last_adc5_sequence))
      {
        INV_AppTrip(INV_FAULT_ADC_SYNC);
      }
      else if ((measurement.fast_heartbeat == last_fast_heartbeat) ||
               (inv_control_heartbeat == last_control_heartbeat))
      {
        INV_AppTrip(INV_FAULT_CONTROL_TIMEOUT);
      }
      else if (measurement.fault_bits != INV_FAULT_NONE)
      {
        INV_AppTrip(measurement.fault_bits);
      }
      else if ((measurement.valid == 0U) ||
               (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
               (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET) ||
               ((inv_state != INV_STATE_SOFT_START) &&
                (inv_state != INV_STATE_RUNNING)))
      {
        INV_AppTrip(INV_FAULT_HRTIM);
      }
      else
      {
        /* 仅在完整安全条件成立时刷新IWDG，禁止在中断中无条件喂狗。 */
        if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
        {
          INV_AppTrip(INV_FAULT_SYSTEM);
        }
      }
#endif

#if (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM)
      last_adc3_sequence = measurement.adc3_sequence;
      last_adc4_sequence = measurement.adc4_sequence;
      last_adc5_sequence = measurement.adc5_sequence;
      last_fast_heartbeat = measurement.fast_heartbeat;
#endif
#if (INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION)
      last_control_heartbeat = inv_control_heartbeat;
#endif
    }

    /*
     * 每100 ms更新一次OLED。安全监督放在本段之前执行，确保软件I2C传输
     * 不会推迟DMA失步、FLT3或控制心跳异常的检测与安全关断。
     */
    if ((HAL_GetTick() - oled_tick) >= INV_USER_OLED_PERIOD_MS)
    {
      /* OLED只显示此处取得的完整快照，不直接读取DMA正在更新的原始数组。 */
      INV_Measurement measurement;
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      INV_CalibrationResult calibration;
#else
      INV_OpenLoopOutput reference;
#endif

      /* 主循环偶尔延迟时不连续补刷旧帧，而是以当前时刻作为下一周期基准。 */
      oled_tick = HAL_GetTick();

      /*
       * 读取测量模块已经发布的完整六通道快照，禁止直接访问正被DMA改写的
       * INV_Adc3Dma/INV_Adc4Dma/INV_Adc5Dma数组，避免显示半更新数据。
       */
      INV_Measure_GetSnapshot(&measurement);
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      INV_Measure_GetCalibrationResult(&calibration);
#else
      INV_OpenLoop_GetSnapshot(&reference);
#endif

      /*
       * 六路ADC为12位原始码，固定显示4位十进制数；即使前导位为0，
       * 数字区域宽度也保持不变，不需要周期性OLED_Clear()。
       */
      OLED_ShowNum(1U, 5U, measurement.vuv_raw, 4U);
      OLED_ShowNum(2U, 5U, measurement.vvw_raw, 4U);
      OLED_ShowNum(3U, 5U, measurement.vwu_raw, 4U);

      /*
       * 输出频率固定显示两位十进制数；故障位用三位十六进制显示，
       * 可覆盖新增的CSS时钟故障位，正常状态为000且故障后不自动清零。
       */
#if (INV_USER_WORK_MODE == INV_MODE_ADC_CALIBRATION)
      /* D十位=complete、个位=valid；比例的完整浮点值通过Watch/VOFA CH6读取。 */
      OLED_ShowNum(4U, 3U, calibration.channel, 1U);
      OLED_ShowNum(4U, 7U,
                   (uint32_t)calibration.complete * 10U + calibration.valid,
                   2U);
      OLED_ShowHexNum(4U, 11U, measurement.fault_bits, 3U);
#else
      OLED_ShowNum(4U, 3U, reference.frequency_hz, 2U);
      OLED_ShowHexNum(4U, 11U, measurement.fault_bits, 3U);
#endif
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
 * ADC3/4/5均由HRTIM Master CMP2通过HRTIM_TRG2以10 kHz触发。每个ADC
 * 分支先发布自己的DMA序列；只有三个序列全部更新后，测量模块才返回true
 * 和一份完整快照，因此一个PWM周期只会运行一次开环控制。
 *
 * 该函数运行在DMA中断上下文，禁止调用阻塞式USART或HAL_Delay()。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
#if (INV_USER_WORK_MODE == INV_MODE_PWM_WAVEFORM)
  INV_Measurement measurement;
  bool frame_ready = false;

  /* 波形模式发布线电压快照，但不因ADC数值、valid或fault_bits关闭PWM。 */
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

  if (frame_ready)
  {
    /* 完整DMA帧仍提供唯一10 kHz控制节拍；输入值不参与开环调制。 */
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

  /*
   * 测量模块内部发现失步或贴轨时只负责锁存记录；应用层必须在同一ISR
   * 立即撤销HRTIM输出，不能等待100 ms后台监督才执行关断。
   */
  if (INV_Measure_GetFault() != INV_FAULT_NONE)
  {
    INV_AppTrip(INV_Measure_GetFault());
    return;
  }

#if (INV_USER_WORK_MODE == INV_MODE_OPEN_LOOP)
  if (frame_ready &&
      (measurement.offset_ready != 0U) &&
      (measurement.valid != 0U))
  {
    INV_AppRunFastControl(&measurement);
  }
#else
  /* 标定模式只发布快照和标定结果，绝不进入DDS/CBSVPWM/HRTIM更新。 */
  (void)frame_ready;
#endif
#endif
}

/**
 * @brief ADC及其DMA错误回调，仅处理本测试启用的ADC3/4/5。
 * @param hadc 报告错误的ADC句柄；NULL或ADC1/2不会进入逆变故障处理。
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
#if (INV_USER_WORK_MODE == INV_MODE_PWM_WAVEFORM)
  /* ADC3错误最终表现为控制心跳停止，由100 ms监督关断；ADC4/5不参与该模式。 */
  (void)hadc;
#else
  /*
   * ADC3/4/5发生Overrun、DMA或内部状态错误时统一锁存逆变ADC故障。
   * ADC1/2在本测试中没有启动，若以后启用应由各自应用模块单独处理。
   */
  if ((hadc != NULL) &&
      ((hadc->Instance == ADC3) ||
       (hadc->Instance == ADC4) ||
       (hadc->Instance == ADC5)))
  {
    INV_AppTrip(INV_FAULT_ADC_ERROR);
  }
#endif
}

/**
 * @brief ADC模拟看门狗越界回调，仅处理当前逆变使用的ADC3/4/5。
 * @param hadc 触发AWD1或AWD2的ADC句柄。
 * @note 当前IOC阈值仍为0～4095，正常不会触发。完成模拟前端标定并收紧阈值后，
 *       本回调会立即关闭PWM并锁存故障；禁止在此处执行串口或OLED操作。
 */
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
#if (INV_USER_WORK_MODE == INV_MODE_OPEN_LOOP)
  if ((hadc != NULL) &&
      ((hadc->Instance == ADC3) ||
       (hadc->Instance == ADC4) ||
       (hadc->Instance == ADC5)))
  {
    INV_AppTrip(INV_FAULT_ADC_WATCHDOG);
  }
#else
  /* 标定可故意接近量程边缘；波形模式不使用ADC值，因此两种模式均忽略AWD。 */
  (void)hadc;
#endif
}

/**
 * @brief HRTIM Fault 3中断回调，在硬件关断完成后锁存软件故障记录。
 * @param hhrtim 触发Fault回调的HRTIM句柄；本工程仅配置HRTIM1。
 */
void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
  /*
   * Fault 3硬件逻辑已先把HRTIM输出置为Inactive；ISR随后记录故障状态，
   * 供主循环显示和状态机处理。清除中断标志不代表允许自动恢复PWM。
   */
  if ((hhrtim != NULL) && (hhrtim->Instance == HRTIM1))
  {
    INV_AppTrip(INV_FAULT_HRTIM);
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
  /*
   * 不可恢复HAL错误先关闭六路PWM并停止C/D/E计数器。若IWDG已经启动，
   * 停止刷新后由其复位；若尚未启动，则保持输出关闭并等待人工复位。
   */
  INV_HRTIM_DisableOutputs();
  INV_Measure_LatchFault(INV_FAULT_SYSTEM);
  INV_OpenLoop_Reset();
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
