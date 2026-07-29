#ifndef INV_OPEN_LOOP_H
#define INV_OPEN_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * 本模块只负责“输出频率/线电压目标 -> 三相开环电压指令”的参考生成。
 * 它不读取ADC、不计算CBSVPWM，也不操作HRTIM或Gate Enable。
 */

/** 题目要求使用的两种三相输出频率，单位Hz。 */
typedef enum
{
    INV_FREQ_30HZ = 30,
    INV_FREQ_60HZ = 60
} INV_OutputFrequency;

/**
 * @brief 单个10 kHz控制周期生成的三相开环参考快照。
 * @note  由INV_OpenLoop_Step()完整写入；应用层随后把三个电压指令交给CBSVPWM。
 */
typedef struct
{
    float sine_u;              /**< U相单位正弦，范围约-1~1，无量纲。 */
    float sine_v;              /**< V相单位正弦，相对U相滞后120度。 */
    float sine_w;              /**< W相单位正弦，相对U相超前120度。 */
    float vu_command;          /**< U相虚拟相电压指令，单位V。 */
    float vv_command;          /**< V相虚拟相电压指令，单位V。 */
    float vw_command;          /**< W相虚拟相电压指令，单位V。 */
    float modulation;          /**< 已施加软启动后的调制度，无量纲。 */
    float ramp;                /**< 软启动进度，范围0~1。 */
    uint32_t phase_accumulator;/**< U相32位DDS相位，允许自然回绕。 */
    uint16_t frequency_hz;     /**< 当前输出频率，只允许30或60 Hz。 */
    uint8_t valid;             /**< 本周期参考有效时为1。 */
} INV_OpenLoopOutput;

/**
 * @brief 初始化5 V低压开环参考发生器。
 * @param dc_bus_v 软件使用的直流母线电压，单位V，必须大于0。
 * @param line_rms_v 目标三相线电压基波有效值，单位V，必须大于0。
 * @note  默认选择60 Hz和1 s软启动。本函数不访问任何STM32外设。
 */
void INV_OpenLoop_Init(float dc_bus_v, float line_rms_v);

/**
 * @brief 在30 Hz和60 Hz之间切换DDS步进，同时保持当前相位连续。
 * @param frequency 只允许INV_FREQ_30HZ或INV_FREQ_60HZ。
 * @retval true 设置成功；false 参数非法或模块尚未正确初始化。
 * @note  函数不重置软启动，不会直接改变PWM、HRTIM或Gate Enable状态。
 */
bool INV_OpenLoop_SetFrequency(INV_OutputFrequency frequency);

/**
 * @brief 推进一个10 kHz控制周期并生成三相开环电压指令。
 * @param output 调用者提供的输出结构体地址，不能为NULL。
 * @retval true 本周期结果有效；false 参数、初始化状态或输出数值非法。
 * @note  由ADC完整同步帧路径每周期调用一次；无阻塞、无动态内存、允许在ISR中调用。
 */
bool INV_OpenLoop_Step(INV_OpenLoopOutput *output);

/**
 * @brief 复位DDS相位和软启动进度，同时保留电压、频率配置。
 * @note  用于停机或故障锁存；不访问PWM和Gate Enable。
 */
void INV_OpenLoop_Reset(void);

/**
 * @brief 复制最近一次由ISR发布的开环参考快照。
 * @param output 主循环提供的输出地址，不能为NULL。
 * @note  复制期间短暂保持原有PRIMASK状态，OLED/VOFA应通过本接口读取。
 */
void INV_OpenLoop_GetSnapshot(INV_OpenLoopOutput *output);

#ifdef __cplusplus
}
#endif

#endif /* INV_OPEN_LOOP_H */
