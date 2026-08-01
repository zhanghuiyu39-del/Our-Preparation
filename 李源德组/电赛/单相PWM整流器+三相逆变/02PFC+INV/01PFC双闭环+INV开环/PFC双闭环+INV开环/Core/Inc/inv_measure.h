#ifndef INV_MEASURE_H
#define INV_MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h"

/*
 * 本模块公开ADC3/4/5循环DMA缓冲区、六通道一致性快照和故障锁存接口。
 * DMA数组只供HAL写入；控制、OLED和VOFA必须读取已发布快照。
 */

extern volatile uint16_t INV_Adc3Dma[2]; /**< Rank1=IU(本次忽略)，Rank2=VUV；DMA写入。 */
extern volatile uint16_t INV_Adc4Dma[2]; /**< Rank1=IV(本次忽略)，Rank2=VVW；DMA写入。 */
extern volatile uint16_t INV_Adc5Dma[2]; /**< Rank1=IW(本次忽略)，Rank2=VWU；DMA写入。 */

/** 可组合锁存的逆变故障位，复位前不自动清除。 */
typedef enum
{
    INV_FAULT_NONE            = 0U,
    INV_FAULT_ADC_SYNC        = 1U << 0, /**< ADC3/4/5序列失步或DMA停止。 */
    INV_FAULT_ADC_ERROR       = 1U << 1, /**< ADC或DMA报告HAL错误。 */
    INV_FAULT_ADC_RANGE       = 1U << 2, /**< 原始码持续贴近电源轨。 */
    INV_FAULT_HRTIM           = 1U << 3, /**< HRTIM启动、更新或FLT3故障。 */
    INV_FAULT_ADC_WATCHDOG    = 1U << 4, /**< ADC3/4/5模拟看门狗越界。 */
    INV_FAULT_PWM_COMMAND     = 1U << 5, /**< 占空比或比较值非法。 */
    INV_FAULT_CBSVPWM         = 1U << 6, /**< CBSVPWM初始化、计算或持续限幅。 */
    INV_FAULT_PARAMETER       = 1U << 7, /**< 开环电压、频率或软启动参数非法。 */
    INV_FAULT_CLOCK           = 1U << 8, /**< CSS检测到HSE时钟故障。 */
    INV_FAULT_CONTROL_TIMEOUT = 1U << 9, /**< 完整帧或快速控制心跳停止。 */
    INV_FAULT_SYSTEM          = 1U << 10 /**< 其他不可恢复HAL/CPU异常。 */
} INV_FaultMask;

/** 测量模块运行方式，由应用工作模式映射，用户不直接修改。 */
typedef enum
{
    INV_MEASURE_RUN_AUTO_OFFSET = 0U, /**< 启动时自动求零点，随后发布物理量。 */
    INV_MEASURE_CALIBRATE_ZERO,       /**< 只计算六路零点，不用于控制。 */
    INV_MEASURE_CALIBRATE_GAIN,       /**< 使用固定零点计算单通道推荐比例。 */
    INV_MEASURE_RUN_FIXED_CALIBRATION /**< 使用用户已确认的固定零点、比例和极性。 */
} INV_MeasureMode;

/** 六路测量模块配置；由main.c从inv_user_config.h构造。 */
typedef struct
{
    INV_MeasureMode mode;
    uint16_t offset_samples;
    uint16_t gain_samples;
    uint16_t minimum_gain_delta_counts;
    uint16_t fixed_offset[6];
    float scale[6];
    /*
     * +1或-1。scale必须是正的A/count或V/count，polarity负责统一规定工程量正方向。
     * 下标顺序固定为IU、IV、IW、VUV、VVW、VWU；该字段只由初始化阶段读取，运行时禁止修改。
     */
    int8_t polarity[6];
    uint8_t calibration_channel;
    float calibration_reference;
    uint8_t max_skew_frames;
    uint16_t rail_low_count;
    uint16_t rail_high_count;
    uint8_t rail_confirm_frames;
} INV_MeasureConfig;

/** ADC标定结果；主循环和VOFA只读该快照。 */
typedef struct
{
    uint16_t offset[6];          /**< ZERO步骤得到的六路平均零点码。 */
    uint16_t raw_average;        /**< GAIN步骤所选通道的平均原始码。 */
    float recommended_scale;     /**< 推荐比例，A/count或V/count，含极性。 */
    float reference_value;       /**< 本次配置的已知物理输入，A或V。 */
    uint16_t collected_samples;  /**< 已累计样本数。 */
    uint8_t channel;             /**< 0～5，对应IU/IV/IW/VUV/VVW/VWU。 */
    uint8_t complete;            /**< 样本采集完成后为1。 */
    uint8_t valid;               /**< 结果通过最小码差和有限数检查后为1。 */
} INV_CalibrationResult;


