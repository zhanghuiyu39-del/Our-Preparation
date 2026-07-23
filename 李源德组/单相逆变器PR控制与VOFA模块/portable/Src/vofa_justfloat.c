#include "vofa_justfloat.h"

#include <stddef.h>
#include <string.h>

static const uint8_t vofa_tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

static uint8_t vofa_next_index(uint8_t index)
{
    index++;
    if (index >= VOFA_JUSTFLOAT_QUEUE_DEPTH) {
        index = 0U;
    }
    return index;
}

bool vofa_justfloat_init(vofa_justfloat_t *stream,
                         uint8_t channel_count,
                         uint16_t decimation,
                         vofa_justfloat_start_tx_fn start_tx,
                         void *user_context)
{
    const uint16_t endian_test = 1U;

    if ((stream == NULL) || (start_tx == NULL) ||
        (channel_count == 0U) ||
        (channel_count > VOFA_JUSTFLOAT_MAX_CHANNELS) ||
        (decimation == 0U) || (sizeof(float) != 4U) ||
        (*((const uint8_t *)&endian_test) != 1U) ||
        (VOFA_JUSTFLOAT_QUEUE_DEPTH < 2U)) {
        return false;
    }

    (void)memset(stream, 0, sizeof(*stream));
    stream->start_tx = start_tx;
    stream->user_context = user_context;
    stream->channel_count = channel_count;
    stream->decimation = decimation;
    stream->frame_bytes = (uint16_t)(channel_count * sizeof(float) +
                                     sizeof(vofa_tail));
    return true;
}

bool vofa_justfloat_push_isr(vofa_justfloat_t *stream,
                             const float *channels,
                             uint8_t channel_count)
{
    uint8_t next;
    uint8_t index;

    if ((stream == NULL) || (channels == NULL) ||
        (channel_count != stream->channel_count)) {
        return false;
    }

    stream->decimation_counter++;
    if (stream->decimation_counter < stream->decimation) {
        return true;
    }
    stream->decimation_counter = 0U;

    next = vofa_next_index(stream->head);
    if (next == stream->tail) {
        stream->dropped_frames++;
        return false;
    }

    index = stream->head;
    (void)memcpy(stream->slots[index].frame,
                 channels,
                 stream->channel_count * sizeof(float));
    (void)memcpy(&stream->slots[index].frame[stream->channel_count * sizeof(float)],
                 vofa_tail,
                 sizeof(vofa_tail));
    stream->head = next;
    return true;
}

bool vofa_justfloat_push3_isr(vofa_justfloat_t *stream,
                              float channel0,
                              float channel1,
                              float channel2)
{
    float channels[3];

    channels[0] = channel0;
    channels[1] = channel1;
    channels[2] = channel2;
    return vofa_justfloat_push_isr(stream, channels, 3U);
}

bool vofa_justfloat_process(vofa_justfloat_t *stream)
{
    uint8_t index;

    if ((stream == NULL) || (stream->tx_busy != 0U) ||
        (stream->tail == stream->head)) {
        return false;
    }

    index = stream->tail;
    stream->tx_busy = 1U;
    if (!stream->start_tx(stream->slots[index].frame,
                          stream->frame_bytes,
                          stream->user_context)) {
        stream->tx_busy = 0U;
        return false;
    }

    return true;
}

void vofa_justfloat_on_tx_complete_isr(vofa_justfloat_t *stream)
{
    if ((stream == NULL) || (stream->tx_busy == 0U)) {
        return;
    }

    stream->tail = vofa_next_index(stream->tail);
    stream->tx_busy = 0U;
}

void vofa_justfloat_on_tx_error_isr(vofa_justfloat_t *stream)
{
    if ((stream == NULL) || (stream->tx_busy == 0U)) {
        return;
    }

    stream->tx_errors++;
    stream->tail = vofa_next_index(stream->tail);
    stream->tx_busy = 0U;
}

uint8_t vofa_justfloat_queued(const vofa_justfloat_t *stream)
{
    uint8_t head;
    uint8_t tail;

    if (stream == NULL) {
        return 0U;
    }
    head = stream->head;
    tail = stream->tail;
    return (head >= tail) ? (uint8_t)(head - tail)
                          : (uint8_t)(VOFA_JUSTFLOAT_QUEUE_DEPTH - tail + head);
}

uint32_t vofa_justfloat_dropped(const vofa_justfloat_t *stream)
{
    return (stream == NULL) ? 0U : stream->dropped_frames;
}

uint32_t vofa_justfloat_tx_errors(const vofa_justfloat_t *stream)
{
    return (stream == NULL) ? 0U : stream->tx_errors;
}
