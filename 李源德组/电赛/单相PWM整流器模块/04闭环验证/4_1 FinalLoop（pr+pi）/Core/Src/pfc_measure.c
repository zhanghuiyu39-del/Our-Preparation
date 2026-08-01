#include "pfc_measure.h"
#include "pfc_hrtim.h"
#include <math.h>

/*
 * 本模块负责“ADC原始值 -> 同周期快照 -> 标定工程量与工频统计”，不负责状态机和调制。
 * 数据流：HRTIM TRG1 -> ADC2/VAC先完成 -> ADC1/IPFC、VBUS完成 -> 一致性快照。
 *
 * PFC_Adc1Dma/PFC_Adc2Dma由DMA写，ADC回调读取；pfc_measurement只由ADC回调写，
 * 主循环和控制模块必须通过PFC_Measure_GetSnapshot()读取。标定未确认时仍发布原始码、
 * 序列号、心跳和1秒统计，但不计算占位工程量，也不允许状态机开放功率输出。
 */
#define PFC_ADC_RAW_MAX                 4095U /* 12位ADC最大原始码。 */
#define PFC_ADC_BIPOLAR_RAIL_LOW        4U    /* 低于4码才视为接近低端，给36 V采样峰值留出更多正常范围。 */
#define PFC_ADC_BIPOLAR_RAIL_HIGH       4091U /* 高于4091码才视为接近高端，短时噪声不会立即关断。 */
#define PFC_ADC_VBUS_RAIL_HIGH          4091U /* VBUS单极性通道只检查高端贴轨。 */
#define PFC_ADC_RAIL_CONFIRM_SAMPLES    64U   /* 必须连续64帧贴轨，10 kHz下约6.4 ms才锁存范围故障。 */
#define PFC_ZERO_HYSTERESIS_V           0.20f /* V，VAC过零检测迟滞半宽。 */
#define PFC_MIN_RMS_WINDOW_SAMPLES      32U   /* 异常参数下RMS窗口的安全下限。 */
#define PFC_MIN_APPARENT_POWER_VA       0.10f /* VA，低于该值时PF会被零点噪声放大，故标记无效。 */

/*
 * DMA直接写入的12位右对齐原始数组，Rank顺序必须与CubeMX一致：
 * ADC1[IPFC,VBUS]，ADC2[VAC]。数组由DMA写、ADC完整回调读；volatile只保证每次
 * 实际访问内存，不保证跨数组同步。主循环禁止直接读取，必须取得已发布快照。
 */
volatile uint16_t PFC_Adc1Dma[2] = {0U, 0U};
volatile uint16_t PFC_Adc2Dma[1] = {0U};

/*
 * ADC1完整回调发布的模块内快照。ISR写、主循环和控制模块通过临界区复制读取；
 * 结构体整体不是天然原子对象，因此不得在模块外直接访问。
 */
static volatile PFC_Measurement pfc_measurement = {0};
static const PFC_Params *pfc_params = 0;      /* 初始化后只读，参数对象必须常驻。 */
static volatile uint32_t adc2_sequence = 0U; /* ADC2 DMA ISR写，ADC1 DMA ISR读。 */
static uint32_t last_adc2_sequence = 0U;      /* ADC1 ISR上次接受的ADC2序列号。 */

/* VAC正向过零和RMS统计状态，只由ADC1完整回调以10 kHz更新。 */
static uint32_t samples_since_cross = 0U; /* 距上次候选正向过零的样本数。 */
static uint32_t rms_window_samples = 0U;  /* 一个标称工频周期对应的控制样本数。 */
static float vac_square_sum = 0.0f;       /* V^2样本累加和。 */
static float ipfc_square_sum = 0.0f;      /* A^2样本累加和。 */
static float input_power_sum = 0.0f;      /* W样本累加和，逐点累计VAC*IPFC。 */
static uint32_t rms_sample_count = 0U;    /* 当前RMS窗口已累计的样本数。 */
static int8_t vac_sign_state = -1;        /* -1表示已到负半周，+1表示正向过零已处理。 */

