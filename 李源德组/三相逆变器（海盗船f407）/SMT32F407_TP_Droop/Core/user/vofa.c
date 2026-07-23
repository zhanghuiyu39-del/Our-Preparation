#include "vofa.h"
#include "usart.h"

#define VOFA_DECIMATION  5
#define VOFA_BUF_DEPTH   8
#define VOFA_FRAME_BYTES (VOFA_CH_COUNT * (uint16_t)sizeof(float))

extern UART_HandleTypeDef huart1;

static const uint8_t tail[4] = {0x00, 0x00, 0x80, 0x7F};

static float vbuf[VOFA_BUF_DEPTH][VOFA_CH_COUNT];
static volatile uint8_t head = 0;
static uint8_t tail_idx = 0;
static uint16_t cnt = 0;

void Vofa_Init(void)
{
    head = 0;
    tail_idx = 0;
    cnt = 0;
}

void Vofa_PushISR(const float *channels)
{
    uint8_t i;

    if (++cnt < VOFA_DECIMATION) {
        return;
    }
    cnt = 0;

    for (i = 0; i < VOFA_CH_COUNT; i++) {
        vbuf[head][i] = channels[i];
    }

    {
        uint8_t next = (head + 1U) % VOFA_BUF_DEPTH;
        if (next != tail_idx) {
            head = next;
        }
    }
}

void Vofa_Send(void)
{
    while (tail_idx != head) {
        HAL_UART_Transmit(&huart1, (uint8_t *)vbuf[tail_idx], VOFA_FRAME_BYTES, 2);
        HAL_UART_Transmit(&huart1, (uint8_t *)tail, 4, 2);
        tail_idx = (tail_idx + 1U) % VOFA_BUF_DEPTH;
    }
}
