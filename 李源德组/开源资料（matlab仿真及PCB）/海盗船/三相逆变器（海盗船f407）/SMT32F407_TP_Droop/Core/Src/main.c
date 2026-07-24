/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include "../user/adc_convert.h"
#include "../user/adc_calib.h"
#include "../user/control.h"
#include "../user/control_dq.h"
#include "../user/dq_power.h"
#include "../user/vofa.h"

volatile uint16_t adc_value[5];

void SystemClock_Config(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM8_Init();
  MX_USART1_UART_Init();

  /* TIM8 主通道计数触发 ADC; 互补通道未启动, MOE=0, 封波全低 */
  ControlDq_TimAdcTriggerStart();

  Vofa_Init();
  Control_Init();

  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_value, 5);

  while (1)
  {
    Vofa_Send();
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    DqAdcSample sample;
    float vdc;
    float vofa_ch[4];

    if (hadc->Instance != ADC1) {
        return;
    }

    AdcCalib_ProcessISR();
    Control_UpdateKeys();
		sample.u_ab = read_adc_u_ab();
		sample.u_bc = read_adc_u_bc();
		sample.i_a = read_adc_i_a();
		sample.i_c = read_adc_i_c();
		vdc = read_adc_vdc();
    if (!Control_IsEnabled()) {
        Control_PwmStop();
        ControlDq_UpdateMeasurement(&sample);
    } else {
        ControlDq_Execute(&sample, vdc);
    }

    /* VOFA JustFloat: CH0下垂频率(Hz) CH1电压期望(V) CH2有功(W) */
//    vofa_ch[0] = sample.u_ab;
//    vofa_ch[1] = sample.u_bc;
//    vofa_ch[2] = sample.i_a;
//		vofa_ch[3] = sample.i_c;
		vofa_ch[0] =dq_svpwm_ta;
		vofa_ch[1] =dq_svpwm_tb;
		vofa_ch[2] =dq_svpwm_tc;
    Vofa_PushISR(vofa_ch);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
