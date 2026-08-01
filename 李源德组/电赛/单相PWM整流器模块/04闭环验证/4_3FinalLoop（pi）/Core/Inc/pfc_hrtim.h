#ifndef PFC_HRTIM_H
#define PFC_HRTIM_H

/* HRTIM A/B应用封装：区分计数器时基、MCU PWM输出和紧急关断。 */

#include <stdint.h>
#include "stm32g4xx_hal.h"

/* 与当前IOC的Timer A/B Period保持一致；修改HRTIM时必须同步核对。 */
#define PFC_HRTIM_PERIOD_COUNTS       34000U
#define PFC_HRTIM_HALF_PERIOD_COUNTS  17000U

/**
 * @brief  启动Master、Timer A和Timer B计数器，建立10 kHz ADC触发时基。
 * @retval HAL状态。
 * @note   不开放TA1/TA2/TB1/TB2引脚；计数器运行与PWM输出开放是两个状态。
 */
HAL_StatusTypeDef PFC_HRTIM_StartSampling(void);

/**
 * @brief  开放单相全桥四路MCU PWM输出。
 * @retval HAL状态。
 * @note   调用前必须已启动计数器并写入安全Compare；驱动板通电时会实际驱动功率管。
 */
HAL_StatusTypeDef PFC_HRTIM_StartOutputs(void);

/**
 * @brief  关闭四路PWM输出并保留HRTIM计数器继续触发ADC。
 * @note   用于人工停机和可诊断软件故障；本工程不存在MCU Gate Enable GPIO。
 */
void PFC_HRTIM_StopPower(void);

/**
 * @brief  关闭四路PWM输出并停止Master、Timer A和Timer B计数器。
 * @note   用于不可恢复初始化错误；停止后ADC不再收到HRTIM触发。
 */
void PFC_HRTIM_StopAll(void);

/**
 * @brief  写Timer A/B Compare 1预装载值。
 * @param  cmp_a Timer A Compare 1计数值，范围1~33999。
 * @param  cmp_b Timer B Compare 1计数值，范围1~33999。
 * @retval HAL_OK表示两个Compare已写入，HAL_ERROR表示句柄或范围无效。
 * @note   Compare已启用预装载，将在IOC配置的HRTIM更新边界生效；本函数不开放输出。
 */
HAL_StatusTypeDef PFC_HRTIM_SetCompare(uint16_t cmp_a, uint16_t cmp_b);

/**
 * @brief  立即把Timer A/B的Compare预装载值同步提交到活动寄存器。
 * @retval HAL_OK表示已发出A/B软件更新，HAL_BUSY/HAL_ERROR表示本次提交失败。
 * @note   只提交Compare，不启动计数器、不开放TA1/TA2/TB1/TB2；用于过零投入前
 *         消除“预装载已写但首周期仍使用旧Compare”的时间空窗。
 */
HAL_StatusTypeDef PFC_HRTIM_CommitCompare(void);

/**
 * @brief  返回Master/A/B计数器启动命令的软件镜像。
 * @retval 1表示本模块已启动且尚未停止，0表示未启动或已停止。
 * @note   不是HRTIM状态寄存器的实时回读。
 */
uint8_t PFC_HRTIM_CountersStarted(void);

/**
 * @brief  返回TA1/TA2/TB1/TB2开放命令的软件镜像。
 * @retval 1表示软件已开放输出，0表示软件已关闭输出。
 * @note   不能据此证明引脚有波形或功率管导通；HRTIM Fault可异步关闭硬件输出。
 */
uint8_t PFC_HRTIM_OutputsEnabled(void);

/**
 * @brief  Cortex异常入口使用的寄存器级最小关断路径。
 * @note   不调用HAL、不等待Tick，只禁止四路HRTIM输出；可在NMI/HardFault中调用。
 */
void PFC_HRTIM_EmergencyOff(void);

#endif