/* 连续异常确认计数，只由ADC1完整回调写；达到阈值后锁存故障。 */
static uint8_t sync_miss_count = 0U;      /* ADC2未在ADC1前发布新序列的连续次数。 */
static uint8_t bipolar_rail_count = 0U;   /* IPFC或VAC双极性前端贴轨连续帧数。 */
static uint8_t vbus_high_rail_count = 0U; /* VBUS高端贴轨连续帧数。 */

/*
 * 原始ADC统计由ADC1完整DMA ISR独占更新，主循环通过Getter读取已发布结果。
 * 10 kHz、1秒窗口时每路最大累加值4095*10000，小于uint32_t上限。
 */
static volatile PFC_AdcCalibrationStats pfc_calibration_stats = {0};
static uint32_t stats_window_samples = 10000U; /* 一秒对应的同步ADC帧数。 */
static uint32_t stats_sample_count = 0U;
static uint32_t stats_ipfc_sum = 0U;
static uint32_t stats_vac_sum = 0U;
static uint32_t stats_vbus_sum = 0U;
static uint16_t stats_ipfc_min = PFC_ADC_RAW_MAX;
static uint16_t stats_ipfc_max = 0U;
static uint16_t stats_vac_min = PFC_ADC_RAW_MAX;
static uint16_t stats_vac_max = 0U;
static uint16_t stats_vbus_min = PFC_ADC_RAW_MAX;
static uint16_t stats_vbus_max = 0U;

/**
 * @brief  把本周期同步原始码计入统计，并在满一秒时发布完整结果。
 * @note   只由ADC1完整DMA ISR调用；不执行浮点运算、显示、通信或阻塞操作。
 */
static void PFC_Measure_UpdateCalibrationStats(void)
{
    uint16_t ipfc = pfc_measurement.ipfc_raw;
    uint16_t vac = pfc_measurement.vac_raw;
    uint16_t vbus = pfc_measurement.vbus_raw;

    stats_ipfc_sum += ipfc;
    stats_vac_sum += vac;
    stats_vbus_sum += vbus;
    if (ipfc < stats_ipfc_min) { stats_ipfc_min = ipfc; }
    if (ipfc > stats_ipfc_max) { stats_ipfc_max = ipfc; }
    if (vac < stats_vac_min) { stats_vac_min = vac; }
    if (vac > stats_vac_max) { stats_vac_max = vac; }
    if (vbus < stats_vbus_min) { stats_vbus_min = vbus; }
    if (vbus > stats_vbus_max) { stats_vbus_max = vbus; }
    stats_sample_count++;

    if (stats_sample_count >= stats_window_samples)
    {
        pfc_calibration_stats.ipfc_latest = ipfc;
        pfc_calibration_stats.ipfc_mean = (uint16_t)(stats_ipfc_sum / stats_sample_count);
        pfc_calibration_stats.ipfc_min = stats_ipfc_min;
        pfc_calibration_stats.ipfc_max = stats_ipfc_max;
        pfc_calibration_stats.vac_latest = vac;
        pfc_calibration_stats.vac_mean = (uint16_t)(stats_vac_sum / stats_sample_count);
        pfc_calibration_stats.vac_min = stats_vac_min;
        pfc_calibration_stats.vac_max = stats_vac_max;
        pfc_calibration_stats.vbus_latest = vbus;
        pfc_calibration_stats.vbus_mean = (uint16_t)(stats_vbus_sum / stats_sample_count);
        pfc_calibration_stats.vbus_min = stats_vbus_min;
        pfc_calibration_stats.vbus_max = stats_vbus_max;
        pfc_calibration_stats.sample_count = stats_sample_count;
        pfc_calibration_stats.window_sequence++;
        pfc_calibration_stats.valid = 1U;

        stats_sample_count = 0U;
        stats_ipfc_sum = 0U;
        stats_vac_sum = 0U;
        stats_vbus_sum = 0U;
        stats_ipfc_min = PFC_ADC_RAW_MAX;
        stats_ipfc_max = 0U;
        stats_vac_min = PFC_ADC_RAW_MAX;
        stats_vac_max = 0U;
        stats_vbus_min = PFC_ADC_RAW_MAX;
        stats_vbus_max = 0U;
    }
}

