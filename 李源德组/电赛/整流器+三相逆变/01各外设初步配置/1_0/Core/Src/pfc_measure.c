#include "pfc_measure.h"
#include "pfc_hrtim.h"

/*
 * 本文件只负责“ADC原始值 -> 同周期快照 -> 物理量”的处理，不负责PI/PR控制。
 * 数据流如下：
 * HRTIM Trigger1 -> ADC1/ADC2 -> DMA缓冲区 -> HAL_ADC_ConvCpltCallback()
 *                -> 本模块建立快照 -> 主循环/OLED读取
 *
 * ADC1需要转换两个Rank，因此完成时间比只有一个Rank的ADC2稍晚。
 * ADC2先更新时间序号，ADC1完成后再读取三路数据，可避免把不同PWM周期的数据混用。
 *
 * IPFC：PFC 电感电流或输入电流的 ADC 采样值。
 * VAC：交流输入电压采样值。 
 * VBUS：PFC 升压后的直流母线电压采样值。
 */

/* ======================== 板级标定参数 ======================== */
#define PFC_ADC_OFFSET_SAMPLES   256U   /* 10 kHz下约25.6 ms，用于计算IPFC/VAC零点。 */
#define PFC_ADC_RAW_MAX          4095U  /* 12位ADC允许的最大原始码值。 */

/*
 * 物理量 = (ADC原始值 - 零点) * 每码值对应的物理量。
 * 以下数值只是低压测试占位值，功率板确定后必须由分压器和传感器参数重新计算。
 */
#define PFC_IPFC_A_PER_COUNT     0.001f /* IPFC：每个ADC码值对应的安培数。 */
#define PFC_VAC_V_PER_COUNT      0.010f /* VAC：每个ADC码值对应的伏特数。 */
#define PFC_VBUS_V_PER_COUNT     0.010f /* VBUS：单极性采样，不减去中点零偏。 */

/* ======================== DMA原始缓冲区 ======================== */
/*
 * 这两个数组由DMA写入，CPU只读取，所以必须使用volatile。
 * 数组顺序必须与CubeMX中的Regular Rank保持完全一致：
 * ADC1[0]=Rank1/IPFC，ADC1[1]=Rank2/VBUS；ADC2[0]=Rank1/VAC。
 */
volatile uint16_t PFC_Adc1Dma[2] = {0U, 0U};
volatile uint16_t PFC_Adc2Dma[1] = {0U};

/* ======================== 模块内部状态 ======================== */
static volatile PFC_Measurement pfc_measurement = {0}; /* ISR发布，主循环通过快照函数读取。 */
static volatile uint32_t adc2_sequence = 0U;           /* ADC2每完成一次DMA就加1。 */
static uint32_t last_adc2_sequence = 0U;               /* ADC1上次已使用的ADC2序号。 */

/* 下面三个量只在上电零点标定阶段使用。 */
static uint32_t ipfc_offset_sum = 0U;
static uint32_t vac_offset_sum = 0U;
static uint16_t offset_sample_count = 0U;
static uint16_t ipfc_offset = 2048U; /* 标定前先给中点默认值，标定完成后会覆盖。 */
static uint16_t vac_offset = 2048U;

static uint8_t sync_miss_count = 0U; /* 连续3次没有等到新的ADC2数据才报错，避免偶发时序误判。 */

/**
 * @brief  清空DMA缓冲区、采样序号、故障状态和零点标定累加器。
 * @note   在HAL_ADC_Start_DMA()之前调用一次；当前main.c在ADC校准前调用。
 */
void PFC_Measure_Init(void)
{
    /* DMA启动前清零，避免OLED或调试器先看到上一次运行残留的数据。 */
    PFC_Adc1Dma[0] = 0U;
    PFC_Adc1Dma[1] = 0U;
    PFC_Adc2Dma[0] = 0U;

    /* 复位对外发布的完整测量结果。 */
    pfc_measurement = (PFC_Measurement){0};

    /* 复位ADC同步判断和自动零点标定状态。 */
    adc2_sequence = 0U;
    last_adc2_sequence = 0U;
    ipfc_offset_sum = 0U;
    vac_offset_sum = 0U;
    offset_sample_count = 0U;
    ipfc_offset = 2048U;
    vac_offset = 2048U;
    sync_miss_count = 0U;
}

/**
 * @brief  ADC2规则组DMA完成处理，表示本周期的VAC已经更新。
 * @note   由HAL_ADC_ConvCpltCallback()调用；这里只加序号，不做换算和OLED显示。
 */
void PFC_Measure_OnAdc2Complete(void)
{
    adc2_sequence++;
}

