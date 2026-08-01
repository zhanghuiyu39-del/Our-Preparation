/**
 ******************************************************************************
 * @file    inv_measure.c
 * @brief   ADC3/4/5双Rank DMA同步、标定与三相测量快照。
 *
 * 数据流：HRTIM Trigger2 -> ADC3/4/5 -> 三个循环DMA原始数组 ->
 * 序列拼帧 -> offset/scale换算 -> 一致性快照。本模块只记录故障和诊断，
 * 不操作HRTIM；默认放宽策略由main.c决定。
 ******************************************************************************
 */

#include "inv_measure.h"

#include "stm32g4xx_hal.h"

#include <stddef.h>
#include <string.h>

#define INV_ADC_CHANNEL_COUNT (6U)
#define INV_ADC_RAW_MAX       (4095U)

volatile uint16_t INV_Adc3Dma[2];
volatile uint16_t INV_Adc4Dma[2];
volatile uint16_t INV_Adc5Dma[2];

static INV_MeasureConfig measure_config;
static volatile INV_Measurement measurement_snapshot;
static volatile INV_CalibrationResult calibration_snapshot;
static volatile uint32_t adc3_sequence;
static volatile uint32_t adc4_sequence;
static volatile uint32_t adc5_sequence;
static uint32_t used_adc3_sequence;
static uint32_t used_adc4_sequence;
static uint32_t used_adc5_sequence;
static uint64_t calibration_sum[INV_ADC_CHANNEL_COUNT];
static uint16_t measurement_offset[INV_ADC_CHANNEL_COUNT];
static uint16_t calibration_sample_count;
static uint8_t rail_frame_count[INV_ADC_CHANNEL_COUNT];
static uint8_t measure_initialized;

static uint8_t INV_Measure_IsFinite(float value)
{
    union
    {
        float value;
        uint32_t bits;
    } number;
    number.value = value;
    return ((number.bits & 0x7F800000UL) != 0x7F800000UL) ? 1U : 0U;
}

static uint32_t INV_Measure_Distance(uint32_t newer, uint32_t older)
{
    return newer - older;
}

static uint8_t INV_Measure_SyncLost(uint32_t seq3, uint32_t seq4, uint32_t seq5)
{
    return ((INV_Measure_Distance(seq3, used_adc3_sequence) >
             measure_config.max_skew_frames) ||
            (INV_Measure_Distance(seq4, used_adc4_sequence) >
             measure_config.max_skew_frames) ||
            (INV_Measure_Distance(seq5, used_adc5_sequence) >
             measure_config.max_skew_frames)) ? 1U : 0U;
}

static uint8_t INV_Measure_UpdateRail(uint16_t raw, uint8_t channel)
{
    if ((raw <= measure_config.rail_low_count) ||
        (raw >= measure_config.rail_high_count))
    {
        if (rail_frame_count[channel] < measure_config.rail_confirm_frames)
        {
            rail_frame_count[channel]++;
        }
    }
    else
    {
        rail_frame_count[channel] = 0U;
    }
    return (rail_frame_count[channel] >= measure_config.rail_confirm_frames) ?
           1U : 0U;
}

static void INV_Measure_ReadRaw(uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    raw[0] = INV_Adc3Dma[0];
    raw[1] = INV_Adc4Dma[0];
    raw[2] = INV_Adc5Dma[0];
    raw[3] = INV_Adc3Dma[1];
    raw[4] = INV_Adc4Dma[1];
    raw[5] = INV_Adc5Dma[1];
}

static void INV_Measure_ProcessZero(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    uint8_t channel;

    if (calibration_snapshot.complete != 0U)
    {
        return;
    }
    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
    {
        calibration_sum[channel] += raw[channel];
    }
    calibration_sample_count++;
    calibration_snapshot.collected_samples = calibration_sample_count;
    if (calibration_sample_count >= measure_config.offset_samples)
    {
        for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
        {
            measurement_offset[channel] =
                (uint16_t)(calibration_sum[channel] /
                           measure_config.offset_samples);
            calibration_snapshot.offset[channel] = measurement_offset[channel];
        }
        calibration_snapshot.complete = 1U;
        calibration_snapshot.valid = 1U;
        measurement_snapshot.offset_ready = 1U;
    }
}