#if PFC_USER_RELAXED_PWM_TEST == 0U
/**
 * @brief  将浮点阈值四舍五入并约束到12位ADC原始码范围。
 * @param  code 根据工程阈值和标定比例计算出的ADC码。
 * @retval 0~4095范围内的整数ADC码。
 */
static uint16_t PFC_Measure_ClampAdcCode(float code)
{
    if (code <= 0.0f)
    {
        return 0U;
    }
    if (code >= (float)PFC_ADC_RAW_MAX)
    {
        return PFC_ADC_RAW_MAX;
    }
    return (uint16_t)(code + 0.5f);
}

/**
 * @brief  填充HAL结构并配置一个ADC模拟看门狗。
 * @param  hadc 目标ADC句柄。
 * @param  watchdog HAL模拟看门狗编号枚举。
 * @param  mode HAL监视组/通道模式枚举。
 * @param  channel 被监视频道的HAL枚举。
 * @param  low 低阈值ADC码，必须小于high。
 * @param  high 高阈值ADC码，最大4095。
 * @retval HAL_OK表示配置成功，HAL_ERROR表示参数或HAL配置失败。
 * @note   只允许在ADC校准后、规则组DMA启动前调用；本函数启用越窗中断。
 */
static HAL_StatusTypeDef PFC_Measure_ConfigAwd(ADC_HandleTypeDef *hadc,
                                               uint32_t watchdog,
                                               uint32_t mode,
                                               uint32_t channel,
                                               uint16_t low,
                                               uint16_t high)
{
    ADC_AnalogWDGConfTypeDef config = {0};

    if ((hadc == 0) || (low >= high) || (high > PFC_ADC_RAW_MAX))
    {
        return HAL_ERROR;
    }
    config.WatchdogNumber = watchdog;
    config.WatchdogMode = mode;
    config.Channel = channel;
    config.ITMode = ENABLE;
    config.HighThreshold = high;
    config.LowThreshold = low;
    config.FilteringConfig = ADC_AWD_FILTERING_NONE;
    return HAL_ADC_AnalogWDGConfig(hadc, &config);
}
#endif

/**
 * @brief  初始化DMA缓冲区、同步序列和测量统计状态。
 * @param  params 常驻只读参数地址，函数返回后仍必须保持有效。
 * @note   在HAL_ADC_Start_DMA()之前调用；不校准ADC、不启动DMA或PWM。
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
    input_power_sum = 0.0f;
    rms_sample_count = 0U;
    vac_sign_state = -1;
    sync_miss_count = 0U;
    bipolar_rail_count = 0U;
    vbus_high_rail_count = 0U;
    pfc_calibration_stats = (PFC_AdcCalibrationStats){0};
    stats_sample_count = 0U;
    stats_ipfc_sum = 0U;
    stats_vac_sum = 0U;
    stats_vbus_sum = 0U;
    stats_ipfc_min = PFC_ADC_RAW_MAX;
    stats_ipfc_max = 0U;
    stats_vac_min = PFC_ADC_RAW_MAX;
    stats_vac_max = 0U;
    stats_vbus_min = PFC_ADC_RAW_MAX;
    stats_vbus_max = 0U;

    if ((params != 0) && (params->grid_frequency_hz > 0.0f))
    {
        rms_window_samples = (uint32_t)(params->control_frequency /
                                        params->grid_frequency_hz + 0.5f);
    }
    else
    {
        rms_window_samples = 0U;
    }
    if (rms_window_samples < PFC_MIN_RMS_WINDOW_SAMPLES)
    {
        rms_window_samples = PFC_MIN_RMS_WINDOW_SAMPLES;
    }

    if ((params != 0) && (params->control_frequency >= 1.0f))
    {
        stats_window_samples = (uint32_t)(params->control_frequency + 0.5f);
    }
    else
    {
        stats_window_samples = 10000U;
    }

    pfc_measurement.offset_ready = PFC_Params_CalibrationReady(params);
}

/**
 * @brief  根据已确认的标定参数配置IPFC、VBUS和VAC运行期模拟看门狗窗口。
 * @param  hadc1_handle ADC1句柄，AWD1监视IPFC、AWD2监视VBUS。
 * @param  hadc2_handle ADC2句柄，AWD1监视VAC。
 * @retval HAL_OK表示配置成功；标定无效时保持CubeMX初始窗口并同样返回HAL_OK。
 * @note   只能在ADC校准后、HAL_ADC_Start_DMA()前调用；不会启动ADC或DMA。
 */
