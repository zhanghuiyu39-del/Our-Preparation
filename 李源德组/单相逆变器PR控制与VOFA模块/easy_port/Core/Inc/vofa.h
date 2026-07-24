#ifndef VOFA_H
#define VOFA_H

#ifdef __cplusplus
extern "C" {
#endif

/* 使用 CubeMX 生成的 main.h 获取 UART_HandleTypeDef 和 HAL 状态定义。 */
#include "main.h"

#include <stdint.h>

/* JustFloat 最多发送 8 个 float 通道，足够覆盖整流与逆变主要调试量。 */
#define VOFA_MAX_CHANNELS (8U)

/* 阻塞发送的最长等待时间，避免调试串口异常时长时间卡住主循环。 */
#define VOFA_TIMEOUT_MS (2U)

/* ==================== VOFA JustFloat 发送 ====================
 * 本函数会阻塞等待串口发送完成，只允许在 while(1) 主循环中调用。
 * data 按通道顺序保存 1~8 个 float，返回值与 HAL_UART_Transmit() 一致。
 * 函数使用内部静态缓冲区，不可在中断中调用，也不可被两个任务同时调用。
 */
HAL_StatusTypeDef VOFA_Send(UART_HandleTypeDef *huart,
                            const float *data,
                            uint8_t channel_count);

#ifdef __cplusplus
}
#endif

#endif /* VOFA_H */
