#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#ifndef VOFA_JUSTFLOAT_MAX_CHANNELS
#define VOFA_JUSTFLOAT_MAX_CHANNELS 8U
#endif

#ifndef VOFA_JUSTFLOAT_QUEUE_DEPTH
#define VOFA_JUSTFLOAT_QUEUE_DEPTH 8U
#endif

#define VOFA_JUSTFLOAT_MAX_FRAME_BYTES \
    ((VOFA_JUSTFLOAT_MAX_CHANNELS * sizeof(float)) + 4U)

typedef bool (*vofa_justfloat_start_tx_fn)(const uint8_t *data,
                                           uint16_t length,
                                           void *user_context);

typedef struct {
    uint8_t frame[VOFA_JUSTFLOAT_MAX_FRAME_BYTES];
} vofa_justfloat_slot_t;

typedef struct {
    vofa_justfloat_slot_t slots[VOFA_JUSTFLOAT_QUEUE_DEPTH];
    vofa_justfloat_start_tx_fn start_tx;
    void *user_context;
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t tx_busy;
    volatile uint16_t decimation_counter;
    volatile uint32_t dropped_frames;
    volatile uint32_t tx_errors;
    uint8_t channel_count;
    uint16_t decimation;
    uint16_t frame_bytes;
} vofa_justfloat_t;

bool vofa_justfloat_init(vofa_justfloat_t *stream,
                         uint8_t channel_count,
                         uint16_t decimation,
                         vofa_justfloat_start_tx_fn start_tx,
                         void *user_context);
bool vofa_justfloat_push_isr(vofa_justfloat_t *stream,
                             const float *channels,
                             uint8_t channel_count);
bool vofa_justfloat_push3_isr(vofa_justfloat_t *stream,
                              float channel0,
                              float channel1,
                              float channel2);
bool vofa_justfloat_process(vofa_justfloat_t *stream);
void vofa_justfloat_on_tx_complete_isr(vofa_justfloat_t *stream);
void vofa_justfloat_on_tx_error_isr(vofa_justfloat_t *stream);
uint8_t vofa_justfloat_queued(const vofa_justfloat_t *stream);
uint32_t vofa_justfloat_dropped(const vofa_justfloat_t *stream);
uint32_t vofa_justfloat_tx_errors(const vofa_justfloat_t *stream);

#ifdef __cplusplus
}
#endif

#endif