HAL_StatusTypeDef PFC_Measure_ConfigureWatchdogs(ADC_HandleTypeDef *hadc1_handle,
                                                 ADC_HandleTypeDef *hadc2_handle)
{
#if PFC_USER_RELAXED_PWM_TEST != 0U
    /* 波形验证模式不改写CubeMX宽窗口；句柄检查仍用于发现初始化调用错误。 */
    return ((hadc1_handle != 0) && (hadc2_handle != 0)) ? HAL_OK : HAL_ERROR;
#else
    /* 三个通道最终使用的12位ADC码阈值。 */
    uint16_t ipfc_low;
    uint16_t ipfc_high;
    uint16_t vbus_high;
    uint16_t vac_low;
    uint16_t vac_high;
    float ipfc_span; /* count，过流阈值相对IPFC零点的码差。 */
    float vac_span;  /* count，VAC峰值阈值相对VAC零点的码差。 */

    if ((hadc1_handle == 0) || (hadc2_handle == 0))
    {
        return HAL_ERROR;
    }

    /* 标定无效时保留CubeMX生成的宽窗口，避免占位比例产生虚假的保护阈值。 */
    if (PFC_Params_CalibrationReady(pfc_params) == 0U)
    {
        return HAL_OK;
    }

    ipfc_span = pfc_params->current_trip / pfc_params->ipfc_amp_per_count;
    ipfc_low = PFC_Measure_ClampAdcCode((float)pfc_params->ipfc_zero_count - ipfc_span);
    ipfc_high = PFC_Measure_ClampAdcCode((float)pfc_params->ipfc_zero_count + ipfc_span);
    vbus_high = PFC_Measure_ClampAdcCode(pfc_params->vbus_overvoltage_trip /
                                         pfc_params->vbus_volt_per_count);

    /* AWD2仅比较12位结果的高8位；向下对齐16码使过压门槛不会被悄悄抬高。 */
    vbus_high = (uint16_t)(vbus_high & 0x0FF0U);
    if (vbus_high < 16U)
    {
        return HAL_ERROR;
    }

    vac_span = pfc_params->vac_peak_trip / pfc_params->vac_volt_per_count;
    vac_low = PFC_Measure_ClampAdcCode((float)pfc_params->vac_zero_count - vac_span);
    vac_high = PFC_Measure_ClampAdcCode((float)pfc_params->vac_zero_count + vac_span);

    /*
     * 当前36 V/100 ohm闭环档以验证控制效果为优先，不把ADC1 AWD1从CubeMX生成的
     * 0~4095宽窗口收紧。这样PWM尚未开放时，4000 uF母线经体二极管自然充电产生的
     * IPFC浪涌不会在软件层锁存F=01010。其他参数档仍保留原来的运行期IPFC AWD窗口。
     * 100 ohm档在PWM开放后仍由下方4.00 A峰值软件判断执行过流关断。
     */
    if ((pfc_params->profile_id != PFC_PROFILE_36V_LIGHT_100R) &&
        (PFC_Measure_ConfigAwd(hadc1_handle, ADC_ANALOGWATCHDOG_1,
                               ADC_ANALOGWATCHDOG_SINGLE_REG, ADC_CHANNEL_2,
                               ipfc_low, ipfc_high) != HAL_OK))
    {
        return HAL_ERROR;
    }
    if (PFC_Measure_ConfigAwd(hadc1_handle, ADC_ANALOGWATCHDOG_2,
                              ADC_ANALOGWATCHDOG_SINGLE_REGINJEC, ADC_CHANNEL_7,
                              0U, vbus_high) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return PFC_Measure_ConfigAwd(hadc2_handle, ADC_ANALOGWATCHDOG_1,
                                 ADC_ANALOGWATCHDOG_SINGLE_REG, ADC_CHANNEL_6,
                                 vac_low, vac_high);
#endif
}

/**
 * @brief  发布ADC2完整DMA序列号，表示VAC原始数组已由DMA更新。
 * @note   由ADC2 DMA ISR以10 kHz调用；不复制VAC、不换算工程量、不执行控制。
 */
void PFC_Measure_OnAdc2Complete(void)
{
    adc2_sequence++;
}

/**
 * @brief  核对ADC2序列并发布一次IPFC/VBUS/VAC一致性快照。
 * @note   由ADC1 DMA完整回调以10 kHz调用；禁止OLED、USART和HAL_Delay()。
 */
void PFC_Measure_OnAdc1Complete(void)
{
    uint32_t current_adc2_sequence = adc2_sequence;
    uint32_t adc2_advance = current_adc2_sequence - last_adc2_sequence;
    uint32_t minimum_period_samples; /* 允许的最高工频对应的最少周期样本数。 */
    uint32_t maximum_period_samples; /* 允许的最低工频对应的最多周期样本数。 */
    int32_t ipfc_count;              /* count，去除IPFC零点后的有符号码差。 */
    int32_t vac_count;               /* count，去除VAC零点后的有符号码差。 */

    if (adc2_advance == 0U)
    {
        if (++sync_miss_count >= 3U)
        {
            PFC_Measure_Trip(PFC_FAULT_ADC_SYNC);
        }
        return;
    }
    if (adc2_advance > 2U)
    {
        PFC_Measure_Trip(PFC_FAULT_ADC_SYNC);
        return;
    }

    sync_miss_count = 0U;
    last_adc2_sequence = current_adc2_sequence;
    pfc_measurement.ipfc_raw = PFC_Adc1Dma[0];
    pfc_measurement.vbus_raw = PFC_Adc1Dma[1];
    pfc_measurement.vac_raw = PFC_Adc2Dma[0];
    pfc_measurement.adc1_sequence++;
    pfc_measurement.adc2_sequence = current_adc2_sequence;
    pfc_measurement.fast_heartbeat++;
    pfc_measurement.raw_valid = 1U;
    PFC_Measure_UpdateCalibrationStats();

    /* 未完成板级标定时只发布原始值，不能使用占位系数进行控制或物理阈值判断。 */
    if (PFC_Params_CalibrationReady(pfc_params) == 0U)
    {
        pfc_measurement.offset_ready = 0U;
        pfc_measurement.valid = 0U;
        return;
    }

    pfc_measurement.offset_ready = 1U;
    if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
        ((pfc_measurement.ipfc_raw < PFC_ADC_BIPOLAR_RAIL_LOW) ||
        (pfc_measurement.ipfc_raw > PFC_ADC_BIPOLAR_RAIL_HIGH) ||
        (pfc_measurement.vac_raw < PFC_ADC_BIPOLAR_RAIL_LOW) ||
        (pfc_measurement.vac_raw > PFC_ADC_BIPOLAR_RAIL_HIGH)))
    {
        if (bipolar_rail_count < PFC_ADC_RAIL_CONFIRM_SAMPLES)
        {
            bipolar_rail_count++;
        }
    }
    else
    {
        bipolar_rail_count = 0U;
    }
    if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
        (pfc_measurement.vbus_raw > PFC_ADC_VBUS_RAIL_HIGH))
    {
        if (vbus_high_rail_count < PFC_ADC_RAIL_CONFIRM_SAMPLES)
        {
            vbus_high_rail_count++;
        }
    }
    else
    {
        vbus_high_rail_count = 0U;
    }
    if ((bipolar_rail_count >= PFC_ADC_RAIL_CONFIRM_SAMPLES) ||
        (vbus_high_rail_count >= PFC_ADC_RAIL_CONFIRM_SAMPLES))
    {
        PFC_Measure_Trip(PFC_FAULT_ADC_RANGE);
        return;
    }

    ipfc_count = (int32_t)pfc_measurement.ipfc_raw - (int32_t)pfc_params->ipfc_zero_count;
    vac_count = (int32_t)pfc_measurement.vac_raw - (int32_t)pfc_params->vac_zero_count;
    pfc_measurement.ipfc = (float)(ipfc_count * pfc_params->ipfc_polarity) *
                           pfc_params->ipfc_amp_per_count;
    pfc_measurement.vac = (float)(vac_count * pfc_params->vac_polarity) *
                          pfc_params->vac_volt_per_count;
    pfc_measurement.vbus = (float)pfc_measurement.vbus_raw *
                           pfc_params->vbus_volt_per_count;

    vac_square_sum += pfc_measurement.vac * pfc_measurement.vac;
    ipfc_square_sum += pfc_measurement.ipfc * pfc_measurement.ipfc;
    input_power_sum += pfc_measurement.vac * pfc_measurement.ipfc;
    rms_sample_count++;
    if (rms_sample_count >= rms_window_samples)
    {
        pfc_measurement.vac_rms = sqrtf(vac_square_sum / (float)rms_sample_count);
        pfc_measurement.ipfc_rms = sqrtf(ipfc_square_sum / (float)rms_sample_count);
        pfc_measurement.input_active_power_w = input_power_sum /
                                               (float)rms_sample_count;
        pfc_measurement.input_apparent_power_va = pfc_measurement.vac_rms *
                                                   pfc_measurement.ipfc_rms;
        if (pfc_measurement.input_apparent_power_va >= PFC_MIN_APPARENT_POWER_VA)
        {
            float power_factor = pfc_measurement.input_active_power_w /
                                 pfc_measurement.input_apparent_power_va;
            if (power_factor > 1.0f) { power_factor = 1.0f; }
            if (power_factor < -1.0f) { power_factor = -1.0f; }
            pfc_measurement.input_power_factor = power_factor;
            pfc_measurement.input_power_valid = 1U;
        }
        else
        {
            pfc_measurement.input_power_factor = 0.0f;
            pfc_measurement.input_power_valid = 0U;
        }
        vac_square_sum = 0.0f;
        ipfc_square_sum = 0.0f;
        input_power_sum = 0.0f;
        rms_sample_count = 0U;
    }

    minimum_period_samples = (uint32_t)(pfc_params->control_frequency /
        (pfc_params->grid_frequency_hz + pfc_params->grid_frequency_tolerance));
    maximum_period_samples = (uint32_t)(pfc_params->control_frequency /
        (pfc_params->grid_frequency_hz - pfc_params->grid_frequency_tolerance)) + 1U;
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
            pfc_measurement.vac_frequency_hz = pfc_params->control_frequency /
                                               (float)samples_since_cross;
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

    /*
     * 未按PD0时HRTIM输出关闭，此时输入电流只可能来自自然整流充电或采样瞬态，
     * 不再把它当成闭环软件过流。只有四路PWM已经由状态机开放后，才按活动参数档
     * 的current_trip判断并锁存故障，避免无PWM阶段被动浪涌反复进入FAULT_LATCH。
     */
    if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
        (PFC_HRTIM_OutputsEnabled() != 0U) &&
        (fabsf(pfc_measurement.ipfc) > pfc_params->current_trip))
    {
        PFC_Measure_Trip(PFC_FAULT_OVERCURRENT);
        return;
    }
    if ((PFC_USER_RELAXED_PWM_TEST == 0U) &&
        (pfc_measurement.vbus > pfc_params->vbus_overvoltage_trip))
    {
        PFC_Measure_Trip(PFC_FAULT_VBUS_OV);
        return;
    }
    pfc_measurement.valid = 1U;
}