static void INV_Measure_ProcessGain(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    uint8_t channel = measure_config.calibration_channel;
    int32_t delta;
    int32_t absolute_delta;

    if (calibration_snapshot.complete != 0U)
    {
        return;
    }
    calibration_sum[channel] += raw[channel];
    calibration_sample_count++;
    calibration_snapshot.collected_samples = calibration_sample_count;
    if (calibration_sample_count < measure_config.gain_samples)
    {
        return;
    }

    calibration_snapshot.raw_average =
        (uint16_t)(calibration_sum[channel] / measure_config.gain_samples);
    delta = (int32_t)calibration_snapshot.raw_average -
            (int32_t)measurement_offset[channel];
    absolute_delta = (delta < 0) ? -delta : delta;
    calibration_snapshot.complete = 1U;
    if (absolute_delta >= (int32_t)measure_config.minimum_gain_delta_counts)
    {
        calibration_snapshot.recommended_scale =
            measure_config.calibration_reference / (float)delta;
        if (INV_Measure_IsFinite(calibration_snapshot.recommended_scale) != 0U)
        {
            calibration_snapshot.valid = 1U;
        }
    }
}

static uint8_t INV_Measure_Convert(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    float value[INV_ADC_CHANNEL_COUNT];
    uint8_t channel;

    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
    {
        value[channel] = ((float)raw[channel] -
                          (float)measurement_offset[channel]) *
                         measure_config.scale[channel];
        if (INV_Measure_IsFinite(value[channel]) == 0U)
        {
            return 0U;
        }
    }

    measurement_snapshot.iu = value[0];
    measurement_snapshot.iv = value[1];
    measurement_snapshot.iw = value[2];
    measurement_snapshot.vuv = value[3];
    measurement_snapshot.vvw = value[4];
    measurement_snapshot.vwu = value[5];
    measurement_snapshot.current_sum = value[0] + value[1] + value[2];
    measurement_snapshot.line_voltage_sum = value[3] + value[4] + value[5];
    return 1U;
}

static bool INV_Measure_TryPublish(INV_Measurement *frame)
{
    uint32_t seq3 = adc3_sequence;
    uint32_t seq4 = adc4_sequence;
    uint32_t seq5 = adc5_sequence;
    uint16_t raw[INV_ADC_CHANNEL_COUNT];
    uint8_t channel;
    uint8_t rail_detected = 0U;

    if (measure_initialized == 0U)
    {
        return false;
    }
    if ((seq3 == used_adc3_sequence) || (seq4 == used_adc4_sequence) ||
        (seq5 == used_adc5_sequence))
    {
        if (INV_Measure_SyncLost(seq3, seq4, seq5) != 0U)
        {
            INV_Measure_LatchDiagnostic(INV_DIAG_ADC_SYNC);
        }
        return false;
    }

    INV_Measure_ReadRaw(raw);
    used_adc3_sequence = seq3;
    used_adc4_sequence = seq4;
    used_adc5_sequence = seq5;
    measurement_snapshot.iu_raw = raw[0];
    measurement_snapshot.iv_raw = raw[1];
    measurement_snapshot.iw_raw = raw[2];
    measurement_snapshot.vuv_raw = raw[3];
    measurement_snapshot.vvw_raw = raw[4];
    measurement_snapshot.vwu_raw = raw[5];
    measurement_snapshot.adc3_sequence = seq3;
    measurement_snapshot.adc4_sequence = seq4;
    measurement_snapshot.adc5_sequence = seq5;

    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
    {
        if (raw[channel] > INV_ADC_RAW_MAX)
        {
            INV_Measure_LatchDiagnostic(INV_DIAG_ADC_RANGE);
        }
        rail_detected |= INV_Measure_UpdateRail(raw[channel], channel);
    }
    if ((rail_detected != 0U) &&
        (measure_config.mode == INV_MEASURE_RUN_AUTO_OFFSET))
    {
        INV_Measure_LatchDiagnostic(INV_DIAG_ADC_RANGE);
    }

    if ((measure_config.mode == INV_MEASURE_RUN_AUTO_OFFSET) ||
        (measure_config.mode == INV_MEASURE_CALIBRATE_ZERO))
    {
        INV_Measure_ProcessZero(raw);
    }
    else
    {
        INV_Measure_ProcessGain(raw);
    }

    if (measurement_snapshot.offset_ready != 0U)
    {
        if (INV_Measure_Convert(raw) == 0U)
        {
            INV_Measure_LatchDiagnostic(INV_DIAG_PARAMETER);
        }
    }
    measurement_snapshot.fast_heartbeat++;
    measurement_snapshot.valid =
        ((measurement_snapshot.offset_ready != 0U) &&
         (measurement_snapshot.fault_bits == INV_FAULT_NONE)) ? 1U : 0U;
    if (frame != NULL)
    {
        *frame = measurement_snapshot;
    }
    return true;
}

