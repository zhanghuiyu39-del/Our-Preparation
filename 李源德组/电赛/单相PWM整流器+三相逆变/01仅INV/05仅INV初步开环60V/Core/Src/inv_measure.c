#include "inv_measure.h"

#include <stddef.h>
#include <string.h>

#include "stm32g4xx.h"

/*
 * 本文件负责“ADC DMA原始值 -> 同周期快照 -> 零点/增益标定 -> 物理量”，
 * 不负责DDS、CBSVPWM、HRTIM关断、OLED或VOFA。
 *
 * 数据流：
 * HRTIM Trigger 2 -> ADC3/4/5双Rank -> 三路循环DMA -> TC回调
 * -> 序列同步 -> 六通道快照 -> 自动零点/单通道增益结果 -> 控制或主循环。
 *
 * DMA原始数组只由DMA写；控制与后台只能读取发布快照。volatile不提供三个
 * DMA数组间的一致性，一致性由三个序列均前进后再发布来保证。
 */

#define INV_ADC_RAW_MAX       (4095U)
#define INV_ADC_CHANNEL_COUNT (6U)

/* DMA独占写入；下标映射固定为IU/VUV、IV/VVW、IW/VWU。 */
volatile uint16_t INV_Adc3Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc4Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc5Dma[2] = {0U, 0U};

/* 供Keil Watch直接观察；ISR写入，主循环应优先调用快照接口。 */
volatile INV_CalibrationResult inv_calibration_result;

/* 配置在ADC/DMA启动前复制一次，采样运行期间只读。 */
static INV_MeasureConfig inv_measure_config;
static uint8_t inv_measure_initialized;

/* ADC完成ISR发布，主循环通过GetSnapshot读取。 */
static volatile INV_Measurement inv_measurement;

/* 三个DMA完成ISR分别写入；used_*表示最近一次共同发布的序列。 */
static volatile uint32_t adc3_sequence;
static volatile uint32_t adc4_sequence;
static volatile uint32_t adc5_sequence;
static uint32_t used_adc3_sequence;
static uint32_t used_adc4_sequence;
static uint32_t used_adc5_sequence;

/* 六通道零点/增益累加状态仅由发布新同步帧的ISR分支更新。 */
static uint32_t calibration_sum[INV_ADC_CHANNEL_COUNT];
static uint16_t measurement_offset[INV_ADC_CHANNEL_COUNT];
static uint16_t calibration_sample_count;

/* 每通道独立统计连续贴轨帧，防止不同通道交替异常造成误判。 */
static uint8_t rail_frame_count[INV_ADC_CHANNEL_COUNT];

/** 判断单精度数不是NaN/Inf；不引入双精度库调用。 */
static uint8_t INV_Measure_IsFinite(float value)
{
    union
    {
        float value;
        uint32_t bits;
    } converter;

    converter.value = value;
    return ((converter.bits & 0x7F800000UL) != 0x7F800000UL) ? 1U : 0U;
}

/** 允许32位序列自然回绕，计算尚未被共同消费的帧数。 */
static uint32_t INV_Measure_SequenceDistance(uint32_t current, uint32_t used)
{
    return current - used;
}

/** 任一路积压超过配置帧数，说明三个ADC/DMA不再属于稳定同一触发节拍。 */
static uint8_t INV_Measure_SyncLost(uint32_t seq3, uint32_t seq4, uint32_t seq5)
{
    return ((INV_Measure_SequenceDistance(seq3, used_adc3_sequence) >
             inv_measure_config.max_skew_frames) ||
            (INV_Measure_SequenceDistance(seq4, used_adc4_sequence) >
             inv_measure_config.max_skew_frames) ||
            (INV_Measure_SequenceDistance(seq5, used_adc5_sequence) >
             inv_measure_config.max_skew_frames)) ? 1U : 0U;
}

/** 更新一个通道的连续贴轨计数，达到确认帧数时返回1。 */
static uint8_t INV_Measure_UpdateRailCounter(uint16_t raw, uint8_t channel)
{
    if ((raw <= inv_measure_config.rail_low_count) ||
        (raw >= inv_measure_config.rail_high_count)) {
        if (rail_frame_count[channel] < inv_measure_config.rail_confirm_frames) {
            rail_frame_count[channel]++;
        }
    } else {
        rail_frame_count[channel] = 0U;
    }

    return (rail_frame_count[channel] >= inv_measure_config.rail_confirm_frames) ? 1U : 0U;
}

/** 按固定顺序组装六路原始码，顺序与配置数组和标定通道编号一致。 */
static void INV_Measure_ReadRaw(uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    raw[0] = INV_Adc3Dma[0];
    raw[1] = INV_Adc4Dma[0];
    raw[2] = INV_Adc5Dma[0];
    raw[3] = INV_Adc3Dma[1];
    raw[4] = INV_Adc4Dma[1];
    raw[5] = INV_Adc5Dma[1];
}