/** ADC3/4/5一个同步触发周期对应的测量快照；本次INV只使用三路线电压。 */
typedef struct
{
    uint16_t iu_raw;  /**< 保留兼容字段；本版INV不使用U相电流。 */
    uint16_t iv_raw;  /**< 保留兼容字段；本版INV不使用V相电流。 */
    uint16_t iw_raw;  /**< 保留兼容字段；本版INV不使用W相电流。 */
    uint16_t vuv_raw; /**< U-V线电压ADC原始码，范围0～4095。 */
    uint16_t vvw_raw; /**< V-W线电压ADC原始码，范围0～4095。 */
    uint16_t vwu_raw; /**< W-U线电压ADC原始码，范围0～4095。 */

    float iu;  /**< U相电流，A；比例来自用户配置，默认值仍待实测标定。 */
    float iv;  /**< V相电流，A；比例来自用户配置，默认值仍待实测标定。 */
    float iw;  /**< W相电流，A；比例来自用户配置，默认值仍待实测标定。 */
    float vuv; /**< U-V线电压，V；比例来自用户配置，默认值仍待实测标定。 */
    float vvw; /**< V-W线电压，V；比例来自用户配置，默认值仍待实测标定。 */
    float vwu; /**< W-U线电压，V；比例来自用户配置，默认值仍待实测标定。 */

    float current_sum;      /**< IU+IV+IW，单位A，当前只记录不参与关断。 */
    float line_voltage_sum; /**< VUV+VVW+VWU，单位V，当前只记录不参与关断。 */

    uint32_t adc3_sequence;  /**< ADC3 DMA完整序列计数。 */
    uint32_t adc4_sequence;  /**< ADC4 DMA完整序列计数。 */
    uint32_t adc5_sequence;  /**< ADC5 DMA完整序列计数。 */
    uint32_t fast_heartbeat; /**< 每发布一个六通道同步帧增加一次。 */
    uint32_t fault_bits;     /**< INV_FaultMask组合，故障后保持锁存。 */
    uint8_t offset_ready;    /**< 配置帧数的零点平均完成或固定offset载入后为1。 */
    uint8_t valid;           /**< 零点完成且无锁存故障时为1。 */
} INV_Measurement;

/**
 * @brief 校验并复制测量配置，同时清空DMA、序列、标定和故障状态。
 * @param config 配置地址，不能为NULL；采样运行期间不得修改原配置。
 * @retval true 配置有效且初始化完成；false 参数非法，模块保持未初始化。
 * @note 仅在ADC校准及DMA启动前调用；采样运行期间禁止调用。
 */
bool INV_Measure_Init(const INV_MeasureConfig *config);

/**
 * @brief 为ADC3/4/5的Rank2线电压配置运行期模拟看门狗窗口。
 * @param hadc3_handle ADC3句柄，AWD1监视IU、AWD2监视VUV。
 * @param hadc4_handle ADC4句柄，AWD1监视IV、AWD2监视VVW。
 * @param hadc5_handle ADC5句柄，AWD1监视IW、AWD2监视VWU。
 * @retval HAL_OK 三个ADC的三个线电压监视窗口配置成功，或当前处于原始标定模式而保持IOC宽窗口。
 * @retval HAL_ERROR 句柄无效、阈值不合法或HAL配置失败。
 * @note 必须在ADC校准后、HAL_ADC_Start_DMA()前调用；不启动ADC、不清故障，也不开放PWM。
 *       当前窗口是“防贴轨”诊断窗口，不是物理过流阈值；越窗只记录，不关闭INV PWM。
 */
HAL_StatusTypeDef INV_Measure_ConfigureWatchdogs(ADC_HandleTypeDef *hadc3_handle,
                                                  ADC_HandleTypeDef *hadc4_handle,
                                                  ADC_HandleTypeDef *hadc5_handle);

/**
 * @brief 通知对应ADC的一次双Rank规则序列已经由DMA完整写入。
 * @param frame 新同步帧输出地址；只有返回true时内容才代表本次新帧。
 * @retval true 三路ADC均已更新并发布一个新帧；false 尚未凑齐或发生失步。
 * @note 只能在HAL_ADC_ConvCpltCallback()中调用，无阻塞且不操作HRTIM。
 */
bool INV_Measure_OnAdc3Complete(INV_Measurement *frame);
bool INV_Measure_OnAdc4Complete(INV_Measurement *frame);
bool INV_Measure_OnAdc5Complete(INV_Measurement *frame);

/**
 * @brief 锁存一个或多个故障位，并将当前测量快照标记为无效。
 * @param fault_bits INV_FaultMask位组合，0输入不产生变化。
 * @note 允许在主循环和ISR调用；只记录故障，不关闭HRTIM输出。
 */
void INV_Measure_LatchFault(uint32_t fault_bits);

/**
 * @brief 复制最近发布的完整六通道快照。
 * @param measurement 调用者提供的目标地址，不能为NULL。
 * @note 保存并恢复原有PRIMASK；临界区只执行一次结构体复制。
 */
void INV_Measure_GetSnapshot(INV_Measurement *measurement);

/**
 * @brief 复制最近一次ADC零点/增益标定结果。
 * @param result 调用者提供的目标地址，不能为NULL。
 * @note 保存并恢复PRIMASK；可在主循环或调试接口中调用，不操作DMA/HRTIM。
 */
void INV_Measure_GetCalibrationResult(INV_CalibrationResult *result);

/**
 * @brief 返回当前锁存故障位。
 * @retval INV_FaultMask组合；0表示没有软件故障记录。
 */
uint32_t INV_Measure_GetFault(void);

#ifdef __cplusplus
}
#endif

#endif /* INV_MEASURE_H */
