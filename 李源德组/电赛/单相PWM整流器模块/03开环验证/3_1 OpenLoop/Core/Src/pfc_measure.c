#include "pfc_measure.h"
#include "pfc_hrtim.h"
#include <math.h>

/*
 * 本模块负责“ADC原始值 -> 同周期快照 -> 物理量与工频统计”，不负责状态机和调制。
 * 数据流如下：
 * HRTIM Master CMP2 -> ADC1/ADC2 Regular -> Circular DMA
 *                    -> ADC2发布VAC序列号
 *                    -> ADC1核对序列号并发布完整PFC_Measurement
 *                    -> 10 kHz控制入口和主循环读取快照
 *
 * ADC1 DMA缓冲区由DMA写，ADC1完成ISR读；pfc_measurement由ADC1完成ISR写，主循环通过
 * PFC_Measure_GetSnapshot()读。volatile只禁止编译器缓存，不保证结构体复制的原子性。
 */
#define PFC_ADC_RAW_MAX          4095U
#define PFC_ADC_RAIL_MARGIN      4U
#define PFC_RMS_WINDOW_SAMPLES   200U  /* 10 kHz下对应一个标称50 Hz周期。 */
#define PFC_ZERO_HYSTERESIS_V    0.20f

/* DMA固定Rank顺序：ADC1[0]=IPFC、ADC1[1]=VBUS、ADC2[0]=VAC，不允许与CubeMX脱节。 */
volatile uint16_t PFC_Adc1Dma[2] = {0U, 0U};
volatile uint16_t PFC_Adc2Dma[1] = {0U};

/* ISR发布的测量快照；主循环不得直接访问DMA数组或该静态对象。 */
static volatile PFC_Measurement pfc_measurement = {0};
static const PFC_Params *pfc_params = 0;       /* 初始化后只读，生命周期覆盖整个程序运行期。 */
static volatile uint32_t adc2_sequence = 0U;  /* ADC2 DMA完成ISR写，ADC1 DMA完成ISR读。 */
static uint32_t last_adc2_sequence = 0U;
static uint32_t samples_since_cross = 0U;      /* 两次VAC正向过零之间的10 kHz样本数。 */
static float vac_square_sum = 0.0f;            /* 200点RMS窗口平方和，仅ADC1完成ISR更新。 */
static float ipfc_square_sum = 0.0f;
static uint16_t rms_sample_count = 0U;
static int8_t vac_sign_state = -1;             /* 带滞回的VAC符号状态，-1等待正向过零。 */
static uint8_t sync_miss_count = 0U;

/**
 * @brief  初始化DMA缓冲区、同步序列和测量统计状态。
 * @param  params 常驻只读参数地址，函数返回后仍必须保持有效。
 * @note   在HAL_ADC_Start_DMA()之前调用；本函数不校准ADC、不启动DMA，也不开放PWM/Gate。
 */
void PFC_Measure_Init(const PFC_Params *params)
{
    PFC_Adc1Dma[0] = 0U;
    PFC_Adc1Dma[1] = 0U;
    PFC_Adc2Dma[0] = 0U;
    pfc_measurement = (PFC_Measurement){0};
    pfc_params = params;
    adc2_sequence = 0U;
    last_adc2_sequence = 0U;
    samples_since_cross = 0U;
    vac_square_sum = 0.0f;
    ipfc_square_sum = 0.0f;
    rms_sample_count = 0U;
    vac_sign_state = -1;
    sync_miss_count = 0U;

    if (params != 0)
    {
        pfc_measurement.offset_ready = 1U;
    }
}

/**
 * @brief  发布ADC2本周期VAC已经完成的信息。
 * @note   仅由ADC2 DMA完整传输回调调用；只推进序列号，不执行浮点换算和控制。
 */
void PFC_Measure_OnAdc2Complete(void)
{
    adc2_sequence++;
}

/**
 * @brief  核对ADC2序列并发布一次IPFC/VBUS/VAC一致性快照。
 * @note   由ADC1 DMA完整传输回调以10 kHz调用；包含换算、RMS累计、过零和快速保护。
 *         不得在这里调用OLED、阻塞USART或HAL_Delay()。
 */
