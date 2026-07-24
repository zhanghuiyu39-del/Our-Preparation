#include "vofa.h"

#include <stddef.h>
#include <string.h>

/* 每个 float 占 4 字节，最后再附加 4 字节 JustFloat 帧尾。 */
#define VOFA_MAX_FRAME_BYTES ((VOFA_MAX_CHANNELS * 4U) + 4U)

/* VOFA+ JustFloat 协议固定帧尾，对应小端字节序的正无穷浮点数。 */
static const uint8_t vofa_tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

/* 静态缓冲区避免动态内存；因此 VOFA_Send() 只能由主循环单点调用。 */
static uint8_t vofa_tx_buffer[VOFA_MAX_FRAME_BYTES];

/* ==================== VOFA 整帧阻塞发送 ====================
 * STM32G474 使用小端 IEEE-754 单精度浮点，可直接按字节发送给 VOFA+。
 */
HAL_StatusTypeDef VOFA_Send(UART_HandleTypeDef *huart,
                            const float *data,
                            uint8_t channel_count)
{
    uint16_t data_bytes;
    uint16_t frame_bytes;

    /* 检查句柄、数据、通道数和 float 大小，参数错误时不访问串口。 */
    if ((huart == NULL) || (data == NULL) ||
        (channel_count == 0U) || (channel_count > VOFA_MAX_CHANNELS) ||
        (sizeof(float) != 4U)) {
        return HAL_ERROR;
    }

    /* 先复制全部通道，再紧接着追加 JustFloat 的 4 字节帧尾。 */
    data_bytes = (uint16_t)channel_count * (uint16_t)sizeof(float);
    frame_bytes = data_bytes + (uint16_t)sizeof(vofa_tail);
    (void)memcpy(vofa_tx_buffer, data, data_bytes);
    (void)memcpy(&vofa_tx_buffer[data_bytes], vofa_tail, sizeof(vofa_tail));

    /* 只在主循环发送；2 ms 超时可避免 USB-TTL 断开时拖住后台任务。 */
    return HAL_UART_Transmit(huart,
                             vofa_tx_buffer,
                             frame_bytes,
                             VOFA_TIMEOUT_MS);
}
