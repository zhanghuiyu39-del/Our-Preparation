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
#include "iwdg.h"
#include "vofa.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * 本文件进行5 V直流母线下的三相三线开环逆变测试。默认输出60 Hz三相基波，
 * 目标线电压为2.5 Vrms；实际桥臂引脚仍是10 kHz
 * 中心对齐PWM，只有经过负载/滤波后的基波分量接近正弦。
 *
 * 调制度计算：M = 2 * Vphase_peak / Vdc，Vphase_peak=Vline_rms*sqrt(2/3)。
 * 当前Vdc=5 V、Vline_rms=2.5 V，因此目标M约为0.816，低于0.90限幅。
 * 软件在1 s内把调制度从0平滑增加到目标值，避免Gate Enable开启后突加电压。
 */
#define INV_OPEN_LOOP_DC_BUS_V             (5.0f)  /* 实际接到逆变桥直流侧的母线电压。 */
#define INV_OPEN_LOOP_LINE_RMS_V            (2.5f)  /* 目标U-V/V-W/W-U线电压有效值。 */
#define INV_OPEN_LOOP_DEFAULT_FREQUENCY     (INV_FREQ_60HZ) /* 改为INV_FREQ_30HZ即可测试题目第5项。 */
#define INV_OPEN_LOOP_ENABLE_POWER_STAGE    (1U)    /* 1=拉高PE1带低压功率级；0=仅测MCU PWM。 */
#define INV_TEST_MODULATION_LIMIT      (0.90f)
#define INV_TEST_MINIMUM_DC_V          (1.0f)
#define INV_TEST_OFFSET_TIMEOUT_MS     (200U)
#define INV_TEST_SUPERVISOR_PERIOD_MS  (100U)
#define INV_TEST_VOFA_PERIOD_MS        (10U)
#define INV_TEST_OLED_PERIOD_MS        (100U)
#define INV_TEST_LIMITED_MAX_FRAMES    (100U) /* 连续限幅10 ms才锁存，允许瞬时舍入。 */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/*
 * VOFA发送数组与VOFA+通道顺序一一对应，固定为8通道：
 * U相单位正弦、六路逆变ADC原始值和故障位。
 */
static float vofa_data[VOFA_MAX_CHANNELS] = {0.0f};

/* 供Keil Watch观察串口HAL调用结果；通信失败不触发功率故障。 */
static uint32_t vofa_tx_ok_count = 0U;
static uint32_t vofa_tx_error_count = 0U;

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
static uint32_t inv_last_control_frame = 0U;
uint32_t inv_control_max_cycles = 0U;
static uint32_t inv_limited_frames = 0U;
/* 主循环在输出启动成功后置1；在此之前ISR只接收ADC快照，不推进软启动。 */
static volatile uint8_t inv_outputs_started = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void INV_TestOpenLoopStep(void);
static HAL_StatusTypeDef INV_TestWaitForOffset(void);
static HAL_StatusTypeDef INV_OpenLoopStartOutputs(void);
static void INV_TestDwtInit(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 功率模式关闭时，仅开放以下六路HRTIM引脚，PE1保持低电平。 */
#define INV_TEST_HRTIM_OUTPUTS (HRTIM_OUTPUT_TC1 | HRTIM_OUTPUT_TC2 | \
                                HRTIM_OUTPUT_TD1 | HRTIM_OUTPUT_TD2 | \
                                HRTIM_OUTPUT_TE1 | HRTIM_OUTPUT_TE2)

/**
 * @brief 等待ADC3/4/5完成零点标定并形成有效的六通道快照。
 * @retval HAL_OK表示三路DMA同步运行且标定完成；超时或故障返回HAL_ERROR。
 *
 * 此时Master和Timer C/D/E只负责产生HRTIM Trigger 2，六路PWM输出及PE1
 * 均保持关闭。等待上限200 ms，正常情况下256帧在约25.6 ms内即可完成。
 */
static HAL_StatusTypeDef INV_TestWaitForOffset(void)
{
  /* start_tick是本次等待的毫秒基准；measurement是测量模块发布的一致性快照。 */
  uint32_t start_tick = HAL_GetTick();
  INV_Measurement measurement;

  while ((HAL_GetTick() - start_tick) < INV_TEST_OFFSET_TIMEOUT_MS)
  {
    INV_Measure_GetSnapshot(&measurement);

    if (measurement.fault_bits != INV_FAULT_NONE)
    {
      return HAL_ERROR;
    }

    if ((measurement.offset_ready != 0U) && (measurement.valid != 0U))
    {
      return HAL_OK;
    }
  }

  INV_Measure_Trip(INV_FAULT_ADC_SYNC);
  return HAL_TIMEOUT;
}