/**
 * @brief  将HAL ADC或DMA错误转换为ADC锁存故障。
 * @note   可在ADC/DMA错误ISR中调用，只执行故障记录和非阻塞HRTIM关断。
 */
void PFC_Measure_AdcError(void)
{
    PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
}

/**
 * @brief  将HAL模拟看门狗来源映射为可诊断的PFC故障位。
 * @param  source IPFC、VBUS或VAC硬件看门狗来源。
 * @note   可在ADC1_2 ISR中调用；IPFC来源在PWM关闭时被忽略，VBUS/VAC来源仍始终锁存。
 */
void PFC_Measure_AnalogWatchdog(PFC_AwdSource source)
{
#if PFC_USER_RELAXED_PWM_TEST != 0U
    /* 测试开关打开时忽略全部AWD中断，不写故障位，也不关闭HRTIM输出。 */
    (void)source;
#else
    switch (source)
    {
    case PFC_AWD_SOURCE_IPFC:
        /* 被动充电阶段不把IPFC AWD事件升级为故障；PWM开放后才执行原有锁存关断。 */
        if (PFC_HRTIM_OutputsEnabled() != 0U)
        {
            PFC_Measure_Trip(PFC_FAULT_AWD_IPFC | PFC_FAULT_OVERCURRENT);
        }
        break;
    case PFC_AWD_SOURCE_VBUS:
        PFC_Measure_Trip(PFC_FAULT_AWD_VBUS | PFC_FAULT_VBUS_OV);
        break;
    case PFC_AWD_SOURCE_VAC:
        PFC_Measure_Trip(PFC_FAULT_AWD_VAC | PFC_FAULT_VAC_LOST);
        break;
    default:
        PFC_Measure_Trip(PFC_FAULT_ADC_ERROR);
        break;
    }
#endif
}

