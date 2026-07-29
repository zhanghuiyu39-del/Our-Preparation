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
 * @brief 初始化CBSVPWM参数，并将U/V/W/N输出恢复为50%中性占空比。
 * @param svpwm 运行对象地址，不能为NULL。
 * @param modulation_limit 最大桥臂调制度，范围(0, 1]，低压初调使用0.90。
 * @param minimum_dc_voltage 允许归一化计算的最低母线电压，单位V。
 * @retval 1 参数有效且初始化完成；0 参数非法并保持未初始化状态。
 * @note 在PWM输出关闭时调用；只修改软件对象，不访问HAL或HRTIM。
 */
uint8_t CBSVPWM_Init(CBSVPWM_t *svpwm,
                     float modulation_limit,
                     float minimum_dc_voltage);

/**
 * @brief 清除最近计算结果并恢复50%中性占空比，同时保留初始化参数。
 * @param svpwm 已初始化的运行对象地址；NULL输入直接返回。
 * @note 无阻塞，允许在ISR中调用；不写HRTIM，应用层仍需单独关闭PWM。
 */
void CBSVPWM_Reset(CBSVPWM_t *svpwm);

/**
 * @brief 计算三桥臂U/V/W的CBSVPWM占空比。
 * @param svpwm 已初始化的运行对象地址。
 * @param vu_command U相虚拟相电压指令，单位V。
 * @param vv_command V相虚拟相电压指令，单位V。
 * @param vw_command W相虚拟相电压指令，单位V。
 * @param dc_voltage 当前直流母线电压，单位V。
 * @retval 1 计算有效；0 输入非法，此时全部占空比恢复到0.5。
 * @note 无阻塞、无动态内存，可在固定周期ISR调用，但不会自动更新HRTIM。
 */
uint8_t CBSVPWM_Calc3Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage);

/**
 * @brief 保留的四桥臂CBSVPWM计算接口，当前三相三线工程不调用。
 * @param svpwm 已初始化的运行对象地址。
 * @param vu_command U相对目标中性点电压指令，单位V。
 * @param vv_command V相对目标中性点电压指令，单位V。
 * @param vw_command W相对目标中性点电压指令，单位V。
 * @param dc_voltage 当前直流母线电压，单位V。
 * @retval 1 计算有效；0 输入非法并恢复中性占空比。
 * @note 本接口不启用HRTIM F；当前赛题确定为三相三线，只使用Calc3Leg。
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
