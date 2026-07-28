#include "inv_measure.h"

#include <stddef.h>

#include "inv_hrtim.h"

/*
 * 数据流：HRTIM Trigger 2 -> ADC3/4/5 -> 三路循环DMA -> 三个完成回调
 *       -> TryPublish建立同周期六通道快照 -> 控制ISR或主循环读取。
 * 本模块不包含QPR、CBSVPWM、OLED、VOFA或任何阻塞HAL调用。
 */

/* ======================== 板级标定参数 ======================== */
#define INV_ADC_OFFSET_SAMPLES (256U)
#define INV_ADC_RAW_MAX        (4095U)
#define INV_ADC_MAX_SKEW       (3U)

/*
 * 物理量=(原始值-零点)*比例。以下比例仅为低压联调占位值，
 * 必须按电流传感器、分压器和运放增益实测后修改。
 */
#define INV_IU_A_PER_COUNT (0.001f)
#define INV_IV_A_PER_COUNT (0.001f)
#define INV_IW_A_PER_COUNT (0.001f)
#define INV_VU_V_PER_COUNT (0.010f)
#define INV_VV_V_PER_COUNT (0.010f)
#define INV_VW_V_PER_COUNT (0.010f)

/* ======================== DMA原始缓冲区 ======================== */
volatile uint16_t INV_Adc3Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc4Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc5Dma[2] = {0U, 0U};

/* ======================== 模块内部状态 ======================== */
static volatile INV_Measurement inv_measurement;
static volatile uint32_t adc3_sequence;
static volatile uint32_t adc4_sequence;
static volatile uint32_t adc5_sequence;
static uint32_t used_adc3_sequence;
static uint32_t used_adc4_sequence;
static uint32_t used_adc5_sequence;

/* 六路前端默认均为双极性中点采样，启动后用256个完整帧求零点。 */
static uint32_t iu_offset_sum;
static uint32_t iv_offset_sum;
static uint32_t iw_offset_sum;
static uint32_t vu_offset_sum;
static uint32_t vv_offset_sum;
static uint32_t vw_offset_sum;
static uint16_t offset_sample_count;
static uint16_t iu_offset;
static uint16_t iv_offset;
static uint16_t iw_offset;
static uint16_t vu_offset;
static uint16_t vv_offset;
static uint16_t vw_offset;

/* 计算无符号序号相对已消费帧的距离，允许32位序号自然回绕。 */
static uint32_t INV_Measure_SequenceDistance(uint32_t current, uint32_t used)
{
    return current - used;
}

/* 三路DMA失步达到三个触发周期时，锁存故障，避免永久混用不同周期数据。 */
static uint8_t INV_Measure_SyncLost(uint32_t seq3, uint32_t seq4, uint32_t seq5)
{
    if ((INV_Measure_SequenceDistance(seq3, used_adc3_sequence) >= INV_ADC_MAX_SKEW) ||
        (INV_Measure_SequenceDistance(seq4, used_adc4_sequence) >= INV_ADC_MAX_SKEW) ||
        (INV_Measure_SequenceDistance(seq5, used_adc5_sequence) >= INV_ADC_MAX_SKEW)) {
        return 1U;
    }

    return 0U;
}