/**
 * @brief 按INV_OPEN_LOOP_ENABLE_POWER_STAGE选择低压带功率或仅MCU试波。
 * @retval HAL_OK表示输出按所选模式启动；Fault或GPIO状态异常返回HAL_ERROR。
 *
 * PB10在IOC中没有用户标签，因此这里按已确认的HRTIM1_FLT3引脚直接读取。
 * PE6是逆变驱动器诊断输入。二者均为低有效，任何一路为低都禁止启动。
 * 功率模式调用INV_HRTIM_EnablePowerStage()，由该函数最后拉高PE1；若将宏改为0，
 * 则只开放MCU的C/D/E引脚，并继续强制PE1为低。
 */
static HAL_StatusTypeDef INV_OpenLoopStartOutputs(void)
{
  HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

  if ((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
      (HAL_GPIO_ReadPin(INV_NFAULT_DIAG_GPIO_Port,
                       INV_NFAULT_DIAG_Pin) == GPIO_PIN_RESET) ||
      (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET))
  {
    return HAL_ERROR;
  }

  /* 三相先写入50%中性占空比，再统一开放输出，避免启动瞬间出现旧比较值。 */
  if (INV_HRTIM_SetDuty(0.5f, 0.5f, 0.5f) != HAL_OK)
  {
    return HAL_ERROR;
  }

#if (INV_OPEN_LOOP_ENABLE_POWER_STAGE != 0U)
  return INV_HRTIM_EnablePowerStage();
#else
  if (HAL_HRTIM_WaveformOutputStart(&hhrtim1,
                                    INV_TEST_HRTIM_OUTPUTS) != HAL_OK)
  {
    return HAL_ERROR;
  }

  /* 仅MCU试波模式下再次强制PE1为低，防止外部门极驱动器被意外使能。 */
  HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);
  return HAL_OK;
#endif
}

/** @brief 开启DWT周期计数器，用于测量10 kHz控制路径最坏执行时间。 */
static void INV_TestDwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 在一个新的六通道ADC同步帧到达后执行一次DDS和CBSVPWM更新。
 * @note  本函数运行于DMA中断上下文，禁止阻塞HAL、OLED、USART和HAL_Delay()。
 */
