#ifndef INV_MEASURE_H
#define INV_MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* DMA原始数组只允许HAL/DMA写入；控制、OLED和VOFA必须读取一致性快照。 */
extern volatile uint16_t INV_Adc3Dma[2]; /**< Rank1=IU，Rank2=VUV。 */
extern volatile uint16_t INV_Adc4Dma[2]; /**< Rank1=IV，Rank2=VVW。 */
extern volatile uint16_t INV_Adc5Dma[2]; /**< Rank1=IW，Rank2=VWU。 */

/** 真正改变运行状态并关闭PWM的锁存故障位。 */
typedef enum
{
    INV_FAULT_NONE   = 0U,
    INV_FAULT_FLT3   = 1U << 0, /**< PB10/HRTIM Fault3已经触发硬件关断。 */
    INV_FAULT_CLOCK  = 1U << 1, /**< CSS发现HSE失效。 */
    INV_FAULT_SYSTEM = 1U << 2  /**< CPU异常或不可恢复初始化错误。 */
} INV_FaultMask;

/** 默认放宽档只记录、不关闭PWM的诊断位。 */
typedef enum
{
    INV_DIAG_NONE            = 0U,
    INV_DIAG_ADC_SYNC        = 1U << 0, /**< ADC3/4/5序列偏差或DMA停滞。 */
    INV_DIAG_ADC_ERROR       = 1U << 1, /**< ADC/DMA报告HAL错误。 */
    INV_DIAG_ADC_RANGE       = 1U << 2, /**< ADC原始码持续贴轨。 */
    INV_DIAG_ADC_WATCHDOG    = 1U << 3, /**< ADC模拟看门狗越窗。 */
    INV_DIAG_PR_INVALID      = 1U << 4, /**< PR输入或计算无效，已回退前馈。 */
    INV_DIAG_SVPWM           = 1U << 5, /**< CBSVPWM计算无效。 */
    INV_DIAG_PWM_COMMAND     = 1U << 6, /**< HRTIM比较值写入失败。 */
    INV_DIAG_LIMITED         = 1U << 7, /**< PR或CBSVPWM发生限幅。 */
    INV_DIAG_CONTROL_TIMEOUT = 1U << 8, /**< 100ms窗口内控制入口未推进。 */
    INV_DIAG_PARAMETER       = 1U << 9  /**< 用户参数或标定配置无效。 */
} INV_DiagnosticMask;

typedef enum
{
    INV_MEASURE_RUN_AUTO_OFFSET = 0U, /**< 启动时对六路零输入求平均。 */
    INV_MEASURE_CALIBRATE_ZERO,       /**< 只执行ZERO标定，不用于控制。 */
    INV_MEASURE_CALIBRATE_GAIN        /**< 使用固定offset计算单通道scale。 */
} INV_MeasureMode;

/** 测量模块配置，由main.c从inv_user_config.h构造。 */
typedef struct
{
    INV_MeasureMode mode;
    uint16_t offset_samples;
    uint16_t gain_samples;
    uint16_t minimum_gain_delta_counts;
    uint16_t fixed_offset[6];
    float scale[6];
    uint8_t calibration_channel;
    float calibration_reference;
    uint8_t max_skew_frames;
    uint16_t rail_low_count;
    uint16_t rail_high_count;
    uint8_t rail_confirm_frames;
} INV_MeasureConfig;

/** ADC零点或增益标定结果，只能通过快照接口读取。 */
typedef struct
{
    uint16_t offset[6];
    uint16_t raw_average;
    float recommended_scale;
    float reference_value;
    uint16_t collected_samples;
    uint8_t channel;
    uint8_t complete;
    uint8_t valid;
} INV_CalibrationResult;

/** 一个HRTIM Trigger2周期对应的六通道测量快照。 */
typedef struct
{
    uint16_t iu_raw;
    uint16_t iv_raw;
    uint16_t iw_raw;
    uint16_t vuv_raw;
    uint16_t vvw_raw;
    uint16_t vwu_raw;

    float iu;  /**< U相电流，单位A。 */
    float iv;  /**< V相电流，单位A。 */
    float iw;  /**< W相电流，单位A。 */
    float vuv; /**< U-V线电压，单位V。 */
    float vvw; /**< V-W线电压，单位V。 */
    float vwu; /**< W-U线电压，单位V。 */
    float current_sum;      /**< IU+IV+IW，单位A，仅诊断。 */
    float line_voltage_sum; /**< VUV+VVW+VWU，单位V，仅诊断。 */

    uint32_t adc3_sequence;
    uint32_t adc4_sequence;
    uint32_t adc5_sequence;
    uint32_t fast_heartbeat;
    uint32_t diagnostic_bits; /**< INV_DiagnosticMask，默认不使快照失效。 */
    uint32_t fault_bits;      /**< INV_FaultMask，FLT3等真正锁存故障。 */
    uint8_t offset_ready;
    uint8_t valid;            /**< 工程量有限、offset就绪且无真正锁存故障。 */
} INV_Measurement;

/** @brief 校验配置并清空DMA、序列、标定和诊断状态。 */
bool INV_Measure_Init(const INV_MeasureConfig *config);

/**
 * @brief 通知对应ADC一次双Rank DMA完整传输。
 * @param frame 返回新的一致性快照；只有返回true时有效。
 * @retval true 三个ADC均产生一个尚未使用的新序列；false 尚未凑齐。
 * @note 只能在HAL_ADC_ConvCpltCallback()调用，不操作HRTIM。
 */
bool INV_Measure_OnAdc3Complete(INV_Measurement *frame);
bool INV_Measure_OnAdc4Complete(INV_Measurement *frame);
bool INV_Measure_OnAdc5Complete(INV_Measurement *frame);

/** @brief 锁存真正故障并使测量快照失效；关PWM由应用层负责。 */
void INV_Measure_LatchFault(uint32_t fault_bits);

/** @brief 锁存调试诊断，不改变测量valid和PWM状态。 */
void INV_Measure_LatchDiagnostic(uint32_t diagnostic_bits);

/** @brief 复制最近发布的完整六通道测量快照。 */
void INV_Measure_GetSnapshot(INV_Measurement *measurement);

/** @brief 复制最近一次ZERO/GAIN标定结果。 */
void INV_Measure_GetCalibrationResult(INV_CalibrationResult *result);

/** @brief 返回真正锁存故障位。 */
uint32_t INV_Measure_GetFault(void);

/** @brief 返回累计诊断位。 */
uint32_t INV_Measure_GetDiagnostic(void);

#ifdef __cplusplus
}
#endif

#endif /* INV_MEASURE_H */
