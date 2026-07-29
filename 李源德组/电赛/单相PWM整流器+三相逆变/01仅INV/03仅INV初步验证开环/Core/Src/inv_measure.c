#include "inv_measure.h"

#include <stddef.h>
#include <string.h>

#include "stm32g4xx.h"

/*
 * 本文件只负责“ADC DMA原始值 -> 同周期快照 -> 低压占位物理量”，
 * 不负责DDS、CBSVPWM、HRTIM关断、OLED或VOFA。
 *
 * 数据流如下：
 * HRTIM Trigger 2 -> ADC3/4/5双Rank规则组 -> 三路循环DMA
 * -> 三个Transfer Complete回调 -> 序列同步检查 -> 六通道一致性快照
 * -> 10 kHz控制路径/主循环读取
 *
 * volatile DMA数组只保证编译器实际访问内存，不保证多数组同步；只有三个
 * 序列均前进后发布的INV_Measurement才可作为同一PWM周期的测量数据。
 */

#define INV_ADC_OFFSET_SAMPLES (256U)  /**< 10 kHz下约25.6 ms零点平均。 */
#define INV_ADC_RAW_MAX        (4095U) /**< 12位ADC最大原始码。 */
#define INV_ADC_MAX_SKEW       (2U)    /**< 三路序列允许的最大未消费帧差。 */
#define INV_ADC_RAIL_LOW       (16U)   /**< 软件贴近低电源轨阈值。 */
#define INV_ADC_RAIL_HIGH      (4079U) /**< 软件贴近高电源轨阈值。 */
#define INV_ADC_RAIL_FRAMES    (8U)    /**< 连续约0.8 ms贴轨才锁存故障。 */
#define INV_ADC_CHANNEL_COUNT  (6U)

/*
 * 物理量=(原始码-零点)*比例。以下均为低压测试占位值，不得用于赛题
 * 过流/过压判断；必须按采样板分压、传感器和运放增益实测后重新标定。
 */
#define INV_IU_A_PER_COUNT  (0.001f)
#define INV_IV_A_PER_COUNT  (0.001f)
#define INV_IW_A_PER_COUNT  (0.001f)
#define INV_VUV_V_PER_COUNT (0.010f)
#define INV_VVW_V_PER_COUNT (0.010f)
#define INV_VWU_V_PER_COUNT (0.010f)

/* DMA独占写入；应用代码禁止绕过快照直接组合三个数组。 */
volatile uint16_t INV_Adc3Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc4Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc5Dma[2] = {0U, 0U};

/* ADC完成ISR发布，主循环通过GetSnapshot读取。 */
static volatile INV_Measurement inv_measurement;

/* 三个DMA完成ISR分别写入；used_*表示最近一次共同发布并消费的序列。 */
static volatile uint32_t adc3_sequence;
static volatile uint32_t adc4_sequence;
static volatile uint32_t adc5_sequence;
static uint32_t used_adc3_sequence;
static uint32_t used_adc4_sequence;
static uint32_t used_adc5_sequence;

/* 六通道零点累加器、结果和样本数只由发布新同步帧的ISR分支更新。 */
static uint32_t iu_offset_sum;
static uint32_t iv_offset_sum;
static uint32_t iw_offset_sum;
static uint32_t vuv_offset_sum;
static uint32_t vvw_offset_sum;
static uint32_t vwu_offset_sum;
static uint16_t offset_sample_count;
static uint16_t iu_offset;
static uint16_t iv_offset;
static uint16_t iw_offset;
static uint16_t vuv_offset;
static uint16_t vvw_offset;
static uint16_t vwu_offset;

/* 每通道独立统计连续贴轨帧，防止不同通道交替异常造成误判。 */
static uint8_t rail_frame_count[INV_ADC_CHANNEL_COUNT];

/** 允许32位序列自然回绕，计算尚未被共同消费的帧数。 */
static uint32_t INV_Measure_SequenceDistance(uint32_t current, uint32_t used)
{
    return current - used;
}

