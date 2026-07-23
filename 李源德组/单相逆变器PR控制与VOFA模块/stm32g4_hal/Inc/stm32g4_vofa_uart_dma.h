#ifndef STM32G4_VOFA_UART_DMA_H
#define STM32G4_VOFA_UART_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include "vofa_justfloat.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    UART_HandleTypeDef *uart;
    vofa_justfloat_t stream;
} stm32g4_vofa_uart_dma_t;

bool stm32g4_vofa_uart_dma_init(stm32g4_vofa_uart_dma_t *vofa,
                                UART_HandleTypeDef *uart,
                                uint8_t channel_count,
                                uint16_t decimation);
bool stm32g4_vofa_uart_dma_push_isr(stm32g4_vofa_uart_dma_t *vofa,
                                    const float *channels,
                                    uint8_t channel_count);
bool stm32g4_vofa_uart_dma_push3_isr(stm32g4_vofa_uart_dma_t *vofa,
                                     float channel0,
                                     float channel1,
                                     float channel2);
bool stm32g4_vofa_uart_dma_process(stm32g4_vofa_uart_dma_t *vofa);
void stm32g4_vofa_uart_dma_on_tx_complete_isr(
    stm32g4_vofa_uart_dma_t *vofa,
    UART_HandleTypeDef *uart);
void stm32g4_vofa_uart_dma_on_error_isr(stm32g4_vofa_uart_dma_t *vofa,
                                        UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif
