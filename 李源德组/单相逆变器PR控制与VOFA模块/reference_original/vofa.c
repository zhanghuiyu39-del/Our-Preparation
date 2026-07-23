#include "vofa.h"
#include "usart.h"

#define VOFA_DECIMATION  5     /* 20kHz / 5 = 4kHz → 50Hz每周期80个点 */
#define VOFA_BUF_DEPTH   8     /* 环形缓冲区深度 */

extern UART_HandleTypeDef huart1;

/* JustFloat 帧尾 */
static const uint8_t tail[4] = {0x00, 0x00, 0x80, 0x7F};

/* ── 环形缓冲区, ISR push / 主循环 pop ────────────── */
static float  vbuf[VOFA_BUF_DEPTH][3];
static volatile uint8_t head = 0;    /* ISR 写入位置 */
static uint8_t          tail_idx = 0;   /* 主循环读取位置 */
static uint16_t         cnt   = 0;

void Vofa_Init(void)
{
    head = 0;
    tail_idx = 0;
    cnt   = 0;
}

/* ── ISR 中调用 ─────────────────────────────────── */
void Vofa_PushISR(float ch0, float ch1, float ch2)
{
    if (++cnt >= VOFA_DECIMATION) {
        cnt = 0;
        vbuf[head][0] = ch0;
        vbuf[head][1] = ch1;
        vbuf[head][2] = ch2;
        uint8_t next = (head + 1) % VOFA_BUF_DEPTH;
        if (next != tail_idx) {   /* 缓冲区未满才推进, 满了丢旧帧 */
            head = next;
        }
    }
}

/* ── 主循环中调用, 将缓冲区中所有待发帧一次性发出 ── */
void Vofa_Send(void)
{
    while (tail_idx != head) {
        HAL_UART_Transmit(&huart1, (uint8_t *)vbuf[tail_idx], 12, 2);
        HAL_UART_Transmit(&huart1, (uint8_t *)tail, 4, 2);
        tail_idx = (tail_idx + 1) % VOFA_BUF_DEPTH;
    }
}