/** 任一路积压超过2帧说明三个ADC/DMA不再属于稳定的同一触发节拍。 */
static uint8_t INV_Measure_SyncLost(uint32_t seq3, uint32_t seq4, uint32_t seq5)
{
    return ((INV_Measure_SequenceDistance(seq3, used_adc3_sequence) > INV_ADC_MAX_SKEW) ||
            (INV_Measure_SequenceDistance(seq4, used_adc4_sequence) > INV_ADC_MAX_SKEW) ||
            (INV_Measure_SequenceDistance(seq5, used_adc5_sequence) > INV_ADC_MAX_SKEW)) ? 1U : 0U;
}

/** 更新一个通道的连续贴轨计数，达到8帧时返回1。 */
static uint8_t INV_Measure_UpdateRailCounter(uint16_t raw, uint8_t channel)
{
    if ((raw <= INV_ADC_RAIL_LOW) || (raw >= INV_ADC_RAIL_HIGH)) {
        if (rail_frame_count[channel] < INV_ADC_RAIL_FRAMES) {
            rail_frame_count[channel]++;
        }
    } else {
        rail_frame_count[channel] = 0U;
    }

    return (rail_frame_count[channel] >= INV_ADC_RAIL_FRAMES) ? 1U : 0U;
}

/** 三路序列均前进后读取DMA并发布一次快照；返回值直接成为控制入口门控。 */
static bool INV_Measure_TryPublish(INV_Measurement *frame)
{
    uint32_t seq3 = adc3_sequence;
    uint32_t seq4 = adc4_sequence;
    uint32_t seq5 = adc5_sequence;
    uint16_t iu_raw;
    uint16_t iv_raw;
    uint16_t iw_raw;
    uint16_t vuv_raw;
    uint16_t vvw_raw;
    uint16_t vwu_raw;
    uint8_t rail_fault;

    if ((seq3 == used_adc3_sequence) ||
        (seq4 == used_adc4_sequence) ||
        (seq5 == used_adc5_sequence)) {
        if (INV_Measure_SyncLost(seq3, seq4, seq5) != 0U) {
            INV_Measure_LatchFault(INV_FAULT_ADC_SYNC);
        }
        return false;
    }

    /* 此分支只在对应两个Rank均DMA完成后执行，因此每个数组内部是一致序列。 */
    iu_raw = INV_Adc3Dma[0];
    vuv_raw = INV_Adc3Dma[1];
    iv_raw = INV_Adc4Dma[0];
    vvw_raw = INV_Adc4Dma[1];
    iw_raw = INV_Adc5Dma[0];
    vwu_raw = INV_Adc5Dma[1];

    used_adc3_sequence = seq3;
    used_adc4_sequence = seq4;
    used_adc5_sequence = seq5;

    inv_measurement.iu_raw = iu_raw;
    inv_measurement.iv_raw = iv_raw;
    inv_measurement.iw_raw = iw_raw;
    inv_measurement.vuv_raw = vuv_raw;
    inv_measurement.vvw_raw = vvw_raw;
    inv_measurement.vwu_raw = vwu_raw;
    inv_measurement.adc3_sequence = seq3;
    inv_measurement.adc4_sequence = seq4;
    inv_measurement.adc5_sequence = seq5;
    inv_measurement.fast_heartbeat++;

    if ((iu_raw > INV_ADC_RAW_MAX) || (iv_raw > INV_ADC_RAW_MAX) ||
        (iw_raw > INV_ADC_RAW_MAX) || (vuv_raw > INV_ADC_RAW_MAX) ||
        (vvw_raw > INV_ADC_RAW_MAX) || (vwu_raw > INV_ADC_RAW_MAX)) {
        INV_Measure_LatchFault(INV_FAULT_ADC_RANGE);
    }

    rail_fault = INV_Measure_UpdateRailCounter(iu_raw, 0U);
    rail_fault |= INV_Measure_UpdateRailCounter(iv_raw, 1U);
    rail_fault |= INV_Measure_UpdateRailCounter(iw_raw, 2U);
    rail_fault |= INV_Measure_UpdateRailCounter(vuv_raw, 3U);
    rail_fault |= INV_Measure_UpdateRailCounter(vvw_raw, 4U);
    rail_fault |= INV_Measure_UpdateRailCounter(vwu_raw, 5U);
    if (rail_fault != 0U) {
        INV_Measure_LatchFault(INV_FAULT_ADC_RANGE);
    }

    if ((inv_measurement.offset_ready == 0U) &&
        (inv_measurement.fault_bits == INV_FAULT_NONE)) {
        iu_offset_sum += iu_raw;
        iv_offset_sum += iv_raw;
        iw_offset_sum += iw_raw;
        vuv_offset_sum += vuv_raw;
        vvw_offset_sum += vvw_raw;
        vwu_offset_sum += vwu_raw;
        offset_sample_count++;

        if (offset_sample_count >= INV_ADC_OFFSET_SAMPLES) {
            iu_offset = (uint16_t)(iu_offset_sum / INV_ADC_OFFSET_SAMPLES);
            iv_offset = (uint16_t)(iv_offset_sum / INV_ADC_OFFSET_SAMPLES);
            iw_offset = (uint16_t)(iw_offset_sum / INV_ADC_OFFSET_SAMPLES);
            vuv_offset = (uint16_t)(vuv_offset_sum / INV_ADC_OFFSET_SAMPLES);
            vvw_offset = (uint16_t)(vvw_offset_sum / INV_ADC_OFFSET_SAMPLES);
            vwu_offset = (uint16_t)(vwu_offset_sum / INV_ADC_OFFSET_SAMPLES);
            inv_measurement.offset_ready = 1U;
        }
    }

    if (inv_measurement.offset_ready != 0U) {
        inv_measurement.iu = ((float)iu_raw - (float)iu_offset) * INV_IU_A_PER_COUNT;
        inv_measurement.iv = ((float)iv_raw - (float)iv_offset) * INV_IV_A_PER_COUNT;
        inv_measurement.iw = ((float)iw_raw - (float)iw_offset) * INV_IW_A_PER_COUNT;
        inv_measurement.vuv = ((float)vuv_raw - (float)vuv_offset) * INV_VUV_V_PER_COUNT;
        inv_measurement.vvw = ((float)vvw_raw - (float)vvw_offset) * INV_VVW_V_PER_COUNT;
        inv_measurement.vwu = ((float)vwu_raw - (float)vwu_offset) * INV_VWU_V_PER_COUNT;
        inv_measurement.current_sum = inv_measurement.iu + inv_measurement.iv + inv_measurement.iw;
        inv_measurement.line_voltage_sum = inv_measurement.vuv +
                                           inv_measurement.vvw +
                                           inv_measurement.vwu;
    }

    inv_measurement.valid = ((inv_measurement.offset_ready != 0U) &&
                             (inv_measurement.fault_bits == INV_FAULT_NONE)) ? 1U : 0U;

    if (frame != NULL) {
        *frame = inv_measurement;
    }
    return true;
}

