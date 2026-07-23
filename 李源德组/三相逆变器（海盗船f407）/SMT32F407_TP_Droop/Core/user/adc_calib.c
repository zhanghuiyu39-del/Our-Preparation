#include "adc_calib.h"

extern volatile uint16_t adc_value[];

typedef enum {
    ADC_CALIB_STATE_WAIT_POWERON = 0,
    ADC_CALIB_STATE_RUNNING,
    ADC_CALIB_STATE_IDLE
} AdcCalibState;

#define ADC_CALIB_CH_COUNT  4U

static const uint8_t s_calib_dma_idx[ADC_CALIB_CH_COUNT] = {
    ADC_DMA_IDX_IA,
    ADC_DMA_IDX_IC,
    ADC_DMA_IDX_UBC,
    ADC_DMA_IDX_UAB,
};

static const float s_calib_base_offset_v[ADC_CALIB_CH_COUNT] = {
    ADC_IOUT_OFFSET_V,
    ADC_IOUT_OFFSET_V,
    ADC_UOUT_OFFSET_V,
    ADC_UOUT_OFFSET_V,
};

static AdcCalibState s_state = ADC_CALIB_STATE_WAIT_POWERON;
static uint32_t s_wait_samples = 0;
static uint8_t s_auto_pending = 0;
static uint16_t s_sample_count = 0;
static float s_sum_vin[ADC_CALIB_CH_COUNT];
static float s_bias_vin[5];

static float raw_to_vin(uint16_t raw)
{
    return (float)raw * 3.3f / 4095.0f;
}

static void calib_reset_accum(void)
{
    uint8_t i;

    s_sample_count = 0;
    for (i = 0; i < ADC_CALIB_CH_COUNT; i++) {
        s_sum_vin[i] = 0.0f;
    }
}

static void calib_start_running(void)
{
    calib_reset_accum();
    s_state = ADC_CALIB_STATE_RUNNING;
}

static void calib_finish(void)
{
    float inv_n = 1.0f / (float)ADC_CALIB_SAMPLE_COUNT;
    uint8_t i;

    for (i = 0; i < ADC_CALIB_CH_COUNT; i++) {
        uint8_t idx = s_calib_dma_idx[i];
        float avg_vin = s_sum_vin[i] * inv_n;

        s_bias_vin[idx] = avg_vin - s_calib_base_offset_v[i];
    }

    calib_reset_accum();
    s_state = ADC_CALIB_STATE_IDLE;
}

void AdcCalib_Init(void)
{
    uint8_t i;

    for (i = 0; i < 5U; i++) {
        s_bias_vin[i] = 0.0f;
    }

    s_wait_samples = ADC_CALIB_DELAY_SAMPLES;
    s_auto_pending = 1U;
    s_state = ADC_CALIB_STATE_WAIT_POWERON;
    calib_reset_accum();
}

void AdcCalib_Request(void)
{
    calib_start_running();
}

void AdcCalib_ProcessISR(void)
{
    uint8_t i;

    if (s_state == ADC_CALIB_STATE_WAIT_POWERON) {
        if (s_wait_samples > 0U) {
            s_wait_samples--;
            return;
        }

        if (s_auto_pending) {
            s_auto_pending = 0U;
            calib_start_running();
        }
        return;
    }

    if (s_state != ADC_CALIB_STATE_RUNNING) {
        return;
    }

    for (i = 0; i < ADC_CALIB_CH_COUNT; i++) {
        uint8_t idx = s_calib_dma_idx[i];
        s_sum_vin[i] += raw_to_vin(adc_value[idx]);
    }

    s_sample_count++;
    if (s_sample_count >= ADC_CALIB_SAMPLE_COUNT) {
        calib_finish();
    }
}

uint8_t AdcCalib_IsBusy(void)
{
    return (s_state == ADC_CALIB_STATE_RUNNING) ? 1U : 0U;
}

float AdcCalib_GetBiasVin(uint8_t dma_idx)
{
    if (dma_idx >= 5U) {
        return 0.0f;
    }
    return s_bias_vin[dma_idx];
}
