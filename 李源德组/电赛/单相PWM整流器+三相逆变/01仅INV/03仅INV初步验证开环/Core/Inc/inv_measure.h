#ifndef INV_MEASURE_H
#define INV_MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * 本模块公开ADC3/4/5循环DMA缓冲区、六通道一致性快照和故障锁存接口。
 * DMA数组只供HAL写入；控制、OLED和VOFA必须读取已发布快照。
 */

extern volatile uint16_t INV_Adc3Dma[2]; /**< Rank1=IU，Rank2=VUV；DMA写入。 */
extern volatile uint16_t INV_Adc4Dma[2]; /**< Rank1=IV，Rank2=VVW；DMA写入。 */
extern volatile uint16_t INV_Adc5Dma[2]; /**< Rank1=IW，Rank2=VWU；DMA写入。 */

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

/** ADC3/4/5一个同步触发周期对应的六通道测量快照。 */
typedef struct
{
    uint16_t iu_raw;  /**< U相电流ADC原始码，范围0～4095。 */
    uint16_t iv_raw;  /**< V相电流ADC原始码，范围0～4095。 */
    uint16_t iw_raw;  /**< W相电流ADC原始码，范围0～4095。 */
    uint16_t vuv_raw; /**< U-V线电压ADC原始码，范围0～4095。 */
    uint16_t vvw_raw; /**< V-W线电压ADC原始码，范围0～4095。 */
    uint16_t vwu_raw; /**< W-U线电压ADC原始码，范围0～4095。 */

    float iu;  /**< U相电流，单位A；当前比例为低压测试占位值。 */
    float iv;  /**< V相电流，单位A；当前比例为低压测试占位值。 */
    float iw;  /**< W相电流，单位A；当前比例为低压测试占位值。 */
    float vuv; /**< U-V线电压，单位V；当前比例为低压测试占位值。 */
    float vvw; /**< V-W线电压，单位V；当前比例为低压测试占位值。 */
    float vwu; /**< W-U线电压，单位V；当前比例为低压测试占位值。 */

    float current_sum;      /**< IU+IV+IW，单位A，当前只记录不参与关断。 */
    float line_voltage_sum; /**< VUV+VVW+VWU，单位V，当前只记录不参与关断。 */

    uint32_t adc3_sequence;  /**< ADC3 DMA完整序列计数。 */
    uint32_t adc4_sequence;  /**< ADC4 DMA完整序列计数。 */
    uint32_t adc5_sequence;  /**< ADC5 DMA完整序列计数。 */
    uint32_t fast_heartbeat; /**< 每发布一个六通道同步帧增加一次。 */
    uint32_t fault_bits;     /**< INV_FaultMask组合，故障后保持锁存。 */
    uint8_t offset_ready;    /**< 256帧零点平均完成后为1。 */
    uint8_t valid;           /**< 零点完成且无锁存故障时为1。 */
} INV_Measurement;

/**
 * @brief 清空DMA缓冲、序列、零点累加器、快照和锁存故障。
 * @note 仅在ADC校准及DMA启动前调用；采样运行期间禁止调用。
 */
void INV_Measure_Init(void);

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
 * @brief 返回当前锁存故障位。
 * @retval INV_FaultMask组合；0表示没有软件故障记录。
 */
uint32_t INV_Measure_GetFault(void);

#ifdef __cplusplus
}
#endif

#endif /* INV_MEASURE_H */
