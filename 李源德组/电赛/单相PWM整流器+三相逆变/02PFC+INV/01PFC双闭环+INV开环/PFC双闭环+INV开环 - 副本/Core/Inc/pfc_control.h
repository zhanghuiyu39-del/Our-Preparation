#ifndef PFC_CONTROL_H
#define PFC_CONTROL_H

/* PI+PR双闭环公共契约：10 kHz ISR写控制状态，主循环只读取遥测快照。 */

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "pfc_measure.h"
#include "pfc_params.h"

/** @brief 双闭环控制器的运行阶段，由PFC应用状态机显式切换。 */
typedef enum
{
    PFC_CONTROL_IDLE = 0,       /* 控制器已复位，不写HRTIM Compare。 */
    PFC_CONTROL_CURRENT_RAMP,   /* PR电流环探测，RMS指令按活动参数档斜坡上升。 */
    PFC_CONTROL_VBUS_RAMP,      /* PI外环已投入，VBUS参考斜坡上升。 */
    PFC_CONTROL_VBUS_RUN        /* 活动参数档母线目标附近的稳态双闭环。 */
} PFC_ControlMode;

/** @brief ISR发布给主循环、状态机和VOFA的一致性控制遥测。 */
typedef struct
{
    float vbus_reference;       /* V，当前母线软启动参考。 */
    float vbus_filtered;        /* V，100 Hz陷波后的母线反馈。 */
    float current_rms_command;  /* A RMS，PI输出或电流环探测斜坡值。 */
    float current_reference;    /* A，当前10 kHz瞬时电流参考。 */
    float current_error;        /* A，current_reference-IPFC。 */
    float current_error_rms;    /* A RMS，最近完整工频周期的电流误差。 */
    float pr_output_v;          /* V，PR输出的电感压降校正量。 */
    float modulation;           /* 归一化全桥调制量，范围受参数档限制。 */
    uint32_t fast_heartbeat;    /* 10 kHz控制成功执行次数，允许自然回绕。 */
    uint32_t outer_heartbeat;   /* 1 kHz陷波与PI执行次数，允许自然回绕。 */
    uint32_t fault_bits;        /* 控制模块发现的PFC_FaultMask组合。 */
    uint16_t saturation_count;  /* PR或调制度连续限幅的10 kHz样本数。 */
    PFC_ControlMode mode;       /* 当前控制阶段。 */
    uint8_t pi_limited;         /* 1表示本次1 kHz PI输出触及上下限。 */
    uint8_t pr_limited;         /* 1表示本次10 kHz PR输出触及上下限。 */
    uint8_t modulation_limited; /* 1表示本次调制量被SPWM限幅。 */
    uint8_t current_loop_qualified; /* 1表示电流误差RMS已满足切外环条件。 */
    uint8_t vbus_reference_reached; /* 1表示参考已爬升到最终VBUS目标。 */
    uint8_t healthy;            /* 1表示控制器及最近一次快速路径均正常。 */
} PFC_ControlTelemetry;

/**
 * @brief  初始化PI、PR、100 Hz陷波器、SPWM底层和遥测状态。
 * @param  params 常驻只读参数地址，函数返回后必须继续有效。
 * @retval HAL_OK表示所有控制器及中性Compare初始化成功。
 * @note   在ADC/DMA启动前调用；不启动HRTIM计数器或PWM输出。
 */
HAL_StatusTypeDef PFC_Control_Init(const PFC_Params *params);

/**
 * @brief  切换双闭环阶段，并完成进入该阶段所需的状态预置。
 * @param  mode 目标控制模式。
 * @param  measurement 当前一致性有效测量快照。
 * @retval HAL_OK表示切换成功，HAL_ERROR表示前置条件或参数无效。
 * @note   状态机在1 ms路径调用；函数使用短临界区避免与10 kHz ISR并发修改状态。
 */
HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_Measurement *measurement);

/**
 * @brief  执行一次10 kHz PR内环，并按1/10分频执行陷波器和PI外环。
 * @param  measurement ADC1完整DMA回调刚发布的一致性测量快照。
 * @retval HAL_OK表示本周期控制与Compare写入成功，HAL_ERROR表示必须安全停机。
 * @note   只允许在ADC1 DMA ISR中调用；禁止阻塞、显示、串口和HAL_Delay()。
 */
HAL_StatusTypeDef PFC_Control_Step10k(const PFC_Measurement *measurement);

/**
 * @brief  清除PI/PR、陷波器、分频、RMS统计、斜坡和遥测运行状态。
 * @note   人工停机和任意故障路径调用；不关闭HRTIM输出，调用者必须先关断功率。
 */
void PFC_Control_Reset(void);

/**
 * @brief  在短临界区内复制控制遥测快照。
 * @param  telemetry 接收快照的非空地址。
 * @note   主循环不得直接读取模块内部ISR状态；临界区不会冻结ADC DMA硬件。
 */
void PFC_Control_GetTelemetry(PFC_ControlTelemetry *telemetry);

/** @brief 返回最近一次控制路径健康状态；1为正常，0为禁止继续带功率。 */
uint8_t PFC_Control_IsHealthy(void);

#endif