static void INV_TestOpenLoopStep(void)
{
  INV_OpenLoopOutput reference;
  uint32_t start_cycles;
  uint32_t elapsed_cycles;

  if ((INV_Measure_GetFault() != INV_FAULT_NONE) ||
      (inv_outputs_started == 0U))
  {
    return;
  }

  start_cycles = DWT->CYCCNT;
  if (!INV_OpenLoop_Step(&reference))
  {
    INV_Measure_Trip(INV_FAULT_PARAMETER);
    return;
  }

  inv_sine_u = reference.sine_u;
  inv_output_frequency_hz = reference.frequency_hz;

  if (CBSVPWM_Calc3Leg(&inv_svpwm,
                       reference.vu_command,
                       reference.vv_command,
                       reference.vw_command,
                       INV_OPEN_LOOP_DC_BUS_V) == 0U)
  {
    INV_Measure_Trip(INV_FAULT_CBSVPWM);
    return;
  }

  if (inv_svpwm.limited != 0U) {
    inv_limited_frames++;
    if (inv_limited_frames >= INV_TEST_LIMITED_MAX_FRAMES) {
      INV_Measure_Trip(INV_FAULT_CBSVPWM);
      return;
    }
  } else {
    inv_limited_frames = 0U;
  }

  if (INV_HRTIM_SetDuty(inv_svpwm.duty_u,
                        inv_svpwm.duty_v,
                        inv_svpwm.duty_w) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_PWM_COMMAND);
    return;
  }

  inv_control_heartbeat++;
  elapsed_cycles = DWT->CYCCNT - start_cycles;
  if (elapsed_cycles > inv_control_max_cycles) {
    inv_control_max_cycles = elapsed_cycles;
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
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_ADC5_Init();
  /* USER CODE BEGIN 2 */

  /* 启动阶段先建立安全态；PE1只能在ADC/Fault自检通过后由HRTIM模块最后拉高。 */
  HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PRECHARGE_EN_GPIO_Port, PRECHARGE_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DISCHARGE_EN_GPIO_Port, DISCHARGE_EN_Pin, GPIO_PIN_RESET);

  /*
   * 上电后只在初始化阶段等待一次，使OLED内部电源和电荷泵稳定。
   * 禁止把HAL_Delay()放入主循环或ADC DMA回调，否则会破坏后台调度
   * 或10 kHz开环控制节拍。等待期间四个安全控制输出仍保持低电平。
   */
  HAL_Delay(100U);
  OLED_Init();

  /*
   * 固定标签只写一次，主循环后续只更新数字区域，避免整屏清除造成闪烁：
   * IU/IV/IW为三相电流ADC原始码，UV/VW/WU为三相线电压ADC原始码；
   * 第4行显示当前30/60 Hz频率和INV_FaultMask锁存故障位。
   */
  OLED_ShowString(1U, 1U, "IU:");
  OLED_ShowString(1U, 9U, "UV:");
  OLED_ShowString(2U, 1U, "IV:");
  OLED_ShowString(2U, 9U, "VW:");
  OLED_ShowString(3U, 1U, "IW:");
  OLED_ShowString(3U, 9U, "WU:");
  OLED_ShowString(4U, 1U, "F:");
  OLED_ShowString(4U, 5U, "Hz");
  OLED_ShowString(4U, 9U, "E:");

  /* 清空测量、DMA同步序列、零点累加器、DDS软启动状态和历史故障。 */
  INV_Measure_Init();
  inv_outputs_started = 0U;
  inv_limited_frames = 0U;
  INV_TestDwtInit();

  /*
   * 5 V母线和2.5 Vrms线电压由开环模块换算成约0.816调制度。
   * 默认60 Hz；后续需要题目第5项时可调用SetFrequency(INV_FREQ_30HZ)，
   * DDS只改变相位步进，不清零当前相位，因此切换时不会产生相位跳变。
   */
  INV_OpenLoop_Init(INV_OPEN_LOOP_DC_BUS_V, INV_OPEN_LOOP_LINE_RMS_V);
  if (!INV_OpenLoop_SetFrequency(INV_OPEN_LOOP_DEFAULT_FREQUENCY))
  {
    INV_Measure_Trip(INV_FAULT_PARAMETER);
    Error_Handler();
  }

  /* CBSVPWM使用5 V开环母线参数；本阶段仍未配置独立VBUS ADC反馈。 */
  if (CBSVPWM_Init(&inv_svpwm,
                   INV_TEST_MODULATION_LIMIT,
                   INV_TEST_MINIMUM_DC_V) == 0U)
  {
    INV_Measure_Trip(INV_FAULT_CBSVPWM);
    Error_Handler();
  }

  /* ADC校准必须在规则组DMA和HRTIM Trigger 2启动之前完成。 */
  if (HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  /*
   * 三个ADC均包含两个规则Rank，DMA长度必须固定为2。先启动ADC5、ADC4，
   * 最后启动ADC3，使三路ADC在Master尚未运行时全部进入等待Trigger 2状态。
   */
  if (HAL_ADC_Start_DMA(&hadc5, (uint32_t *)INV_Adc5Dma, 2U) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)INV_Adc4Dma, 2U) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)INV_Adc3Dma, 2U) != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
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
   * 以10 kHz触发ADC3/4/5；该函数不会开放六路PWM或拉高PE1。
   */
  if (INV_HRTIM_StartSampling() != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_HRTIM);
    Error_Handler();
  }

  /*
   * 先等待ADC零点和同步链路通过，再开放MCU侧HRTIM引脚。标定期间必须
   * 保证电流、电压采样前端处于真实零输入，否则所得偏置不能用于后续测试。
   */
  if (INV_TestWaitForOffset() != HAL_OK)
  {
    Error_Handler();
  }

  if (INV_OpenLoopStartOutputs() != HAL_OK)
  {
    INV_Measure_Trip(INV_FAULT_HRTIM);
    Error_Handler();
  }
  inv_outputs_started = 1U;

  /*
   * IWDG最后启动。之后只有ADC3/4/5序列、控制心跳、Fault和安全GPIO
   * 全部健康时才刷新；任何监督异常都保留故障并等待看门狗复位。
   */
  MX_IWDG_Init();

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
  uint32_t last_adc3_sequence = 0U;
  uint32_t last_adc4_sequence = 0U;
  uint32_t last_adc5_sequence = 0U;
  uint32_t last_fast_heartbeat = 0U;
  uint32_t last_control_heartbeat = 0U;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /*
     * 每10 ms发送一次VOFA数据。阻塞式USART只允许位于主循环，禁止放入
     * 10 kHz DMA回调，否则约0.78 ms的串口发送会破坏实时控制节拍。
     */
    if ((HAL_GetTick() - vofa_tick) >= INV_TEST_VOFA_PERIOD_MS)
    {
      /* 局部副本在短暂关中断期间一次性取得，后续串口发送不再访问ISR共享对象。 */
      INV_Measurement measurement;
      INV_OpenLoopOutput reference;

      vofa_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);
      INV_OpenLoop_GetSnapshot(&reference);

      /* CH0显示当前30/60 Hz单位正弦，用来同时验证DDS节拍和USART数据顺序。 */
      vofa_data[0] = reference.sine_u;

      /* CH1~CH6依次显示三个电流和三路线电压ADC原始码，便于核对Rank。 */
      vofa_data[1] = (float)measurement.iu_raw;
      vofa_data[2] = (float)measurement.iv_raw;
      vofa_data[3] = (float)measurement.iw_raw;
      vofa_data[4] = (float)measurement.vuv_raw;
      vofa_data[5] = (float)measurement.vvw_raw;
      vofa_data[6] = (float)measurement.vwu_raw;
      vofa_data[7] = (float)measurement.fault_bits;

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
    if ((HAL_GetTick() - supervisor_tick) >= INV_TEST_SUPERVISOR_PERIOD_MS)
    {
      /* 本轮监督使用同一份完整快照比较所有ADC序列、有效标志和故障位。 */
      INV_Measurement measurement;

      supervisor_tick = HAL_GetTick();
      INV_Measure_GetSnapshot(&measurement);

      if ((measurement.adc3_sequence == last_adc3_sequence) ||
          (measurement.adc4_sequence == last_adc4_sequence) ||
          (measurement.adc5_sequence == last_adc5_sequence) ||
          (measurement.fast_heartbeat == last_fast_heartbeat) ||
          (inv_control_heartbeat == last_control_heartbeat))
      {
        INV_Measure_Trip(INV_FAULT_ADC_SYNC);
      }
      else if ((measurement.valid == 0U) ||
               (measurement.fault_bits != INV_FAULT_NONE) ||
               (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) ||
               (HAL_GPIO_ReadPin(INV_NFAULT_DIAG_GPIO_Port,
                                INV_NFAULT_DIAG_Pin) == GPIO_PIN_RESET) ||
               (HAL_GPIO_ReadPin(PFC_GATE_EN_GPIO_Port,
                                PFC_GATE_EN_Pin) != GPIO_PIN_RESET) ||
               ((INV_OPEN_LOOP_ENABLE_POWER_STAGE != 0U) &&
                (HAL_GPIO_ReadPin(INV_GATE_EN_GPIO_Port,
                                 INV_GATE_EN_Pin) != GPIO_PIN_SET)) ||
               ((INV_OPEN_LOOP_ENABLE_POWER_STAGE == 0U) &&
                (HAL_GPIO_ReadPin(INV_GATE_EN_GPIO_Port,
                                 INV_GATE_EN_Pin) != GPIO_PIN_RESET)))
      {
        INV_Measure_Trip(INV_FAULT_DRIVER);
      }
      else
      {
        /* 仅在完整安全条件成立时刷新IWDG，禁止在中断中无条件喂狗。 */
        (void)HAL_IWDG_Refresh(&hiwdg);
      }

      last_adc3_sequence = measurement.adc3_sequence;
      last_adc4_sequence = measurement.adc4_sequence;
      last_adc5_sequence = measurement.adc5_sequence;
      last_fast_heartbeat = measurement.fast_heartbeat;
      last_control_heartbeat = inv_control_heartbeat;
    }

    /*
     * 每100 ms更新一次OLED。安全监督放在本段之前执行，确保软件I2C传输
     * 不会推迟DMA失步、驱动故障或Gate Enable异常的检测与安全关断。
     */
    if ((HAL_GetTick() - oled_tick) >= INV_TEST_OLED_PERIOD_MS)
    {
      /* OLED只显示此处取得的完整快照，不直接读取DMA正在更新的原始数组。 */
      INV_Measurement measurement;
      INV_OpenLoopOutput reference;

      /* 主循环偶尔延迟时不连续补刷旧帧，而是以当前时刻作为下一周期基准。 */
      oled_tick = HAL_GetTick();

      /*
       * 读取测量模块已经发布的完整六通道快照，禁止直接访问正被DMA改写的
       * INV_Adc3Dma/INV_Adc4Dma/INV_Adc5Dma数组，避免显示半更新数据。
       */
      INV_Measure_GetSnapshot(&measurement);
      INV_OpenLoop_GetSnapshot(&reference);

      /*
       * 六路ADC为12位原始码，固定显示4位十进制数；即使前导位为0，
       * 数字区域宽度也保持不变，不需要周期性OLED_Clear()。
       */
      OLED_ShowNum(1U, 4U, measurement.iu_raw, 4U);
      OLED_ShowNum(1U, 12U, measurement.vuv_raw, 4U);
      OLED_ShowNum(2U, 4U, measurement.iv_raw, 4U);
      OLED_ShowNum(2U, 12U, measurement.vvw_raw, 4U);
      OLED_ShowNum(3U, 4U, measurement.iw_raw, 4U);
      OLED_ShowNum(3U, 12U, measurement.vwu_raw, 4U);

      /*
       * 输出频率固定显示两位十进制数；故障位用三位十六进制显示，
       * 可覆盖新增的CSS时钟故障位，正常状态为000且故障后不自动清零。
       */
      OLED_ShowNum(4U, 3U, reference.frequency_hz, 2U);
      OLED_ShowHexNum(4U, 11U, measurement.fault_bits, 3U);
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
 * 分支先发布自己的DMA序列；只有三个序列全部更新后，测量模块才把
 * fast_heartbeat增加一次。随后比较心跳，保证一个PWM周期只运行一次控制。
 *
 * 该函数运行在DMA中断上下文，禁止调用阻塞式USART或HAL_Delay()。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  /* 回调内快照用于判断三个DMA是否已拼成一个新的、可供控制使用的完整帧。 */
  INV_Measurement measurement;

  /* 标记本次回调是否来自ADC3/4/5，避免未来其他ADC误触发逆变控制入口。 */
  uint8_t inverter_adc = 1U;

  if ((hadc != NULL) && (hadc->Instance == ADC3))
  {
    INV_Measure_OnAdc3Complete();
  }
  else if ((hadc != NULL) && (hadc->Instance == ADC4))
  {
    INV_Measure_OnAdc4Complete();
  }
  else if ((hadc != NULL) && (hadc->Instance == ADC5))
  {
    INV_Measure_OnAdc5Complete();
  }
  else
  {
    inverter_adc = 0U;
  }

  if (inverter_adc == 0U)
  {
    return;
  }

  INV_Measure_GetSnapshot(&measurement);

  /*
   * 标定完成且出现新的六通道同步帧时才推进开环。三个DMA回调均会经过
   * 此处，但fast_heartbeat每帧只变化一次，因此不会重复更新CBSVPWM。
   */
  if ((measurement.offset_ready != 0U) &&
      (measurement.valid != 0U) &&
      (measurement.fast_heartbeat != inv_last_control_frame))
  {
    inv_last_control_frame = measurement.fast_heartbeat;
    INV_TestOpenLoopStep();
  }
}

