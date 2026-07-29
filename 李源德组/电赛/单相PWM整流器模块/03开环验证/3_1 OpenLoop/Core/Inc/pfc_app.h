#ifndef PFC_APP_H
#define PFC_APP_H

#include <stdint.h>
#include "pfc_measure.h"
#include "pfc_params.h"

/* 状态编号同时发送到OLED/VOFA；修改顺序时同步更新上位机名称。 */
typedef enum
{
    PFC_SAFE = 0,          /* 复位后的强制安全等待，PE0和PWM均关闭。 */
    PFC_CALIBRATION,       /* 输出原始ADC；参数未确认时停留在此状态。 */
    PFC_ADC_CHECK,         /* HRTIM只产生ADC触发，验证DMA连续性。 */
    PFC_PASSIVE_PRECHARGE, /* 等待体二极管预充和VAC锁定。 */
    PFC_READY,             /* 启动条件成立，等待PD0短按。 */
    PFC_OPEN_LOOP_RAMP,    /* Gate开启，电流指令按斜坡增加。 */
    PFC_OPEN_LOOP_RUN,     /* 开环指令达到目标或母线达到目标。 */
    PFC_STOP,              /* 人工停机后的100 ms过渡。 */
    PFC_FAULT_LATCH        /* 故障锁存，只允许复位/重新上电恢复。 */
} PFC_State;

/**
 * @brief  初始化状态机并强制功率输出进入安全态。
 * @param  params 常驻参数地址。
 * @param  iwdg_reset_seen 非0表示检测到IWDG复位原因。
 * @note   主程序初始化阶段调用，不启动PWM输出或Gate Enable。
 */
void PFC_AppInit(const PFC_Params *params, uint8_t iwdg_reset_seen);

/** @brief ADC1完整DMA回调中的唯一10 kHz控制入口，不允许阻塞。 */
void PFC_AppFastStep(void);

/** @brief 主循环每1 ms调用一次，处理PD0和状态迁移。 */
void PFC_AppTick1ms(void);

/** @brief 锁存故障并关断功率输出，可由ISR或主循环调用。 */
void PFC_AppTrip(uint32_t fault_bits);

/** @brief 返回当前PFC_State，供显示和遥测只读。 */
PFC_State PFC_AppGetState(void);

/** @brief 每100 ms执行一次安全监督，返回非0时才允许刷新IWDG。 */
uint8_t PFC_AppWatchdogHealthy(void);

#endif
