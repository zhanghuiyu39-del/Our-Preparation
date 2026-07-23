#ifndef __ADC_CALIB_H__
#define __ADC_CALIB_H__

#include "adc_convert.h"
#include "control.h"

/* 上电后延迟再自动校准 (28374 约 2s 预充电后再采 1000 点) */
#define ADC_CALIB_POWERON_DELAY_MS   500u
#define ADC_CALIB_SAMPLE_COUNT       1000u
#define ADC_CALIB_DELAY_SAMPLES      ((uint32_t)((ADC_CALIB_POWERON_DELAY_MS * PWM_FREQ_HZ) / 1000u))

void AdcCalib_Init(void);
void AdcCalib_ProcessISR(void);
void AdcCalib_Request(void);
uint8_t AdcCalib_IsBusy(void);
float AdcCalib_GetBiasVin(uint8_t dma_idx);

#endif /* __ADC_CALIB_H__ */