void PFC_Measure_OnAdc1Complete(void)
{
    uint32_t current_adc2_sequence = adc2_sequence;
    uint32_t minimum_period_samples;
    uint32_t maximum_period_samples;
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

    if ((pfc_params == 0) ||
        (pfc_measurement.ipfc_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.vbus_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.vac_raw > PFC_ADC_RAW_MAX) ||
        (pfc_measurement.ipfc_raw <= PFC_ADC_RAIL_MARGIN) ||
        (pfc_measurement.ipfc_raw >= (PFC_ADC_RAW_MAX - PFC_ADC_RAIL_MARGIN)) ||
        (pfc_measurement.vac_raw <= PFC_ADC_RAIL_MARGIN) ||
        (pfc_measurement.vac_raw >= (PFC_ADC_RAW_MAX - PFC_ADC_RAIL_MARGIN)))
    {
        PFC_Measure_Trip(PFC_FAULT_ADC_RANGE);
        return;
    }

    ipfc_count = (int32_t)pfc_measurement.ipfc_raw - (int32_t)pfc_params->ipfc_zero_count;
    vac_count = (int32_t)pfc_measurement.vac_raw - (int32_t)pfc_params->vac_zero_count;
    pfc_measurement.ipfc = (float)(ipfc_count * pfc_params->ipfc_polarity) * pfc_params->ipfc_amp_per_count;
    pfc_measurement.vac = (float)(vac_count * pfc_params->vac_polarity) * pfc_params->vac_volt_per_count;
    pfc_measurement.vbus = (float)pfc_measurement.vbus_raw * pfc_params->vbus_volt_per_count;

    /* 每200点才执行一次sqrtf，避免在10 kHz ISR中每周期开平方。 */
    vac_square_sum += pfc_measurement.vac * pfc_measurement.vac;
    ipfc_square_sum += pfc_measurement.ipfc * pfc_measurement.ipfc;
    rms_sample_count++;
    if (rms_sample_count >= PFC_RMS_WINDOW_SAMPLES)
    {
        pfc_measurement.vac_rms = sqrtf(vac_square_sum / (float)PFC_RMS_WINDOW_SAMPLES);
        pfc_measurement.ipfc_rms = sqrtf(ipfc_square_sum / (float)PFC_RMS_WINDOW_SAMPLES);
        vac_square_sum = 0.0f;
        ipfc_square_sum = 0.0f;
        rms_sample_count = 0U;
    }

    /* 允许45~55 Hz；样本边界由实际控制频率计算，便于后续修改控制周期。 */
    minimum_period_samples = (uint32_t)(pfc_params->control_frequency / 55.0f);
    maximum_period_samples = (uint32_t)(pfc_params->control_frequency / 45.0f) + 1U;
    samples_since_cross++;
    if (samples_since_cross > maximum_period_samples)
    {
        pfc_measurement.vac_locked = 0U;
    }
    if (pfc_measurement.vac < -PFC_ZERO_HYSTERESIS_V)
    {
        vac_sign_state = -1;
    }
    else if ((pfc_measurement.vac > PFC_ZERO_HYSTERESIS_V) && (vac_sign_state < 0))
    {
        if ((samples_since_cross >= minimum_period_samples) &&
            (samples_since_cross <= maximum_period_samples))
        {
            pfc_measurement.vac_frequency_hz = pfc_params->control_frequency / (float)samples_since_cross;
            pfc_measurement.vac_zero_cross_sequence++;
            pfc_measurement.vac_locked = 1U;
        }
        else if (pfc_measurement.vac_zero_cross_sequence != 0U)
        {
            pfc_measurement.vac_locked = 0U;
        }
        samples_since_cross = 0U;
        vac_sign_state = 1;
    }

    if ((PFC_Params_IsValid(pfc_params) != 0U) &&
        ((pfc_measurement.ipfc > pfc_params->current_trip) ||
         (pfc_measurement.ipfc < -pfc_params->current_trip)))
    {
        PFC_Measure_Trip(PFC_FAULT_OVERCURRENT);
        return;
    }

    if ((PFC_Params_IsValid(pfc_params) != 0U) &&
        (pfc_measurement.vbus > pfc_params->vbus_overvoltage_trip))
    {
        PFC_Measure_Trip(PFC_FAULT_VBUS_OV);
        return;
    }

    pfc_measurement.fast_heartbeat++;
    pfc_measurement.valid = 1U;
}

/** @brief HAL报告ADC/DMA错误时锁存PFC_FAULT_ADC_ERROR并关闭功率输出。 */
void PFC_Measure_AdcError(void)
{
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
}

/**
 * @brief  锁存一个或多个故障位并执行软件安全关断。
 * @param  fault_bits PFC_FaultMask按位或组合，历史故障不会被后续调用覆盖。
 * @note   可从ISR或主循环调用；关闭PE0和PWM输出，但保留HRTIM计数器继续触发ADC。
 */
void PFC_Measure_Trip(uint32_t fault_bits)
{
    pfc_measurement.fault_bits |= fault_bits;
    pfc_measurement.valid = 0U;
    PFC_HRTIM_StopPower();
}

/**
 * @brief  将ISR发布的完整测量结果复制给调用者。
 * @param  measurement 接收快照的非空地址。
 * @note   临界区保存并恢复PRIMASK，因此主循环和ISR均可调用，不会错误地提前开中断。
 */
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
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/** @brief 返回当前锁存故障位；0表示尚未发生软件故障。 */
uint32_t PFC_Measure_GetFault(void)
{
    return pfc_measurement.fault_bits;
}
