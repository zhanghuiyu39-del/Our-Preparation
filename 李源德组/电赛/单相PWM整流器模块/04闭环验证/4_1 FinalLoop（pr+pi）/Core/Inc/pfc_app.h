#ifndef PFC_APP_H
#define PFC_APP_H

/* PFC状态机公共接口：连接1 ms主循环、10 kHz双闭环回调和HRTIM功率许可。 */

#include <stdint.h>
#include "pfc_measure.h"
#include "pfc_params.h"

/** @brief 单相PWM整流器从安全采样到PI+PR双闭环运行的应用状态。 */
typedef enum
{
    PFC_SAFE = 0,          /* 复位后的强制安全等待，PWM输出关闭。 */
    PFC_RAW_ADC_CALIBRATION, /* 原始ADC统计状态，工程量和PD0均禁用。 */
    PFC_ENGINEERING_CHECK, /* 已标定工程量复核状态，PD0和PWM仍禁用。 */
    PFC_ADC_CHECK,         /* 标定有效后检查同步采样和工程量有效性。 */
    PFC_PASSIVE_PRECHARGE, /* 等待VAC同步和母线被动建立并连续稳定。 */
    PFC_READY,             /* 启动条件满足，等待PD0完成一次有效短按。 */
    PFC_CURRENT_LOOP_RAMP, /* PWM已开放，PR内环执行0.20 A RMS探测斜坡。 */
    PFC_VBUS_LOOP_RAMP,    /* PI外环已无扰投入，VBUS参考按1 V/s上升。 */
    PFC_VBUS_LOOP_RUN,     /* VBUS进入目标容差后的稳态双闭环。 */
    PFC_STOP,              /* 人工停机后的短暂等待，采样计数器仍运行。 */
    PFC_FAULT_LATCH        /* 故障锁存，软件不允许自动恢复。 */
} PFC_State;

/**
 * @brief  初始化PFC状态机、PD0消抖和PI+PR双闭环控制器。
 * @param  params 常驻只读参数地址。
 * @param  iwdg_reset_seen 非0表示本次启动来自IWDG复位。
 * @note   初始化会强制关闭HRTIM输出，不会开放实际功率驱动。
 */
void PFC_AppInit(const PFC_Params *params, uint8_t iwdg_reset_seen);

/**
 * @brief  ADC1完整DMA回调中的唯一10 kHz控制入口。
 * @note   在DMA ISR中调用，不允许OLED、USART、HAL_Delay()或其他阻塞操作。
 */
void PFC_AppFastStep(void);

/**
 * @brief  处理PD0消抖、启动许可和状态迁移。
 * @note   主循环每1 ms调用一次；PD0只提供运行命令，不承担硬件急停职责。
 */
void PFC_AppTick1ms(void);

/**
 * @brief  锁存故障并关闭HRTIM输出。
 * @param  fault_bits 一个或多个PFC_FaultMask位。
 * @note   可由ISR或主循环调用；故障后只能通过系统复位恢复。
 */
void PFC_AppTrip(uint32_t fault_bits);

/**
 * @brief  返回当前状态，供OLED、VOFA和调试器只读。
 * @retval 当前PFC_State。
 */
PFC_State PFC_AppGetState(void);

/**
 * @brief  检查当前活动参数是否可用于ADC工程量换算。
 * @retval 1表示数值自洽且标定确认，0表示只能使用原始ADC统计。
 */
uint8_t PFC_AppCalibrationValid(void);

/**
 * @brief  检查ADC、控制心跳、状态机和HRTIM软件状态是否一致。
 * @retval 1表示本监督周期允许刷新IWDG，0表示不得刷新。
 * @note   由主循环每100 ms调用一次，每个监督周期只能调用一次。
 */
uint8_t PFC_AppWatchdogHealthy(void);

#endif