/* 三路DMA均有新序列时，按固定Rank顺序发布一次完整测量帧。 */
static void INV_Measure_TryPublish(void)
{
    uint32_t seq3 = adc3_sequence;
    uint32_t seq4 = adc4_sequence;
    uint32_t seq5 = adc5_sequence;
    int32_t signed_count;

    if ((seq3 == used_adc3_sequence) ||
        (seq4 == used_adc4_sequence) ||
        (seq5 == used_adc5_sequence)) {
        if (INV_Measure_SyncLost(seq3, seq4, seq5) != 0U) {
            INV_Measure_Trip(INV_FAULT_ADC_SYNC);
        }
        return;
    }

    /* 序号必须相等才属于同一次HRTIM触发，禁止把超前ADC与上一周期数据拼帧。 */
    if ((seq3 != seq4) || (seq3 != seq5)) {
        if (INV_Measure_SyncLost(seq3, seq4, seq5) != 0U) {
            INV_Measure_Trip(INV_FAULT_ADC_SYNC);
        }
        return;
    }

    /* 一次性消费三个新序列，保证fast_heartbeat每个HRTIM触发只增加一次。 */
    used_adc3_sequence = seq3;
    used_adc4_sequence = seq4;
    used_adc5_sequence = seq5;

    inv_measurement.iu_raw = INV_Adc3Dma[0];
    inv_measurement.vu_raw = INV_Adc3Dma[1];
    inv_measurement.iv_raw = INV_Adc4Dma[0];
    inv_measurement.vv_raw = INV_Adc4Dma[1];
    inv_measurement.iw_raw = INV_Adc5Dma[0];
    inv_measurement.vw_raw = INV_Adc5Dma[1];
    inv_measurement.adc3_sequence = seq3;
    inv_measurement.adc4_sequence = seq4;
    inv_measurement.adc5_sequence = seq5;
    inv_measurement.fast_heartbeat++;

    /* uint16_t可暴露DMA宽度或内存破坏导致的超12位异常。 */
    if ((inv_measurement.iu_raw > INV_ADC_RAW_MAX) ||
        (inv_measurement.iv_raw > INV_ADC_RAW_MAX) ||
        (inv_measurement.iw_raw > INV_ADC_RAW_MAX) ||
        (inv_measurement.vu_raw > INV_ADC_RAW_MAX) ||
        (inv_measurement.vv_raw > INV_ADC_RAW_MAX) ||
        (inv_measurement.vw_raw > INV_ADC_RAW_MAX)) {
        INV_Measure_Trip(INV_FAULT_ADC_RANGE);
        return;
    }

    /* Master可能在Fault后继续触发ADC；故障锁存期间绝不重新发布valid数据。 */
    if (inv_measurement.fault_bits != INV_FAULT_NONE) {
        inv_measurement.valid = 0U;
        return;
    }

    /* 标定阶段不发布有效物理量，PE1和六路PWM输出必须保持关闭。 */
    if (offset_sample_count < INV_ADC_OFFSET_SAMPLES) {
        iu_offset_sum += inv_measurement.iu_raw;
        iv_offset_sum += inv_measurement.iv_raw;
        iw_offset_sum += inv_measurement.iw_raw;
        vu_offset_sum += inv_measurement.vu_raw;
        vv_offset_sum += inv_measurement.vv_raw;
        vw_offset_sum += inv_measurement.vw_raw;
        offset_sample_count++;

        if (offset_sample_count == INV_ADC_OFFSET_SAMPLES) {
            iu_offset = (uint16_t)(iu_offset_sum / INV_ADC_OFFSET_SAMPLES);
            iv_offset = (uint16_t)(iv_offset_sum / INV_ADC_OFFSET_SAMPLES);
            iw_offset = (uint16_t)(iw_offset_sum / INV_ADC_OFFSET_SAMPLES);
            vu_offset = (uint16_t)(vu_offset_sum / INV_ADC_OFFSET_SAMPLES);
            vv_offset = (uint16_t)(vv_offset_sum / INV_ADC_OFFSET_SAMPLES);
            vw_offset = (uint16_t)(vw_offset_sum / INV_ADC_OFFSET_SAMPLES);
            inv_measurement.offset_ready = 1U;
        }
        return;
    }

    /* 先以有符号32位完成零点扣除，再乘每码值比例得到A或V。 */
    signed_count = (int32_t)inv_measurement.iu_raw - (int32_t)iu_offset;
    inv_measurement.iu = (float)signed_count * INV_IU_A_PER_COUNT;
    signed_count = (int32_t)inv_measurement.iv_raw - (int32_t)iv_offset;
    inv_measurement.iv = (float)signed_count * INV_IV_A_PER_COUNT;
    signed_count = (int32_t)inv_measurement.iw_raw - (int32_t)iw_offset;
    inv_measurement.iw = (float)signed_count * INV_IW_A_PER_COUNT;
    signed_count = (int32_t)inv_measurement.vu_raw - (int32_t)vu_offset;
    inv_measurement.vu = (float)signed_count * INV_VU_V_PER_COUNT;
    signed_count = (int32_t)inv_measurement.vv_raw - (int32_t)vv_offset;
    inv_measurement.vv = (float)signed_count * INV_VV_V_PER_COUNT;
    signed_count = (int32_t)inv_measurement.vw_raw - (int32_t)vw_offset;
    inv_measurement.vw = (float)signed_count * INV_VW_V_PER_COUNT;

    inv_measurement.valid = 1U;
}

void INV_Measure_Init(void)
{
    /* DMA启动前清零，避免调试器先看到上一次运行留下的数据。 */
    INV_Adc3Dma[0] = 0U;
    INV_Adc3Dma[1] = 0U;
    INV_Adc4Dma[0] = 0U;
    INV_Adc4Dma[1] = 0U;
    INV_Adc5Dma[0] = 0U;
    INV_Adc5Dma[1] = 0U;

    inv_measurement = (INV_Measurement){0};
    adc3_sequence = 0U;
    adc4_sequence = 0U;
    adc5_sequence = 0U;
    used_adc3_sequence = 0U;
    used_adc4_sequence = 0U;
    used_adc5_sequence = 0U;

    iu_offset_sum = 0U;
    iv_offset_sum = 0U;
    iw_offset_sum = 0U;
    vu_offset_sum = 0U;
    vv_offset_sum = 0U;
    vw_offset_sum = 0U;
    offset_sample_count = 0U;
    iu_offset = 2048U;
    iv_offset = 2048U;
    iw_offset = 2048U;
    vu_offset = 2048U;
    vv_offset = 2048U;
    vw_offset = 2048U;
}

void INV_Measure_OnAdc3Complete(void)
{
    adc3_sequence++;
    INV_Measure_TryPublish();
}

void INV_Measure_OnAdc4Complete(void)
{
    adc4_sequence++;
    INV_Measure_TryPublish();
}

void INV_Measure_OnAdc5Complete(void)
{
    adc5_sequence++;
    INV_Measure_TryPublish();
}

void INV_Measure_AdcError(void)
{
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
}

void INV_Measure_Trip(uint32_t fault_bits)
{
    inv_measurement.fault_bits |= fault_bits;
    inv_measurement.valid = 0U;
    INV_HRTIM_StopAll();
}

void INV_Measure_GetSnapshot(INV_Measurement *measurement)
{
    uint32_t primask;

    if (measurement == NULL) {
        return;
    }

    /* 恢复进入函数前的PRIMASK，避免误开启调用者原本已经关闭的中断。 */
    primask = __get_PRIMASK();
    __disable_irq();
    *measurement = inv_measurement;
    __set_PRIMASK(primask);
}

uint32_t INV_Measure_GetFault(void)
{
    return inv_measurement.fault_bits;
}