void INV_Measure_Init(void)
{
    INV_Adc3Dma[0] = 0U;
    INV_Adc3Dma[1] = 0U;
    INV_Adc4Dma[0] = 0U;
    INV_Adc4Dma[1] = 0U;
    INV_Adc5Dma[0] = 0U;
    INV_Adc5Dma[1] = 0U;

    (void)memset((void *)&inv_measurement, 0, sizeof(inv_measurement));
    adc3_sequence = 0U;
    adc4_sequence = 0U;
    adc5_sequence = 0U;
    used_adc3_sequence = 0U;
    used_adc4_sequence = 0U;
    used_adc5_sequence = 0U;
    iu_offset_sum = 0U;
    iv_offset_sum = 0U;
    iw_offset_sum = 0U;
    vuv_offset_sum = 0U;
    vvw_offset_sum = 0U;
    vwu_offset_sum = 0U;
    offset_sample_count = 0U;
    iu_offset = 0U;
    iv_offset = 0U;
    iw_offset = 0U;
    vuv_offset = 0U;
    vvw_offset = 0U;
    vwu_offset = 0U;
    (void)memset(rail_frame_count, 0, sizeof(rail_frame_count));
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

uint32_t INV_Measure_GetFault(void)
{
    return inv_measurement.fault_bits;
}
