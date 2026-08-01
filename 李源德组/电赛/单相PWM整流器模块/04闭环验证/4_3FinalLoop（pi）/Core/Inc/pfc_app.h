#ifndef PFC_APP_H
#define PFC_APP_H

/* PFC状态机公共接口：连接1 ms主循环、10 kHz PI-only模型控制和HRTIM功率许可。 */

#include <stdint.h>
#include "pfc_measure.h"
#include "pfc_params.h"

/** @brief 单相PWM整流器从安全采样到PI-only母线闭环运行的应用状态。 */
typedef enum
{
    PFC_SAFE = 0,            /* 复位后的强制安全等待，PWM输出关闭。 */
    PFC_RAW_ADC_CALIBRATION = 1, /* 原始ADC统计状态，工程量和PD0均禁用。 */
    PFC_ENGINEERING_CHECK = 2,   /* 已标定工程量复核状态，PD0和PWM仍禁用。 */
    PFC_ADC_CHECK = 3,           /* 标定有效后检查同步采样和工程量有效性。 */
    PFC_PASSIVE_PRECHARGE = 4,   /* 等待VAC同步和母线被动建立并连续稳定。 */
    PFC_READY = 5,               /* 启动条件满足，等待PD0完成一次有效短按。 */
    PFC_POWER_RAMP = 6,          /* PWM已开放，模型虚拟电流指令从0缓慢建立。 */
    PFC_VBUS_LOOP_RAMP = 7,      /* PI外环已无扰投入，VBUS参考按参数斜率上升。 */
    PFC_VBUS_LOOP_RUN = 8,       /* VBUS进入目标容差后的稳态PI-only闭环。 */
    PFC_STOP = 9,                /* 人工停机后的短暂等待，采样计数器仍运行。 */
    PFC_FAULT_LATCH = 10,        /* 故障锁存，软件不允许自动恢复。 */
    PFC_PWM_PRIME = 11           /* PD0后等待正向过零并预装载首周期Compare，输出仍关闭。 */
} PFC_State;

/* 兼容旧调试表达式；新代码和文档统一使用PFC_POWER_RAMP。 */
#define PFC_CURRENT_LOOP_RAMP PFC_POWER_RAMP

/** @brief READY启动条件的诊断位；可组合显示，0表示全部实时条件满足。 */
typedef enum
{
    PFC_READY_BLOCK_NONE       = 0U,
    PFC_READY_BLOCK_MODE       = 1U << 0, /* 当前不是已确认标定的闭环模式。 */
    PFC_READY_BLOCK_PARAM      = 1U << 1, /* 参数数值或标定确认无效。 */
    PFC_READY_BLOCK_MEASUREMENT = 1U << 2,/* 工程量快照尚未有效。 */
    PFC_READY_BLOCK_VAC_LOCK   = 1U << 3, /* VAC正向过零同步尚未锁定。 */
    PFC_READY_BLOCK_VAC_RMS    = 1U << 4, /* VAC RMS不在额定值正负20%内。 */
    PFC_READY_BLOCK_VAC_FREQ   = 1U << 5, /* VAC频率超出参数档允许范围。 */
    PFC_READY_BLOCK_VBUS_LOW   = 1U << 6, /* 被动母线低于启动下限。 */
    PFC_READY_BLOCK_VBUS_HIGH  = 1U << 7, /* 母线达到告警值，不允许再次投入PWM。 */
    PFC_READY_BLOCK_FAULT      = 1U << 8  /* 已存在锁存故障。 */
} PFC_ReadyBlockMask;

/**
 * @brief  初始化PFC状态机、PD0消抖和PI-only模型控制器。
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
 * @brief  返回当前不能进入READY或开放PWM的具体原因。
 * @param  measurement 主循环读取的一致性测量快照。
 * @retval PFC_ReadyBlockMask按位组合；0表示实时启动条件全部满足。
 * @note   该接口只诊断，不清故障、不改变状态，也不操作HRTIM输出。
 */
uint16_t PFC_AppGetReadyBlockReason(const PFC_Measurement *measurement);

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
