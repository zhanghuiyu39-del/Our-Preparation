#include "vofa.h"

#include <stddef.h>
#include <string.h>

/* 每个float占4字节，整帧末尾再附加4字节JustFloat帧尾。 */
#define VOFA_MAX_FRAME_BYTES ((VOFA_MAX_CHANNELS * 4U) + 4U)

/* JustFloat固定帧尾：在STM32小端存储下对应浮点正无穷。 */
static const uint8_t vofa_tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

/* 使用静态发送缓冲区，不使用动态内存；因此发送函数不可重入。 */
static uint8_t vofa_tx_buffer[VOFA_MAX_FRAME_BYTES];

HAL_StatusTypeDef VOFA_Send(UART_HandleTypeDef *huart,
                            const float *data,
                            uint8_t channel_count)
{
    uint16_t data_bytes;
    uint16_t frame_bytes;

    /* 参数不合法时直接返回，避免访问空指针或写越界。 */
    if ((huart == NULL) || (data == NULL) ||
        (channel_count == 0U) || (channel_count > VOFA_MAX_CHANNELS) ||
        (sizeof(float) != 4U))
    {
        return HAL_ERROR;
    }

    /* 先复制全部float原始字节，再在帧末追加JustFloat识别尾。 */
    data_bytes = (uint16_t)channel_count * (uint16_t)sizeof(float);
    frame_bytes = data_bytes + (uint16_t)sizeof(vofa_tail);
    (void)memcpy(vofa_tx_buffer, data, data_bytes);
    (void)memcpy(&vofa_tx_buffer[data_bytes], vofa_tail, sizeof(vofa_tail));

    /* 10通道共44字节，460800 baud、8N1下约0.96 ms，2 ms超时仍留有余量。 */
    return HAL_UART_Transmit(huart,
                             vofa_tx_buffer,
                             frame_bytes,
                             VOFA_TIMEOUT_MS);
}
