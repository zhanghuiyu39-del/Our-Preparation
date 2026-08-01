#ifndef INV_OPEN_LOOP_H
#define INV_OPEN_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * 本模块只负责“运行参数 -> 三相开环相电压指令”的参考生成。
 * 它不读取ADC、不计算CBSVPWM，也不操作HRTIM或任何板级GPIO。
 */

/** 赛题逆变部分使用的两种输出频率，单位Hz。 */
typedef enum
{
    INV_FREQ_30HZ = 30,
    INV_FREQ_60HZ = 60
} INV_OutputFrequency;

/**
 * @brief 三相三线开环运行参数。
 * @note 当前5 V验证和后续赛题参数都从该结构体进入，避免在算法内散布常量。
 */
typedef struct
{
    float dc_bus_v;                 /**< 直流母线电压，单位V，当前开环使用软件给定值。 */
    float line_rms_target_v;        /**< 三相输出目标线电压基波有效值，单位Vrms。 */
    INV_OutputFrequency frequency;  /**< 输出频率，只允许30 Hz或60 Hz。 */
    float modulation_limit;         /**< 最大桥臂调制度，范围(0, 1]。 */
    uint32_t soft_start_ms;         /**< 从零幅值升到目标值的时间，单位ms，必须大于0。 */
    uint32_t control_frequency_hz;  /**< INV_OpenLoop_Step调用频率，单位Hz。 */
} INV_OpenLoopConfig;

/** 单个控制周期生成的三相参考快照。 */
typedef struct
{
    float sine_u;               /**< U相单位正弦，范围约-1～1。 */
    float sine_v;               /**< V相单位正弦，相对U相滞后120度。 */
    float sine_w;               /**< W相单位正弦，相对U相超前120度。 */
    float vu_command;           /**< U相虚拟相电压指令，单位V。 */
    float vv_command;           /**< V相虚拟相电压指令，单位V。 */
    float vw_command;           /**< W相虚拟相电压指令，单位V。 */
    float dc_bus_v;             /**< 本周期CBSVPWM应使用的母线电压，单位V。 */
    float modulation;           /**< 已施加软启动的目标调制度，无量纲。 */
    float ramp;                 /**< 软启动进度，范围0～1。 */
    uint32_t phase_accumulator; /**< U相32位DDS相位，允许自然回绕。 */
    uint16_t frequency_hz;      /**< 当前输出频率，单位Hz。 */
    uint8_t valid;              /**< 本周期全部参考量有效时为1。 */
} INV_OpenLoopOutput;

/**
 * @brief 根据配置对象初始化DDS、电压指令和软启动参数。
 * @param config 只读配置地址，各字段必须满足INV_OpenLoopConfig约束。
 * @retval true 参数有效且初始化完成；false 参数不可实现或地址为空。
 * @note 必须在PWM输出关闭时调用；函数不访问STM32外设。
 */
bool INV_OpenLoop_Init(const INV_OpenLoopConfig *config);

/**
 * @brief 在30 Hz和60 Hz之间切换DDS步进，并保持当前相位连续。
 * @param frequency 只允许INV_FREQ_30HZ或INV_FREQ_60HZ。
 * @retval true 设置成功；false 参数非法或模块尚未初始化。
 * @note 函数不重置相位和软启动。运行期调用者必须自行保证参数交换一致性。
 */
bool INV_OpenLoop_SetFrequency(INV_OutputFrequency frequency);

/**
 * @brief 读取当前DDS输出频率。
 * @param frequency 接收当前频率的地址，只能在非空时写入。
 * @retval true 模块已初始化且读取成功；false 地址为空或模块未初始化。
 * @note 可由1 ms主循环调用；读取期间使用短临界区，不能在此函数内阻塞。
 */
bool INV_OpenLoop_GetFrequency(INV_OutputFrequency *frequency);

/**
 * @brief 更新开环使用的母线电压和目标线电压。
 * @param dc_bus_v 当前直流母线电压，单位V，必须大于0。
 * @param line_rms_v 目标线电压基波有效值，单位Vrms，必须大于0。
 * @retval true 新目标在调制度限制内并已生效；false 参数非法或目标不可实现。
 * @note 只允许在PWM关闭的SAFE/READY状态调用；本函数不会自动重启软启动。
 */
bool INV_OpenLoop_SetVoltageCommand(float dc_bus_v, float line_rms_v);

/**
 * @brief 推进一个控制周期并生成U/V/W三相开环相电压指令。
 * @param output 调用者提供的输出地址，不能为NULL。
 * @retval true 本周期结果有效；false 模块状态或浮点结果非法。
 * @note 由ADC完整同步帧每周期调用一次；无阻塞、无动态内存、允许在ISR中调用。
 */
bool INV_OpenLoop_Step(INV_OpenLoopOutput *output);

/**
 * @brief 复位DDS相位和软启动进度，同时保留电压及频率配置。
 * @note 用于停机或故障锁存；不访问PWM和GPIO。
 */
void INV_OpenLoop_Reset(void);

/**
 * @brief 复制最近一次由ISR发布的开环参考快照。
 * @param output 主循环提供的输出地址，不能为NULL。
 * @note 复制期间保存并恢复原有PRIMASK，OLED和VOFA应通过本接口读取。
 */
void INV_OpenLoop_GetSnapshot(INV_OpenLoopOutput *output);

#ifdef __cplusplus
}
#endif

#endif /* INV_OPEN_LOOP_H */