/** 把六路数组写入带物理名称的发布快照。 */
static void INV_Measure_PublishRaw(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    inv_measurement.iu_raw = raw[0];
    inv_measurement.iv_raw = raw[1];
    inv_measurement.iw_raw = raw[2];
    inv_measurement.vuv_raw = raw[3];
    inv_measurement.vvw_raw = raw[4];
    inv_measurement.vwu_raw = raw[5];
}

/** 处理自动零点或ZERO标定；完成后发布六路offset。 */
static void INV_Measure_ProcessZero(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    uint8_t channel;

    if (inv_measurement.offset_ready != 0U) {
        return;
    }

    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
        calibration_sum[channel] += raw[channel];
    }
    calibration_sample_count++;
    inv_calibration_result.collected_samples = calibration_sample_count;

    if (calibration_sample_count >= inv_measure_config.offset_samples) {
        for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
            measurement_offset[channel] = (uint16_t)(calibration_sum[channel] /
                                                     inv_measure_config.offset_samples);
            inv_calibration_result.offset[channel] = measurement_offset[channel];
        }
        inv_calibration_result.complete = 1U;
        inv_calibration_result.valid = 1U;
        inv_measurement.offset_ready = 1U;
    }
}

/** 处理单通道已知量标定，比例符号自动包含模拟前端极性。 */
static void INV_Measure_ProcessGain(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    uint8_t channel = inv_measure_config.calibration_channel;
    int32_t delta;
    int32_t absolute_delta;

    if (inv_calibration_result.complete != 0U) {
        return;
    }

    calibration_sum[channel] += raw[channel];
    calibration_sample_count++;
    inv_calibration_result.collected_samples = calibration_sample_count;

    if (calibration_sample_count < inv_measure_config.gain_samples) {
        return;
    }

    inv_calibration_result.raw_average =
        (uint16_t)(calibration_sum[channel] / inv_measure_config.gain_samples);
    delta = (int32_t)inv_calibration_result.raw_average -
            (int32_t)measurement_offset[channel];
    absolute_delta = (delta < 0) ? -delta : delta;
    inv_calibration_result.complete = 1U;

    if (absolute_delta >= (int32_t)inv_measure_config.minimum_gain_delta_counts) {
        inv_calibration_result.recommended_scale =
            inv_measure_config.calibration_reference / (float)delta;
        if (INV_Measure_IsFinite(inv_calibration_result.recommended_scale) != 0U) {
            inv_calibration_result.valid = 1U;
        }
    }
}

/** 使用当前offset/scale换算六路物理量；标定值未确认前只用于观察。 */
static void INV_Measure_ConvertPhysical(const uint16_t raw[INV_ADC_CHANNEL_COUNT])
{
    if (inv_measure_config.line_voltage_only != 0U) {
        /* 当前只观察线电压；电流Rank虽已由IOC转换，但不发布电流物理量。 */
        inv_measurement.iu = 0.0f;
        inv_measurement.iv = 0.0f;
        inv_measurement.iw = 0.0f;
    } else {
    inv_measurement.iu = ((float)raw[0] - (float)measurement_offset[0]) *
                         inv_measure_config.scale[0];
    inv_measurement.iv = ((float)raw[1] - (float)measurement_offset[1]) *
                         inv_measure_config.scale[1];
    inv_measurement.iw = ((float)raw[2] - (float)measurement_offset[2]) *
                         inv_measure_config.scale[2];
    }
    inv_measurement.vuv = ((float)raw[3] - (float)measurement_offset[3]) *
                          inv_measure_config.scale[3];
    inv_measurement.vvw = ((float)raw[4] - (float)measurement_offset[4]) *
                          inv_measure_config.scale[4];
    inv_measurement.vwu = ((float)raw[5] - (float)measurement_offset[5]) *
                          inv_measure_config.scale[5];
    inv_measurement.current_sum = inv_measurement.iu + inv_measurement.iv + inv_measurement.iw;
    inv_measurement.line_voltage_sum = inv_measurement.vuv +
                                       inv_measurement.vvw +
                                       inv_measurement.vwu;
}