/**
 * @brief  ADC1规则组DMA完成处理，建立一次IPFC/VBUS/VAC同步快照。
 * @note   本函数运行在DMA中断中，后续PR电流环也应从这里每100 us执行一次。
 */
void PFC_Measure_OnAdc1Complete(void)
{
    /* 32位变量在Cortex-M4上可一次读完，先保存局部副本避免处理中途再次变化。 */
    uint32_t current_adc2_sequence = adc2_sequence;
    int32_t ipfc_count;
    int32_t vac_count;

    /*
     * ADC1完成时，ADC2序号必须已经前进。
     * 若序号没变，说明VAC仍是上一个PWM周期的数据，本次数据不能用于控制。
     */
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

    /* DMA已经完成整个规则序列，此时按固定Rank顺序复制三路原始值。 */
    pfc_measurement.ipfc_raw = PFC_Adc1Dma[0];
    pfc_measurement.vbus_raw = PFC_Adc1Dma[1];
    pfc_measurement.vac_raw = PFC_Adc2Dma[0];
    pfc_measurement.adc1_sequence++;
    pfc_measurement.adc2_sequence = current_adc2_sequence;

    /* 12位ADC正常数据只能是0~4095，超出范围说明配置或内存数据异常。 */
    if ((pfc_measurement.ipfc_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.vbus_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.vac_raw > PFC_ADC_RAW_MAX))
    {
        PFC_Measure_Trip(PFC_FAULT_ADC_RANGE);
        return;
    }

    /*
     * 上电后的前256组数据只用于求零点，不发布有效物理量。
     * 此时必须保持无交流输入、无负载电流、Gate Enable关闭。
     */
    if (offset_sample_count < PFC_ADC_OFFSET_SAMPLES)
    {
        ipfc_offset_sum += pfc_measurement.ipfc_raw;
        vac_offset_sum += pfc_measurement.vac_raw;
        offset_sample_count++;

        if (offset_sample_count == PFC_ADC_OFFSET_SAMPLES)
        {
            /* 使用平均值抑制单次ADC噪声，得到双极性采样前端的中点。 */
            ipfc_offset = (uint16_t)(ipfc_offset_sum / PFC_ADC_OFFSET_SAMPLES);
            vac_offset = (uint16_t)(vac_offset_sum / PFC_ADC_OFFSET_SAMPLES);
            pfc_measurement.offset_ready = 1U;
        }
        return;
    }

    /* IPFC和VAC以中点为0，因此先转为有符号差值；VBUS是单极性量，不减零点。 */
    ipfc_count = (int32_t)pfc_measurement.ipfc_raw - (int32_t)ipfc_offset;
    vac_count = (int32_t)pfc_measurement.vac_raw - (int32_t)vac_offset;
    pfc_measurement.ipfc = (float)ipfc_count * PFC_IPFC_A_PER_COUNT;
    pfc_measurement.vac = (float)vac_count * PFC_VAC_V_PER_COUNT;
    pfc_measurement.vbus = (float)pfc_measurement.vbus_raw * PFC_VBUS_V_PER_COUNT;

    /* 心跳每个有效采样周期加1，正常情况下每秒约增加10000。 */
    pfc_measurement.fast_heartbeat++;
    pfc_measurement.valid = 1U;
}

/**
 * @brief  HAL报告ADC或DMA错误时的统一入口。
 */
void PFC_Measure_AdcError(void)
{
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
}

/**
 * @brief  锁存故障并立即关闭Gate Enable、HRTIM输出和采样时基。
 * @param  fault_bits 可一次传入一个或多个PFC_FaultMask位。
 * @note   使用“或”运算保留历史故障；本函数不会自动清除故障或重新启动PWM。
 */
void PFC_Measure_Trip(uint32_t fault_bits)
{
    pfc_measurement.fault_bits |= fault_bits;
    pfc_measurement.valid = 0U;
    PFC_HRTIM_StopAll();
}

/**
 * @brief  将ISR维护的测量结果复制给主循环。
 * @param  measurement 主循环提供的目标结构体地址。
 * @note   复制期间短暂关中断，防止结构体复制到一半时被DMA回调改写。
 *         当前工程只允许在主循环调用，因此复制完成后直接恢复中断。
 */
void PFC_Measure_GetSnapshot(PFC_Measurement *measurement)
{
    if (measurement == 0)
    {
        return;
    }

    __disable_irq();
    *measurement = pfc_measurement;
    __enable_irq();
}

/**
 * @brief  返回当前锁存故障位，0表示没有软件故障。
 */
uint32_t PFC_Measure_GetFault(void)
{
    return pfc_measurement.fault_bits;
}
