#ifndef SPWM_H
#define SPWM_H

/* VAC同步开环前馈调制接口；不负责状态机、输出许可或电流闭环。 */

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "pfc_params.h"
#include "pfc_measure.h"

/**
 * @brief  初始化VAC同步开环调制器并写入50%中性Compare。
 * @param  params 常驻只读参数地址，初始化后必须继续有效。
 * @retval HAL_OK表示初始化和Compare写入成功，否则返回HAL_ERROR。
 * @note   不启动计数器，也不开放PWM输出。
 */
HAL_StatusTypeDef SPWM_Init(const PFC_Params *params);

/**
 * @brief  用最新有效测量更新VAC相位和调制输入快照。
 * @param  measurement 当前ADC1周期的一致性有效快照。
 * @retval HAL_OK表示快照有效，HAL_ERROR表示不能继续调制。
 * @note   每个10 kHz有效ADC1快照调用一次；不开放PWM输出。
 */
HAL_StatusTypeDef SPWM_SyncUpdate(const PFC_Measurement *measurement);

/**
 * @brief  执行一次电流指令爬升、开环电感模型和Compare更新。
 * @retval HAL_OK表示Compare写入成功，HAL_ERROR表示输入或HRTIM写入无效。
 * @note   只允许在ADC1完整DMA回调中调用，禁止阻塞和低速外设访问。
 */
HAL_StatusTypeDef SPWM_OpenLoopRampStep(void);

/**
 * @brief  限幅归一化调制量并写入Timer A/B Compare预装载。
 * @param  modulation 归一化全桥差分调制量，正负方向已包含bridge_polarity。
 * @retval HAL_OK表示Compare写入成功，HAL_ERROR表示数值或HRTIM无效。
 * @note   不改变HRTIM计数器或输出开放状态。
 */
HAL_StatusTypeDef SPWM_ApplyModulation(float modulation);

/**
 * @brief  返回最近写入HRTIM的归一化调制量。
 * @retval 受modulation_limit约束的归一化调制量。
 */
float SPWM_GetModulation(void);

/**
 * @brief  返回当前开环输入电流峰值指令。
 * @retval 电流峰值指令，单位A peak。
 */
float SPWM_GetCurrentCommand(void);

/**
 * @brief  检查VAC同步状态。
 * @retval 1表示过零频率已锁定，0表示尚未同步。
 */
uint8_t SPWM_IsSynchronized(void);

/**
 * @brief  检查最近一次调制路径。
 * @retval 1表示正常，0表示输入或写入失败。
 */
uint8_t SPWM_IsHealthy(void);

/**
 * @brief  检查最近一次调制限幅状态。
 * @retval 1表示触及限幅，0表示未限幅。
 */
uint8_t SPWM_IsLimited(void);

/**
 * @brief  清除相位、斜坡和同步状态，并把Compare恢复到50%中性值。
 * @note   不关闭HRTIM输出，停机调用者必须先执行PFC_HRTIM_StopPower()。
 */
void SPWM_Reset(void);

/**
 * @brief  只写50%中性Compare，不改变HRTIM计数器或输出状态。
 * @retval HAL_OK表示Compare写入成功，否则返回HAL_ERROR。
 */
HAL_StatusTypeDef SPWM_ForceNeutral(void);

#endif
