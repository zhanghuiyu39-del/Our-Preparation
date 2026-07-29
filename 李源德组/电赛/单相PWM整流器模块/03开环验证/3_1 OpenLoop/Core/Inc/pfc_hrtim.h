#ifndef PFC_HRTIM_H
#define PFC_HRTIM_H

#include <stdint.h>
#include "stm32g4xx_hal.h"

/**
 * @brief  启动Master、Timer A和Timer B计数器，为ADC Trigger 1提供10 kHz时基。
 * @retval HAL状态。
 * @note   不开放TA1/TA2/TB1/TB2，也不改变PE0。
 */
HAL_StatusTypeDef PFC_HRTIM_StartSampling(void);

/**
 * @brief  开放单相全桥四路MCU PWM输出。
 * @retval HAL状态。
 * @note   调用前必须已写入安全Compare且计数器稳定；本函数不拉高PE0。
 */
HAL_StatusTypeDef PFC_HRTIM_StartOutputs(void);

/**
 * @brief  拉低PE0并关闭四路PWM输出，保留计数器继续触发ADC。
 * @note   用于人工停机和可诊断的软件故障。
 */
void PFC_HRTIM_StopPower(void);

/**
 * @brief  拉低PE0、关闭PWM并停止Master/Timer A/B计数器。
 * @note   仅用于不可恢复初始化错误；停止后ADC将不再收到HRTIM触发。
 */
void PFC_HRTIM_StopAll(void);

/**
 * @brief  写Timer A/B Compare 1预装载值。
 * @param  cmp_a Timer A Compare 1计数值。
 * @param  cmp_b Timer B Compare 1计数值。
 * @note   不启动计数器、PWM或Gate；调用者负责保证0~Period范围。
 */
void PFC_HRTIM_SetCompare(uint16_t cmp_a, uint16_t cmp_b);

/**
 * @brief  Cortex异常入口使用的寄存器级最小关断路径。
 * @note   不调用HAL、不等待Tick；只拉低PE0并禁止四路HRTIM输出。
 */
void PFC_HRTIM_EmergencyOff(void);

#endif
