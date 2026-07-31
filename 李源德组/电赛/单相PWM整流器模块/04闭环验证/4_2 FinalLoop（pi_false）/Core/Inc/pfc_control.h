#ifndef PFC_CONTROL_H
#define PFC_CONTROL_H

/*
 * 单相PWM整流器虚拟母线反馈控制接口。
 *
 * ADC关闭时，本模块使用参数档额定频率生成软件相位，不读取VAC、VBUS或IPFC。
 * PI接收pfc_user_config.h中配置的固定母线反馈；PR模块保留在工程中但本模式不调用。
 * 因此该模式只能用于低压、限流的阶段性开环功率验证，不能称为真实母线稳压闭环。
 */

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "pfc_measure.h"
#include "pfc_params.h"

/** @brief 虚拟反馈控制器的运行阶段。 */
typedef enum
{
    PFC_CONTROL_IDLE = 0,       /* 控制器已复位，不更新HRTIM Compare。 */
    PFC_CONTROL_VIRTUAL_RAMP,   /* PI已预置，实际开环电流指令由0缓慢上升。 */
    PFC_CONTROL_VIRTUAL_RUN     /* 电流指令达到PI输出后的持续开环运行。 */
} PFC_ControlMode;

/**
 * @brief ISR发布给主循环、OLED、VOFA和Keil Watch的一致性控制遥测。
 * @note  所有浮点字段均由ADC1完整DMA回调写入，主循环必须通过Getter复制后读取。
 */
typedef struct
{
    float virtual_vbus_reference; /* V，PI参考，默认9 V。 */
    float virtual_vbus_feedback;  /* V，固定PI反馈，不是ADC测得的真实VBUS。 */
    float measured_vbus_monitor;  /* V，仅监测/保护使用的真实母线电压。 */
    float pi_output_current_rms;  /* A RMS，1 kHz PI计算输出。 */
    float applied_current_rms;    /* A RMS，经软启动斜坡后真正送入前馈的幅值。 */
    float synthetic_vac_reference;/* V，按VAC过零相位生成的理想交流电压参考。 */
    float current_reference;      /* A，开环期望输入电流瞬时值，不是实测反馈。 */
    float bridge_voltage_reference;/* V，VAC前馈减去电感压降后的桥侧指令。 */
    float modulation;             /* 归一化全桥调制量，受参数档上限限制。 */
    float pi_error;               /* V，虚拟参考减虚拟反馈；默认应为0。 */
    uint32_t fast_heartbeat;      /* 10 kHz路径成功执行次数，允许自然回绕。 */
    uint32_t outer_heartbeat;     /* 1 kHz PI执行次数，允许自然回绕。 */
    uint32_t fault_bits;          /* 控制模块发现的PFC_FaultMask组合。 */
    uint16_t saturation_count;    /* 连续PI或调制限幅的10 kHz样本数。 */
    PFC_ControlMode mode;         /* 当前控制阶段。 */
    uint8_t pi_limited;           /* 1表示PI输出本次触及上下限。 */
    uint8_t modulation_limited;   /* 1表示SPWM本次触及调制度上限。 */
    uint8_t ramp_complete;        /* 1表示applied_current_rms已经到达PI输出。 */
    uint8_t pr_enabled;           /* 本项目固定为0，用于现场确认PR确实未运行。 */
    uint8_t phase_synchronized;   /* 1表示VAC正向过零已经建立50 Hz同步。 */
    uint8_t healthy;              /* 1表示初始化和最近一次快速路径均正常。 */
} PFC_ControlTelemetry;

/**
 * @brief  初始化PI、虚拟反馈参数和SPWM底层接口。
 * @param  params 常驻只读参数档地址；函数返回后该地址必须继续有效。
 * @retval HAL_OK表示参数和底层接口有效，否则返回HAL_ERROR并记录控制故障。
 * @note   在ADC/DMA启动前调用；本函数不启动HRTIM计数器或PWM输出。
 */
HAL_StatusTypeDef PFC_Control_Init(const PFC_Params *params);

/**
 * @brief  切换虚拟反馈斜坡或持续运行阶段。
 * @param  mode 只允许IDLE、VIRTUAL_RAMP或VIRTUAL_RUN。
 * @param  measurement ADC启用时为一致性快照；ADC关闭时只作为兼容占位输入。
 * @retval HAL_OK表示状态已经完成预置，HAL_ERROR表示输入或控制器状态无效。
 * @note   由1 ms状态机调用；VIRTUAL_RAMP会把PI输出预置到用户设置的RMS电流。
 */
HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_Measurement *measurement);

/**
 * @brief  执行一次10 kHz虚拟反馈PI与开环电感前馈计算。
 * @param  measurement ADC启用时用于过零同步；ADC关闭时不读取其中的采样值。
 * @retval HAL_OK表示Compare更新成功，否则返回HAL_ERROR并要求锁存故障。
 * @note   由ADC1 DMA或HRTIM Master ISR二选一调用；禁止OLED、USART和阻塞操作。
 */
HAL_StatusTypeDef PFC_Control_Step10k(const PFC_Measurement *measurement);

/**
 * @brief  清除PI、相位、斜坡、限幅和遥测运行状态。
 * @note   不负责关闭PWM；调用者必须先执行PFC_HRTIM_StopPower()。
 */
void PFC_Control_Reset(void);

/** @brief 在短临界区内复制完整控制遥测。 */
void PFC_Control_GetTelemetry(PFC_ControlTelemetry *telemetry);

/** @brief 返回最近控制路径健康状态；1为正常，0为禁止继续带功率。 */
uint8_t PFC_Control_IsHealthy(void);

#endif