bool INV_Measure_Init(const INV_MeasureConfig *config)
{
    uint8_t channel;

    if ((config == NULL) || (config->offset_samples == 0U) ||
        (config->gain_samples == 0U) ||
        (config->minimum_gain_delta_counts == 0U) ||
        (config->calibration_channel >= INV_ADC_CHANNEL_COUNT) ||
        (config->max_skew_frames == 0U) ||
        (config->rail_low_count >= config->rail_high_count) ||
        (config->rail_high_count > INV_ADC_RAW_MAX) ||
        (config->rail_confirm_frames == 0U) ||
        (INV_Measure_IsFinite(config->calibration_reference) == 0U))
    {
        return false;
    }
    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
    {
        if ((config->fixed_offset[channel] > INV_ADC_RAW_MAX) ||
            (INV_Measure_IsFinite(config->scale[channel]) == 0U) ||
            (config->scale[channel] == 0.0f))
        {
            return false;
        }
    }

    measure_config = *config;
    (void)memset((void *)INV_Adc3Dma, 0, sizeof(INV_Adc3Dma));
    (void)memset((void *)INV_Adc4Dma, 0, sizeof(INV_Adc4Dma));
    (void)memset((void *)INV_Adc5Dma, 0, sizeof(INV_Adc5Dma));
    (void)memset((void *)&measurement_snapshot, 0, sizeof(measurement_snapshot));
    (void)memset((void *)&calibration_snapshot, 0, sizeof(calibration_snapshot));
    (void)memset(calibration_sum, 0, sizeof(calibration_sum));
    (void)memset(measurement_offset, 0, sizeof(measurement_offset));
    (void)memset(rail_frame_count, 0, sizeof(rail_frame_count));
    adc3_sequence = 0U;
    adc4_sequence = 0U;
    adc5_sequence = 0U;
    used_adc3_sequence = 0U;
    used_adc4_sequence = 0U;
    used_adc5_sequence = 0U;
    calibration_sample_count = 0U;
    calibration_snapshot.channel = config->calibration_channel;
    calibration_snapshot.reference_value = config->calibration_reference;
    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
    {
        calibration_snapshot.offset[channel] = config->fixed_offset[channel];
    }
    if (config->mode == INV_MEASURE_CALIBRATE_GAIN)
    {
        for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++)
        {
            measurement_offset[channel] = config->fixed_offset[channel];
        }
        measurement_snapshot.offset_ready = 1U;
    }
    measure_initialized = 1U;
    return true;
}

bool INV_Measure_OnAdc3Complete(INV_Measurement *frame)
{
    adc3_sequence++;
    return INV_Measure_TryPublish(frame);
}

bool INV_Measure_OnAdc4Complete(INV_Measurement *frame)
{
    adc4_sequence++;
    return INV_Measure_TryPublish(frame);
}

bool INV_Measure_OnAdc5Complete(INV_Measurement *frame)
{
    adc5_sequence++;
    return INV_Measure_TryPublish(frame);
}

void INV_Measure_LatchFault(uint32_t fault_bits)
{
    uint32_t primask;
    if (fault_bits == INV_FAULT_NONE)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    measurement_snapshot.fault_bits |= fault_bits;
    measurement_snapshot.valid = 0U;
    __set_PRIMASK(primask);
}

void INV_Measure_LatchDiagnostic(uint32_t diagnostic_bits)
{
    uint32_t primask;
    if (diagnostic_bits == INV_DIAG_NONE)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    measurement_snapshot.diagnostic_bits |= diagnostic_bits;
    __set_PRIMASK(primask);
}

void INV_Measure_GetSnapshot(INV_Measurement *measurement)
{
    uint32_t primask;
    if (measurement == NULL)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    *measurement = measurement_snapshot;
    __set_PRIMASK(primask);
}

void INV_Measure_GetCalibrationResult(INV_CalibrationResult *result)
{
    uint32_t primask;
    if (result == NULL)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    *result = calibration_snapshot;
    __set_PRIMASK(primask);
}

uint32_t INV_Measure_GetFault(void)
{
    return measurement_snapshot.fault_bits;
}

uint32_t INV_Measure_GetDiagnostic(void)
{
    return measurement_snapshot.diagnostic_bits;
}
