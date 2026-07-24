#ifndef __ADC_CONVERT_H__
#define __ADC_CONVERT_H__

#include "main.h"

/* 硬件采样口 (ADC1 扫描 rank 顺序)
 * ADC1 / Rank1 / PA0: A相电流
 * ADC2 / Rank2 / PA1: C相电流
 * ADC3 / Rank3 / PA4: BC线电压
 * ADC4 / Rank4 / PA5: 直流母线
 * ADC5 / Rank5 / PC0: AB线电压
 */
#define ADC1_IA_Pin         GPIO_PIN_0
#define ADC1_IA_GPIO_Port   GPIOA
#define ADC2_IC_Pin         GPIO_PIN_1
#define ADC2_IC_GPIO_Port   GPIOA
#define ADC3_UBC_Pin        GPIO_PIN_4
#define ADC3_UBC_GPIO_Port  GPIOA
#define ADC4_VDC_Pin        GPIO_PIN_5
#define ADC4_VDC_GPIO_Port  GPIOA
#define ADC5_UAB_Pin        GPIO_PIN_0
#define ADC5_UAB_GPIO_Port  GPIOC

#define ADC_DMA_IDX_IA      0U
#define ADC_DMA_IDX_IC      1U
#define ADC_DMA_IDX_UBC     2U
#define ADC_DMA_IDX_VDC     3U
#define ADC_DMA_IDX_UAB     4U

#define ADC_UOUT_OFFSET_V   1.631f
#define ADC_UOUT_GAIN       -33.552f
#define ADC_IOUT_OFFSET_V   1.65f
#define ADC_IOUT_GAIN       -4.0f
#define ADC_VDC_OFFSET_V    0.09f
#define ADC_VDC_GAIN        16.9f

#define ADC_VDC_MIN_VALID   1.0f
#define ADC_VDC_FALLBACK    40.0f

float read_adc_u_ab(void);
float read_adc_u_bc(void);
float read_adc_i_a(void);
float read_adc_i_c(void);
float read_adc_vdc(void);

#endif /* __ADC_CONVERT_H__ */
