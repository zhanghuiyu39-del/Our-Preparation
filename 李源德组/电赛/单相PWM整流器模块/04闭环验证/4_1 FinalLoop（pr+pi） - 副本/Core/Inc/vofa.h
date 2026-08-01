#ifndef VOFA_H
#define VOFA_H

#ifdef __cplusplus
extern "C" {
#endif

/* 使用CubeMX生成的main.h取得UART句柄类型和HAL状态定义。 */
#include "main.h"

#include <stdint.h>

/* JustFloat最多发送10个float通道，用于追加输入有功功率和软件PF诊断。 */
#define VOFA_MAX_CHANNELS (10U)

/* 阻塞发送最长等待2 ms，避免调试串口异常时长期占用主循环。 */
#define VOFA_TIMEOUT_MS (2U)

/**
 * @brief  按VOFA+ JustFloat格式发送1~10个浮点通道。
 * @param  huart UART句柄，当前工程传入&huart2。
 * @param  data 按通道顺序排列的float数组。
 * @param  channel_count 实际发送的通道数，范围为1~10。
 * @retval HAL_OK表示MCU已完成本帧发送，其他值表示HAL调用失败或超时。
 * @note   本函数使用阻塞式HAL_UART_Transmit()和内部静态缓冲区，
 *         只能从while(1)主循环单点调用，禁止放入ADC/HRTIM中断。
 */
HAL_StatusTypeDef VOFA_Send(UART_HandleTypeDef *huart,
                            const float *data,
                            uint8_t channel_count);

#ifdef __cplusplus
}
#endif

#endif /* VOFA_H */
