#include "adc_convert.h"
#include "adc_calib.h"

extern volatile uint16_t adc_value[];

static float adc_raw_to_phy(uint16_t raw, float offset, float gain, uint8_t dma_idx)
{
    float vin = (float)raw * 3.3f / 4095.0f;
    return (vin - offset - AdcCalib_GetBiasVin(dma_idx)) * gain;
}

float read_adc_i_a(void)
{
    return adc_raw_to_phy(adc_value[ADC_DMA_IDX_IA], ADC_IOUT_OFFSET_V, ADC_IOUT_GAIN, ADC_DMA_IDX_IA);
}

float read_adc_i_c(void)
{
    return adc_raw_to_phy(adc_value[ADC_DMA_IDX_IC], ADC_IOUT_OFFSET_V, ADC_IOUT_GAIN, ADC_DMA_IDX_IC);
}

float read_adc_u_bc(void)
{
    return adc_raw_to_phy(adc_value[ADC_DMA_IDX_UBC], ADC_UOUT_OFFSET_V, ADC_UOUT_GAIN, ADC_DMA_IDX_UBC);
}

float read_adc_vdc(void)
{
    return adc_raw_to_phy(adc_value[ADC_DMA_IDX_VDC], ADC_VDC_OFFSET_V, ADC_VDC_GAIN, ADC_DMA_IDX_VDC);
}

float read_adc_u_ab(void)
{
    return adc_raw_to_phy(adc_value[ADC_DMA_IDX_UAB], ADC_UOUT_OFFSET_V, ADC_UOUT_GAIN, ADC_DMA_IDX_UAB);
}
