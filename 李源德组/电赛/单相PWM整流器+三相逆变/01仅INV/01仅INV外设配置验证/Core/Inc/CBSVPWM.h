#ifndef CBSVPWM_H
#define CBSVPWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * 本头文件公开CBSVPWM运行状态和三/四桥臂计算接口。
 * 模块只输出无量纲占空比，不直接依赖STM32 HAL或具体PWM外设。
 */

/*
 * 载波型空间矢量PWM运行对象。
 *
 * 结构体公开是为了便于在Keil Watch中观察归一化过程、零序注入量和最终占空比。
 * 应用代码只设置初始化参数，不应在运行期间直接修改中间量和输出字段。
 */
typedef struct {
    /* 用户配置：调制度为无量纲，母线电压单位为V。 */
    float modulation_limit;
    float minimum_dc_voltage;

    /* 输入电压除以Vdc/2后的三相归一化指令。 */
    float normalized_u;
    float normalized_v;
    float normalized_w;

    /* 仿真模型中的三相平均值、最大最小值零序注入量和整体限幅比例。 */
    float average;
    float zero_sequence;
    float applied_scale;

    /* 完成零序注入和整体比例限幅后的桥臂调制量，范围受modulation_limit约束。 */
    float modulation_u;
    float modulation_v;
    float modulation_w;
    float modulation_n;

    /* 输出占空比，供应用层映射到HRTIM、TIM或其他PWM外设。 */
    float duty_u;
    float duty_v;
    float duty_w;
    float duty_n;

    /* initialized表示参数有效；valid表示最近一次计算有效；limited表示发生整体限幅。 */
    uint8_t initialized;
    uint8_t valid;
    uint8_t limited;
} CBSVPWM_t;

/**
 * 初始化CBSVPWM参数并把四路输出恢复到50%中性占空比。
 * modulation_limit：允许的桥臂最大绝对调制度，范围为(0, 1]，推荐从0.90f开始。
 * minimum_dc_voltage：允许执行归一化计算的最低母线电压，单位V，必须大于0。
 * 返回值：参数有效返回1，否则返回0并保持未初始化状态。
 * 调用位置：外设初始化完成、功率输出和Gate Enable仍关闭时调用；允许在ISR外调用。
 * 副作用：只修改svpwm对象，不访问HAL、PWM寄存器或门极使能。
 */
uint8_t CBSVPWM_Init(CBSVPWM_t *svpwm,
                     float modulation_limit,
                     float minimum_dc_voltage);

/**
 * 清除最近一次计算结果并恢复50%中性占空比，同时保留初始化参数。
 * 调用位置：停机、Fault锁存、重新闭环或软启动开始前；函数无阻塞，允许在ISR中调用。
 * 副作用：不写入HRTIM，应用层仍需单独关闭PWM输出和Gate Enable。
 */
void CBSVPWM_Reset(CBSVPWM_t *svpwm);

/**
 * 计算三桥臂U/V/W的CBSVPWM占空比。
 * vu/vv/vw_command：QPR或开环参考给出的三相电压指令，单位V。
 * dc_voltage：本控制周期的直流母线电压，单位V。
 * 返回值：计算成功返回1；空指针、未初始化、非有限输入或母线欠压返回0。
 * 调用位置：可在固定周期ADC/HRTIM控制ISR中调用；函数不阻塞、不分配内存、不访问HAL。
 * 失败行为：U/V/W/N占空比全部恢复为0.5，不能据此自动开放Gate Enable。
 */
uint8_t CBSVPWM_Calc3Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage);

/**
 * 计算四桥臂U/V/W/N的CBSVPWM占空比，N桥臂关系与目标Simulink模型的Gn一致。
 * vu/vv/vw_command：相对于目标负载中性点的三相电压指令，单位V。
 * dc_voltage：本控制周期的直流母线电压，单位V。
 * 返回值、ISR约束和失败行为与CBSVPWM_Calc3Leg()相同。
 * 注意：调用本函数不代表HRTIM F已经配置或允许启动，中性桥臂硬件必须单独验收。
 */
uint8_t CBSVPWM_Calc4Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage);

#ifdef __cplusplus
}
#endif

#endif /* CBSVPWM_H */
