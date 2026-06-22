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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "SPWM.h"
#include "hhPID.h"
#include "measure.h"
#include "arm_math.h"
#include "hqpr.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


// 工频参数 
#define F0      50.0f		//工频50Hz
#define W0      2*3.1415926f*F0
#define WC      10.0f    // PR谐振带宽
#define Ts      0.00005f // 20kHz 50us

// ==================== PWM 调制度范围 ====================
#define PWM_MAX   800   // 调制度上限
#define PWM_MIN   100       // 调制度下限
#define PWM_MID   500     // 中点（正弦波上下调制）


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

//ADC2------------
/*命名说明：
ADC_开头的是ADC的数字量0-4095
U_ I_开头的是换算后的电压电流值
*/
QPR_TypeDef  Voltage_QPR;   // 电压外环
QPR_TypeDef  Current_QPR;   // 电流内环

uint16_t AD_Value[2];//DMA转移的ADC值
extern uint8_t protect_status;  //具体怎么用？

const float VOLTAGE_RATIO = 20.07f; // 电压变比系数  (根据实际硬件调整) 
const float CURRENT_RATIO = 0.05f; // 电流互感器比例 (根据实际硬件调整) 

float U_fb;      // ADC 采样输出电压
float I_fb;      // ADC 采样输出电流

uint16_t i;    //for循环软启动

extern float inverter_duty;

//USART2-------------------
//float V_real_raw = 0.0f;
//float I_real_raw = 0.0f;
//float I_calibrated = 0.0f;//校准后实际负载电流值

float Theta = 0.0f;       // 相位角
float U_amp = 25.4f;     // 18V有效值对应的幅值

//OLED
char str[50];  // 存放浮点数转化的字符串
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 在20kHz中断最开头调用

float Generate_U_Ref(void)
{
    // 相位步进：w0 * Ts
    Theta += W0 * Ts;
    
    // 相位限幅 0~2PI
    if(Theta >= 2*PI)  
        Theta -= 2*PI;
    
    // 瞬时电压给定
    return U_amp * sinf(Theta);
}

//USART2+VOFA+-----------------
void Serial_SendFloat(float data)
{
    uint8_t *bytes = (uint8_t*)&data;
    for(int i = 0; i < 4; i++) {
        hhSerialSendByte(bytes[i]);
    }
		
    hhSerialSendByte(0x00);
    hhSerialSendByte(0x00);
    hhSerialSendByte(0x80);
    hhSerialSendByte(0x7F); 
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
	OLED_Init();
	OLED_Clear();
	
  // ==================== 电流内环（先整定）====================
	// 标准最优参数：Kp=0.6，Kr=40，wc=10
	QPR_Init(&Current_QPR, 0.6f,  40.0f, WC, W0);

	// ==================== 电压外环（后整定）====================
	// 标准最优参数：Kp=1.2，Kr=80，wc=10
	QPR_Init(&Voltage_QPR, 1.2f,  80.0f, WC, W0);

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC2_Init();
  MX_TIM4_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */
  

		//TIM4_50us20khz--------------------------------------
	__HAL_TIM_CLEAR_FLAG(&htim4,TIM_FLAG_UPDATE);
	HAL_TIM_Base_Start_IT(&htim4);
	
	
	//TIM8+TIM1+SPWM--------------------------
    //	HAL_TIM_PWM_Start(&htim8,TIM_CHANNEL_1);
	//	HAL_TIM_PWM_Start(&htim8,TIM_CHANNEL_2);
		HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);
  

	
	
	//ADC2內部基准校准(+DMA)------------------------------------
	HAL_ADCEx_Calibration_Start(&hadc2,ADC_SINGLE_ENDED);
	
	/*ADC采样值通过dma传递到AD_Value[]数组，本函数只是开启DMA，其位置不影响采样速度
	要更改速度要在CUBEMX中设置触发源，目前设置成TIM4中断触发
	*/
	HAL_ADC_Start_DMA(&hadc2,(uint32_t*)&AD_Value, 2);
	
  //硬启动
	inverter_duty = 50;
	
//软启动，缓慢上升调制系数，调制系数为0	
  inverter_duty = 50;
	for(i=1;i<= 920;i++)                 //缓慢上升调制系数
	  {
		  inverter_duty=i;
		  HAL_Delay(1);
	  } 
i= 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */


		OLED_ShowString(1,1,"V_raw:");
		OLED_ShowString(2,1,"I_raw:");
		OLED_ShowNum(1,7,AD_Value[0],4);
		OLED_ShowNum(2,7,AD_Value[1],4);
		
		sprintf(str, "%.2f", U_fb);
		OLED_ShowString(3,1,str);
		OLED_ShowNum(4,1,inverter_duty,4);

		HAL_Delay(10);

//	//USART2+VOFA+--------------------------------
//	Serial_SendFloat();
//	Serial_SendFloat();
//	HAL_Delay(50);
//	  

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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 75;
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

//定时器中断，TIM4+SPWM----------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//RMS计算	
	static uint16_t count=0;
	count++;
	
	float U_ref;     // 电压正弦给定（Amp*sinθ）

	float I_ref;     // 电压环输出 = 电流给定
	float PWM_Out;   // 电流环输出 = 调制波
	
	if(htim==&htim4)
	{ 

		//SPWM启动-------------
		//inverter_spwm_T8_1();
		//inverter_spwm_T8_2();
		inverter_spwm_T1_1();
		inverter_spwm_T1_3();

		ADC_Data_Update();
		//RMS计算
//		AC_rms.V_rms = measure_rms(0, 100) * 0.00080586f * VOLTAGE_RATIO;  //使用电压通道和电压比例系数
//		AC_rms.I_rms = measure_rms(1, 100) * 0.00080586f * CURRENT_RATIO;  //使用电流通道和电流比例系数
		//相当于/ 4095.0f * 3.3f
		
		if(count > 400)
		{
			
			U_ref = Generate_U_Ref(); // 拿到瞬时正弦给定
			calculate_rms();
			//	瞬时值控制
			// ========== 1. 读取采样值（你只需改这两个函数）==========
			U_fb = get_voltage_sample();  // 你的电压采样函数
			I_fb = get_current_sample();  // 你的电流采样函数

			// ========== 2. 电压 QPR 外环 ==========
			I_ref = QPR_Calc(&Voltage_QPR, U_ref, U_fb, Ts);

			// ========== 电流限幅（必须加！）==========
			if(I_ref >  5.0f) I_ref =  5.0f;   // 根据你的电感/额定电流改
			if(I_ref < -5.0f) I_ref = -5.0f;

			// ========== 3. 电流 QPR 内环 ==========
			PWM_Out = QPR_Calc(&Current_QPR, I_ref, I_fb, Ts);

			// ========== 4. 调制波限幅 + 映射到 0~1000 ==========
			if(PWM_Out >  PWM_MAX) PWM_Out =  PWM_MAX;   // 正幅值
			if(PWM_Out < -PWM_MIN) PWM_Out = -PWM_MIN;   // 负幅值

			//uint16_t inverter_duty = (uint16_t)(PWM_MID + PWM_Out); // 100~900
			
			count = 410;
		}
		
		if(inverter_duty>=920)inverter_duty=920;
		if(inverter_duty<=200)inverter_duty=200;

		__HAL_TIM_CLEAR_FLAG(&htim4,TIM_FLAG_UPDATE);
		
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