/** 三路序列均前进后读取DMA并发布一次快照；返回值成为控制入口门控。 */
static bool INV_Measure_TryPublish(INV_Measurement *frame)
{
    uint32_t seq3 = adc3_sequence;
    uint32_t seq4 = adc4_sequence;
    uint32_t seq5 = adc5_sequence;
    uint16_t raw[INV_ADC_CHANNEL_COUNT];
    uint8_t channel;
    uint8_t rail_fault = 0U;

    if (inv_measure_initialized == 0U) {
        return false;
    }

    if ((seq3 == used_adc3_sequence) ||
        (seq4 == used_adc4_sequence) ||
        (seq5 == used_adc5_sequence)) {
        if (INV_Measure_SyncLost(seq3, seq4, seq5) != 0U) {
            INV_Measure_LatchFault(INV_FAULT_ADC_SYNC);
        }
        return false;
    }

    INV_Measure_ReadRaw(raw);
    used_adc3_sequence = seq3;
    used_adc4_sequence = seq4;
    used_adc5_sequence = seq5;
    INV_Measure_PublishRaw(raw);
    inv_measurement.adc3_sequence = seq3;
    inv_measurement.adc4_sequence = seq4;
    inv_measurement.adc5_sequence = seq5;
    inv_measurement.fast_heartbeat++;

    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
        if ((raw[channel] > INV_ADC_RAW_MAX) &&
            ((inv_measure_config.line_voltage_only == 0U) || (channel >= 3U))) {
            INV_Measure_LatchFault(INV_FAULT_ADC_RANGE);
        }
    }

    /* 标定模式允许施加接近量程边缘的已知输入，不运行贴轨关断。 */
    if (inv_measure_config.mode == INV_MEASURE_RUN_AUTO_OFFSET) {
        for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
            if ((inv_measure_config.line_voltage_only == 0U) || (channel >= 3U)) {
                rail_fault |= INV_Measure_UpdateRailCounter(raw[channel], channel);
            }
        }
        if (rail_fault != 0U) {
            INV_Measure_LatchFault(INV_FAULT_ADC_RANGE);
        }
        INV_Measure_ProcessZero(raw);
    } else if (inv_measure_config.mode == INV_MEASURE_CALIBRATE_ZERO) {
        INV_Measure_ProcessZero(raw);
    } else {
        INV_Measure_ProcessGain(raw);
    }

    if (inv_measurement.offset_ready != 0U) {
        INV_Measure_ConvertPhysical(raw);
    }

    inv_measurement.valid = ((inv_measurement.offset_ready != 0U) &&
                             (inv_measurement.fault_bits == INV_FAULT_NONE)) ? 1U : 0U;

    if (frame != NULL) {
        *frame = inv_measurement;
    }
    return true;
}

bool INV_Measure_Init(const INV_MeasureConfig *config)
{
    uint8_t channel;

    inv_measure_initialized = 0U;
    if ((config == NULL) ||
        (config->mode > INV_MEASURE_CALIBRATE_GAIN) ||
        (config->offset_samples == 0U) ||
        (config->gain_samples == 0U) ||
        (config->minimum_gain_delta_counts == 0U) ||
        (config->calibration_channel >= INV_ADC_CHANNEL_COUNT) ||
        (config->max_skew_frames == 0U) ||
        (config->rail_low_count >= config->rail_high_count) ||
        (config->rail_high_count > INV_ADC_RAW_MAX) ||
        (config->rail_confirm_frames == 0U)) {
        return false;
    }

    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
        if ((config->fixed_offset[channel] > INV_ADC_RAW_MAX) ||
            (INV_Measure_IsFinite(config->scale[channel]) == 0U) ||
            (config->scale[channel] == 0.0f)) {
            return false;
        }
    }
    if ((config->mode == INV_MEASURE_CALIBRATE_GAIN) &&
        ((INV_Measure_IsFinite(config->calibration_reference) == 0U) ||
         (config->calibration_reference == 0.0f))) {
        return false;
    }

    inv_measure_config = *config;
    INV_Adc3Dma[0] = 0U;
    INV_Adc3Dma[1] = 0U;
    INV_Adc4Dma[0] = 0U;
    INV_Adc4Dma[1] = 0U;
    INV_Adc5Dma[0] = 0U;
    INV_Adc5Dma[1] = 0U;
    (void)memset((void *)&inv_measurement, 0, sizeof(inv_measurement));
    (void)memset((void *)&inv_calibration_result, 0, sizeof(inv_calibration_result));
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

    inv_calibration_result.channel = config->calibration_channel;
    inv_calibration_result.reference_value = config->calibration_reference;
    for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
        inv_calibration_result.offset[channel] = config->fixed_offset[channel];
    }

    if (config->mode == INV_MEASURE_CALIBRATE_GAIN) {
        for (channel = 0U; channel < INV_ADC_CHANNEL_COUNT; channel++) {
            measurement_offset[channel] = config->fixed_offset[channel];
        }
        inv_measurement.offset_ready = 1U;
    }

    inv_measure_initialized = 1U;
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

    if (fault_bits == INV_FAULT_NONE) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    inv_measurement.fault_bits |= fault_bits;
    inv_measurement.valid = 0U;
    __set_PRIMASK(primask);
}

void INV_Measure_GetSnapshot(INV_Measurement *measurement)
{
    uint32_t primask;

    if (measurement == NULL) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *measurement = inv_measurement;
    __set_PRIMASK(primask);
}

void INV_Measure_GetCalibrationResult(INV_CalibrationResult *result)
{
    uint32_t primask;

    if (result == NULL) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *result = inv_calibration_result;
    __set_PRIMASK(primask);
}

uint32_t INV_Measure_GetFault(void)
{
    return inv_measurement.fault_bits;
}
