#include "pfc_measure.h"
#include "pfc_hrtim.h"

/* ===== 板级参数：先用于低压验证，接入功率板前必须按实测修改 ===== */
#define PFC_ADC_OFFSET_SAMPLES   256U
#define PFC_ADC_RAW_MIN          0U
#define PFC_ADC_RAW_MAX          4095U
#define PFC_IPFC_A_PER_COUNT     0.001f
#define PFC_VAC_V_PER_COUNT      0.010f
#define PFC_VBUS_V_PER_COUNT     0.010f

volatile uint16_t PFC_Adc1Dma[2] = {0U, 0U};
volatile uint16_t PFC_Adc2Dma[1] = {0U};

static volatile PFC_Measurement pfc_measurement = {0};
static volatile uint32_t adc2_sequence = 0U;
static uint32_t last_adc2_sequence = 0U;
static uint32_t ipfc_offset_sum = 0U;
static uint32_t vac_offset_sum = 0U;
static uint16_t offset_sample_count = 0U;
static uint16_t ipfc_offset = 2048U;
static uint16_t vac_offset = 2048U;
static uint8_t sync_miss_count = 0U;

void PFC_Measure_Init(void)
{
    PFC_Adc1Dma[0] = 0U;
    PFC_Adc1Dma[1] = 0U;
    PFC_Adc2Dma[0] = 0U;
    pfc_measurement = (PFC_Measurement){0};
    adc2_sequence = 0U;
    last_adc2_sequence = 0U;
    ipfc_offset_sum = 0U;
    vac_offset_sum = 0U;
    offset_sample_count = 0U;
    ipfc_offset = 2048U;
    vac_offset = 2048U;
    sync_miss_count = 0U;
}

/* ADC2 只有 VAC，一个序列完成只发布时序标记。 */
void PFC_Measure_OnAdc2Complete(void)
{
    adc2_sequence++;
}

/* ADC1 序列完成后读取三个同周期样本，并作为唯一的 10 kHz 快速入口。 */
void PFC_Measure_OnAdc1Complete(void)
{
    uint32_t current_adc2_sequence = adc2_sequence;
    int32_t ipfc_count;
    int32_t vac_count;

    if (current_adc2_sequence == last_adc2_sequence)
    {
        if (++sync_miss_count >= 3U)
        {
            PFC_Measure_Trip(PFC_FAULT_ADC_SYNC);
        }
        return;
    }

    sync_miss_count = 0U;
    last_adc2_sequence = current_adc2_sequence;

    pfc_measurement.ipfc_raw = PFC_Adc1Dma[0];
    pfc_measurement.vbus_raw = PFC_Adc1Dma[1];
    pfc_measurement.vac_raw = PFC_Adc2Dma[0];
    pfc_measurement.adc1_sequence++;
    pfc_measurement.adc2_sequence = current_adc2_sequence;

    if ((pfc_measurement.ipfc_raw < PFC_ADC_RAW_MIN) ||
        (pfc_measurement.ipfc_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.vbus_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.vac_raw > PFC_ADC_RAW_MAX))
    {
        PFC_Measure_Trip(PFC_FAULT_ADC_RANGE);
        return;
    }

    /* 无交流、无驱动时自动记录 IPFC/VAC 的 ADC 零点。 */
    if (offset_sample_count < PFC_ADC_OFFSET_SAMPLES)
    {
        ipfc_offset_sum += pfc_measurement.ipfc_raw;
        vac_offset_sum += pfc_measurement.vac_raw;
        offset_sample_count++;

        if (offset_sample_count == PFC_ADC_OFFSET_SAMPLES)
        {
            ipfc_offset = (uint16_t)(ipfc_offset_sum / PFC_ADC_OFFSET_SAMPLES);
            vac_offset = (uint16_t)(vac_offset_sum / PFC_ADC_OFFSET_SAMPLES);
            pfc_measurement.offset_ready = 1U;
        }
        return;
    }

    ipfc_count = (int32_t)pfc_measurement.ipfc_raw - (int32_t)ipfc_offset;
    vac_count = (int32_t)pfc_measurement.vac_raw - (int32_t)vac_offset;
    pfc_measurement.ipfc = (float)ipfc_count * PFC_IPFC_A_PER_COUNT;
    pfc_measurement.vac = (float)vac_count * PFC_VAC_V_PER_COUNT;
    pfc_measurement.vbus = (float)pfc_measurement.vbus_raw * PFC_VBUS_V_PER_COUNT;
    pfc_measurement.fast_heartbeat++;
    pfc_measurement.valid = 1U;
}

void PFC_Measure_AdcError(void)
{
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
}

/* 软件故障只做锁存，不自动恢复。 */
void PFC_Measure_Trip(uint32_t fault_bits)
{
    pfc_measurement.fault_bits |= fault_bits;
    pfc_measurement.valid = 0U;
    PFC_HRTIM_StopAll();
}

void PFC_Measure_GetSnapshot(PFC_Measurement *measurement)
{
    uint32_t primask;

    if (measurement == 0)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *measurement = pfc_measurement;
    __set_PRIMASK(primask);
}

uint32_t PFC_Measure_GetFault(void)
{
    return pfc_measurement.fault_bits;
}
