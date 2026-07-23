#include "stm32g4_vofa_uart_dma.h"

#include <stddef.h>

static bool stm32g4_vofa_start_tx(const uint8_t *data,
                                  uint16_t length,
                                  void *user_context)
{
    stm32g4_vofa_uart_dma_t *vofa = (stm32g4_vofa_uart_dma_t *)user_context;

    if ((vofa == NULL) || (vofa->uart == NULL)) {
        return false;
    }
    return HAL_UART_Transmit_DMA(vofa->uart,
                                 (uint8_t *)data,
                                 length) == HAL_OK;
}

bool stm32g4_vofa_uart_dma_init(stm32g4_vofa_uart_dma_t *vofa,
                                UART_HandleTypeDef *uart,
                                uint8_t channel_count,
                                uint16_t decimation)
{
    if ((vofa == NULL) || (uart == NULL)) {
        return false;
    }

    vofa->uart = uart;
    return vofa_justfloat_init(&vofa->stream,
                               channel_count,
                               decimation,
                               stm32g4_vofa_start_tx,
                               vofa);
}

bool stm32g4_vofa_uart_dma_push_isr(stm32g4_vofa_uart_dma_t *vofa,
                                    const float *channels,
                                    uint8_t channel_count)
{
    return (vofa == NULL) ? false
                          : vofa_justfloat_push_isr(&vofa->stream,
                                                    channels,
                                                    channel_count);
}

bool stm32g4_vofa_uart_dma_push3_isr(stm32g4_vofa_uart_dma_t *vofa,
                                     float channel0,
                                     float channel1,
                                     float channel2)
{
    return (vofa == NULL) ? false
                          : vofa_justfloat_push3_isr(&vofa->stream,
                                                     channel0,
                                                     channel1,
                                                     channel2);
}

bool stm32g4_vofa_uart_dma_process(stm32g4_vofa_uart_dma_t *vofa)
{
    return (vofa == NULL) ? false : vofa_justfloat_process(&vofa->stream);
}

void stm32g4_vofa_uart_dma_on_tx_complete_isr(
    stm32g4_vofa_uart_dma_t *vofa,
    UART_HandleTypeDef *uart)
{
    if ((vofa != NULL) && (uart == vofa->uart)) {
        vofa_justfloat_on_tx_complete_isr(&vofa->stream);
    }
}

void stm32g4_vofa_uart_dma_on_error_isr(stm32g4_vofa_uart_dma_t *vofa,
                                        UART_HandleTypeDef *uart)
{
    if ((vofa != NULL) && (uart == vofa->uart)) {
        vofa_justfloat_on_tx_error_isr(&vofa->stream);
    }
}
