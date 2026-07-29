#ifndef SPWM_H
#define SPWM_H

#include <stdint.h>
#include "pfc_params.h"
#include "pfc_measure.h"

/**
 * @brief  初始化VAC同步开环调制器并写入50%中性Compare。
 * @param  params 常驻只读参数地址。
 * @note   初始化阶段调用；不启动HRTIM计数器、不开放PWM输出，也不拉高PE0。
 */
void SPWM_Init(const PFC_Params *params);

/**
 * @brief  用最新测量快照更新工频相位、频率步长和调制输入副本。
 * @param  measurement ADC1完整DMA回调发布的同周期快照。
 * @note   每个10 kHz有效样本调用一次；只更新模块状态，不直接开放功率输出。
 */
void SPWM_SyncUpdate(const PFC_Measurement *measurement);

/**
 * @brief  按电流斜坡和470 uH模型计算桥侧电压，并写Timer A/B Compare 1预装载值。
 * @note   只能在VAC已同步且测量有效时调用；当前是开环前馈，不构成电流闭环或母线稳压。
 */
void SPWM_OpenLoopRampStep(void);

/**
 * @brief  将归一化差分调制量转换为Timer A/B互反Compare。
 * @param  modulation 归一化调制量，内部限制到参数档的正负上限。
 * @retval 实际采用的调制量。
 * @note   只写预装载寄存器，新值在HRTIM配置的周期更新边界生效。
 */
float SPWM_ApplyModulation(float modulation);

/** @brief 返回最近一次实际写入的归一化调制量。 */
float SPWM_GetModulation(void);

/** @brief 返回当前开环输入电流峰值指令，单位A peak。 */
float SPWM_GetCurrentCommand(void);

/** @brief 返回非0表示已由有效VAC正向过零建立工频同步。 */
uint8_t SPWM_IsSynchronized(void);

/** @brief 返回非0表示最近一次同步和调制计算没有非法输入。 */
uint8_t SPWM_IsHealthy(void);

/**
 * @brief  清除相位、斜坡和同步状态，并把Compare恢复到50%中性值。
 * @note   应先拉低PE0并关闭PWM输出，再调用本函数；中性调制不能替代安全停机。
 */
void SPWM_Reset(void);

/** @brief 只写50%中性Compare，不改变HRTIM输出和PE0状态。 */
void SPWM_ForceNeutral(void);

#endif
