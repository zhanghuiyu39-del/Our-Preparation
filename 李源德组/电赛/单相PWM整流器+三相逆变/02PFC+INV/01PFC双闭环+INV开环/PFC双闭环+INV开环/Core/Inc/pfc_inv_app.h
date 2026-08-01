#ifndef PFC_INV_APP_H
#define PFC_INV_APP_H

/*
 * PFC+INV联合协调层公共接口。
 *
 * 本模块协调PFC和INV两个功率域。联合阶段INV不等待PFC母线闭环，
 * INV线电压ADC只用于观察，不参与PWM许可或运行期关断。
 * PFC快速环由ADC1完整DMA回调进入；INV开环由HRTIM Master重复事件进入，
 * ADC3/4/5回调只更新线电压观察快照。PD0/PD1、OLED、VOFA和IWDG均由主循环处理。
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
    uint16_t inv_frequency_hz;
    uint8_t pfc_outputs_enabled;
    uint8_t inv_outputs_enabled;
    uint8_t vbus_stable;
    uint8_t healthy;
} PFC_INV_Telemetry;

/** @brief 初始化PFC、INV测量模块、控制器和联合状态机；不启动DMA或PWM。 */
HAL_StatusTypeDef PFC_INV_AppInit(const PFC_Params *pfc_params,
                                  uint8_t iwdg_reset_seen);

/** @brief 由联合层一次性启动Master及A~E计数器，并使能Master MREP控制中断，输出仍保持关闭。 */
HAL_StatusTypeDef PFC_INV_AppStartSharedTimebase(void);

/** @brief 检查公共时基和PFC采样是否就绪；INV ADC不参与PWM启动许可。 */
uint8_t PFC_INV_AppSamplingReady(void);

/** @brief ADC1完整DMA回调中的PFC 10 kHz入口。 */
void PFC_INV_AppFastPfcStep(void);

/**
 * @brief HRTIM Master重复事件提供的INV 10 kHz开环入口。
 * @note 不读取ADC3/4/5控制量；这些ADC只用于线电压观察，停止或异常不影响C/D/E控制节拍。
 */
void PFC_INV_AppFastInvStep(void);

/** @brief 主循环每1 ms调用，处理PD0启停、PD1频率切换和联合状态迁移。 */
void PFC_INV_AppTick1ms(void);

/** @brief 锁存联合故障、关闭A~E并复位两个控制域；故障后禁止自动恢复。 */
void PFC_INV_AppTrip(uint32_t joint_fault_bits,
                     uint32_t pfc_fault_bits,
                     uint32_t inv_fault_bits);

/** @brief 记录PFC域故障；仅当当前阶段需要PFC数据或PFC功率域时升级为联合停机。 */
void PFC_INV_AppReportPfcFault(uint32_t pfc_fault_bits);

/** @brief 记录INV ADC诊断；不改变联合状态，不关闭C/D/E。 */
void PFC_INV_AppReportInvFault(uint32_t inv_fault_bits);

/** @brief 100 ms监督周期调用一次；返回1才允许刷新IWDG。 */
uint8_t PFC_INV_AppWatchdogHealthy(void);

/** @brief 复制联合遥测快照，供OLED、VOFA和主循环监督使用。 */
void PFC_INV_AppGetTelemetry(PFC_INV_Telemetry *telemetry);

/** @brief 返回当前联合状态。 */
PFC_INV_State PFC_INV_AppGetState(void);

/** @brief 异常入口使用的A~E寄存器级关断，不调用HAL或Tick。 */
void PFC_INV_AppEmergencyOff(void);

#endif /* PFC_INV_APP_H */
