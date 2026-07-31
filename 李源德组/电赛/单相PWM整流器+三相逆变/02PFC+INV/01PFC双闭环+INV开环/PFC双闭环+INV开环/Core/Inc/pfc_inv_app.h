#ifndef PFC_INV_APP_H
#define PFC_INV_APP_H

/*
 * PFC+INV联合协调层公共接口。
 *
 * 本模块把两套独立应用串成一个安全状态机：PFC先建立并稳定VBUS，
 * 联合阶段再开放INV；任一活动域故障都会关闭A~E全部输出并锁存。
 * 高频ADC回调只调用FastStep接口，PD0、OLED、VOFA和IWDG均由主循环处理。
 */

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "pfc_params.h"
#include "pfc_measure.h"
#include "inv_measure.h"
#include "CBSVPWM.h"

typedef enum
{
    PFC_INV_STATE_SAFE = 0U,
    PFC_INV_STATE_ADC_CALIBRATION,
    PFC_INV_STATE_READY,
    PFC_INV_STATE_PFC_CURRENT_LOOP,
    PFC_INV_STATE_PFC_VBUS_RAMP,
    PFC_INV_STATE_PFC_VBUS_STABLE,
    PFC_INV_STATE_INV_SOFT_START,
    PFC_INV_STATE_JOINT_RUN,
    PFC_INV_STATE_STOP,
    PFC_INV_STATE_FAULT_LATCH
} PFC_INV_State;

typedef enum
{
    PFC_INV_FAULT_NONE       = 0U,
    PFC_INV_FAULT_PFC        = 1U << 0,
    PFC_INV_FAULT_INV        = 1U << 1,
    PFC_INV_FAULT_VBUS_STALE = 1U << 2,
    PFC_INV_FAULT_SEQUENCE   = 1U << 3,
    PFC_INV_FAULT_HRTIM     = 1U << 4,
    PFC_INV_FAULT_SYSTEM     = 1U << 5
} PFC_INV_FaultMask;

typedef struct
{
    PFC_INV_State state;
    uint32_t joint_fault_bits;
    uint32_t pfc_fault_bits;
    uint32_t inv_fault_bits;
    float pfc_vbus;
    float inv_modulation;
    float inv_ramp;
    float inv_duty_u;
    float inv_duty_v;
    float inv_duty_w;
    uint32_t pfc_heartbeat;
    uint32_t inv_heartbeat;
    uint8_t pfc_outputs_enabled;
    uint8_t inv_outputs_enabled;
    uint8_t vbus_stable;
    uint8_t healthy;
} PFC_INV_Telemetry;

/** @brief 初始化PFC、INV测量模块、控制器和联合状态机；不启动DMA或PWM。 */
HAL_StatusTypeDef PFC_INV_AppInit(const PFC_Params *pfc_params,
                                  uint8_t iwdg_reset_seen);

/** @brief 由联合层一次性启动Master及A~E计数器，输出仍保持关闭。 */
HAL_StatusTypeDef PFC_INV_AppStartSharedTimebase(void);

/** @brief 检查五路DMA是否已经产生稳定的新序列，可在启动IWDG前调用。 */
uint8_t PFC_INV_AppSamplingReady(void);

/** @brief ADC1完整DMA回调中的PFC 10 kHz入口。 */
void PFC_INV_AppFastPfcStep(void);

/** @brief ADC3/4/5凑齐一帧后的INV 10 kHz入口。 */
void PFC_INV_AppFastInvStep(const INV_Measurement *measurement);

/** @brief 主循环每1 ms调用，处理PD0和PFC先行、INV后投的状态迁移。 */
void PFC_INV_AppTick1ms(void);

/** @brief 锁存联合故障、关闭A~E并复位两个控制域；故障后禁止自动恢复。 */
void PFC_INV_AppTrip(uint32_t joint_fault_bits,
                     uint32_t pfc_fault_bits,
                     uint32_t inv_fault_bits);

/** @brief 记录PFC域故障；仅当当前阶段需要PFC数据或PFC功率域时升级为联合停机。 */
void PFC_INV_AppReportPfcFault(uint32_t pfc_fault_bits);

/** @brief 记录INV域故障；PFC-only阶段保留诊断但不让未活动INV域阻断PFC试验。 */
void PFC_INV_AppReportInvFault(uint32_t inv_fault_bits);

/** @brief 100 ms监督周期调用一次；返回1才允许刷新IWDG。 */
uint8_t PFC_INV_AppWatchdogHealthy(void);

/** @brief 复制联合遥测快照，供OLED、VOFA和Keil Watch使用。 */
void PFC_INV_AppGetTelemetry(PFC_INV_Telemetry *telemetry);

/** @brief 返回当前联合状态。 */
PFC_INV_State PFC_INV_AppGetState(void);

/** @brief 异常入口使用的A~E寄存器级关断，不调用HAL或Tick。 */
void PFC_INV_AppEmergencyOff(void);

/* 公开运行对象，便于Keil Watch观察CBSVPWM的零序、限幅和占空比。 */
extern volatile CBSVPWM_t pfc_inv_svpwm;
extern volatile PFC_INV_State pfc_inv_state;
extern volatile uint32_t pfc_inv_fault_bits;
extern volatile uint32_t pfc_inv_control_heartbeat;

#endif /* PFC_INV_APP_H */
