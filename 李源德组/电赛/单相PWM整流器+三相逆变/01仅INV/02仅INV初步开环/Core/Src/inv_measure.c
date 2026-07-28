#include "inv_measure.h"

#include <stddef.h>

#include "inv_hrtim.h"

/*
 * 数据流：HRTIM Trigger 2 -> ADC3/4/5 -> 三路循环DMA -> 三个完成回调
 *       -> TryPublish建立同周期六通道快照 -> 控制ISR或主循环读取。
 * 本模块不包含QPR、CBSVPWM、OLED、VOFA或任何阻塞HAL调用。
 *
 * IU/IV/IW：三相逆变器输出电流采样值，换算后单位A。
 * VU/VV/VW：三相逆变器输出电压采样值，换算后单位V。
 * ADC3/4/5分别提供U/V/W相，每个ADC的Rank 1为电流、Rank 2为电压。
 */

/* ======================== 板级标定参数 ======================== */
/* 使用256个同步完整帧求平均零点；10 kHz采样时约需25.6 ms。 */
#define INV_ADC_OFFSET_SAMPLES (256U)
/* 12位ADC的最大合法原始码。 */
#define INV_ADC_RAW_MAX        (4095U)
/* 任一路尚未被共同消费的序列距离达到3帧时，判定三路DMA失步。 */
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
/* DMA负责写入；中断和主循环应通过已发布快照读取，不直接拼接这些数组。 */
volatile uint16_t INV_Adc3Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc4Dma[2] = {0U, 0U};
volatile uint16_t INV_Adc5Dma[2] = {0U, 0U};

/* ======================== 模块内部状态 ======================== */
/* 唯一对外发布的测量对象，由ADC完成中断维护，主循环通过GetSnapshot复制。 */
static volatile INV_Measurement inv_measurement;

/* 各DMA完整传输回调各自递增对应序号，用于判断三个ADC是否属于同一触发帧。 */
static volatile uint32_t adc3_sequence;
static volatile uint32_t adc4_sequence;
static volatile uint32_t adc5_sequence;

/* 最近一次已经共同拼帧并消费的序号，防止同一DMA帧被重复发布。 */
static uint32_t used_adc3_sequence;
static uint32_t used_adc4_sequence;
static uint32_t used_adc5_sequence;

/* 六路前端默认均为双极性中点采样，启动后用256个完整帧求零点。 */
/* 32位累加器保存256帧原始码之和，容量覆盖4095*256且不会溢出。 */
static uint32_t iu_offset_sum;
static uint32_t iv_offset_sum;
static uint32_t iw_offset_sum;
static uint32_t vu_offset_sum;
static uint32_t vv_offset_sum;
static uint32_t vw_offset_sum;
/* 已参与零点平均的同步帧数，达到INV_ADC_OFFSET_SAMPLES后停止累加。 */
static uint16_t offset_sample_count;
/* 六路零输入平均原始码，后续换算物理量时作为ADC零点扣除。 */
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
    /* 先各读取一次volatile序号，本次拼帧判断始终使用同一组局部副本。 */
    uint32_t seq3 = adc3_sequence;
    uint32_t seq4 = adc4_sequence;
    uint32_t seq5 = adc5_sequence;
    /* 复用的有符号中间量，保存“原始码-零点”，避免无符号减法下溢。 */
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

/**
 * @brief  清空DMA缓冲区、同步序号、故障状态及六路零点标定累加器。
 * @note   在ADC3/4/5校准和HAL_ADC_Start_DMA()之前调用一次；采样运行时禁止调用。
 */
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

/**
 * @brief  ADC3规则组DMA完成处理，表示本周期IU和VU两个Rank均已更新。
 * @note   由HAL_ADC_ConvCpltCallback()调用；函数随后尝试建立六通道同步快照。
 */
void INV_Measure_OnAdc3Complete(void)
{
    adc3_sequence++;
    INV_Measure_TryPublish();
}

/**
 * @brief  ADC4规则组DMA完成处理，表示本周期IV和VV两个Rank均已更新。
 * @note   由HAL_ADC_ConvCpltCallback()调用；函数随后尝试建立六通道同步快照。
 */
void INV_Measure_OnAdc4Complete(void)
{
    adc4_sequence++;
    INV_Measure_TryPublish();
}

/**
 * @brief  ADC5规则组DMA完成处理，表示本周期IW和VW两个Rank均已更新。
 * @note   由HAL_ADC_ConvCpltCallback()调用；函数随后尝试建立六通道同步快照。
 */
void INV_Measure_OnAdc5Complete(void)
{
    adc5_sequence++;
    INV_Measure_TryPublish();
}

/**
 * @brief  HAL报告ADC或DMA错误时的统一入口。
 * @note   锁存INV_FAULT_ADC_ERROR，并通过测量故障入口立即关闭逆变输出。
 */
void INV_Measure_AdcError(void)
{
    INV_Measure_Trip(INV_FAULT_ADC_ERROR);
}

/**
 * @brief  锁存故障并立即关闭逆变Gate Enable、HRTIM输出和C/D/E计数器。
 * @param  fault_bits 可一次传入一个或多个INV_FaultMask位。
 * @note   使用“或”运算保留历史故障；本函数不会自动清除故障或重启PWM。
 */
void INV_Measure_Trip(uint32_t fault_bits)
{
    inv_measurement.fault_bits |= fault_bits;
    inv_measurement.valid = 0U;
    INV_HRTIM_StopAll();
}

/**
 * @brief  将ADC中断维护的完整六通道测量结果复制给调用者。
 * @param  measurement 调用者提供的目标结构体地址；NULL输入会直接返回。
 * @note   复制期间短暂屏蔽中断，并恢复调用前的PRIMASK，保证快照字段来自同一时刻。
 */
void INV_Measure_GetSnapshot(INV_Measurement *measurement)
{
    /* 保存调用者原有中断屏蔽状态，复制完成后按原状态恢复。 */
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

/**
 * @brief  返回当前锁存的INV_FaultMask组合，0表示没有软件故障。
 */
uint32_t INV_Measure_GetFault(void)
{
    return inv_measurement.fault_bits;
}