/**
 * @brief ADC及其DMA错误回调，仅处理本测试启用的ADC3/4/5。
 * @param hadc 报告错误的ADC句柄；NULL或ADC1/2不会进入逆变故障处理。
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  /*
   * ADC3/4/5发生Overrun、DMA或内部状态错误时统一锁存逆变ADC故障。
   * ADC1/2在本测试中没有启动，若以后启用应由各自应用模块单独处理。
   */
  if ((hadc != NULL) &&
      ((hadc->Instance == ADC3) ||
       (hadc->Instance == ADC4) ||
       (hadc->Instance == ADC5)))
  {
    INV_Measure_AdcError();
  }
}

/**
 * @brief GPIO外部中断公共回调。
 * @param GPIO_Pin 触发EXTI的GPIO引脚掩码。
 *
 * PE6是逆变驱动器nFAULT诊断信号。该回调用于记录和锁存故障；真正的快速
 * 关断应由驱动器DESAT/OCP到HRTIM Fault 3的硬件链路完成，不依赖CPU。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == INV_NFAULT_DIAG_Pin)
  {
    INV_Measure_Trip(INV_FAULT_DRIVER);
  }
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
  (void)hhrtim;
  INV_Measure_Trip(INV_FAULT_HRTIM);
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
   * 不可恢复的初始化或HAL错误统一进入安全态：先拉低两个Gate Enable，
   * 再关闭逆变HRTIM输出和C/D/E计数器。若IWDG已经启动，最终由其复位；
   * 若IWDG尚未启动，则保持在门极关闭的死循环中等待人工复位。
   */
  HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);
  INV_OpenLoop_Reset();
  CBSVPWM_Reset(&inv_svpwm);
  INV_HRTIM_StopAll();
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