/**
 * @brief  按位锁存故障、撤销工程量有效标志并紧急关闭HRTIM输出。
 * @param  fault_bits 一个或多个PFC_FaultMask位；已有故障位不会被清除。
 * @note   可由ISR或主循环调用；不停止采样计数器，不允许在此执行显示或通信。
 */
void PFC_Measure_Trip(uint32_t fault_bits)
{
    pfc_measurement.fault_bits |= fault_bits;
    pfc_measurement.valid = 0U;
    PFC_HRTIM_EmergencyOff();
}

/**
 * @brief  在短临界区内复制一份完整、前后一致的测量快照。
 * @param  measurement 接收快照的非空地址；返回后由调用者独占该副本。
 * @note   临界区只保护pfc_measurement结构体复制，不会冻结DMA原始数组；函数保留
 *         调用前的PRIMASK状态，既可由主循环调用，也可由当前ADC ISR调用。
 */
void PFC_Measure_GetSnapshot(PFC_Measurement *measurement)
{
    uint32_t primask; /* 调用前全局中断屏蔽状态，用于退出时原样恢复。 */

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

void PFC_Measure_GetCalibrationStats(PFC_AdcCalibrationStats *stats)
{
    uint32_t primask;

    if (stats == 0)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    *stats = pfc_calibration_stats;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/**
 * @brief  读取当前锁存故障位。
 * @retval PFC_FaultMask按位组合；0表示尚未记录故障。
 * @note   32位对齐读取在Cortex-M4上为单次访问，本接口不会清除故障。
 */
uint32_t PFC_Measure_GetFault(void)
{
    return pfc_measurement.fault_bits;
}
