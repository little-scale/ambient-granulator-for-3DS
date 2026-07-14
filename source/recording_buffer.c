#include "recording_buffer.h"

#include <string.h>

void recording_buffer_begin(RecordingBuffer *buffer, int16_t *destination,
                            size_t capacity_samples)
{
    buffer->destination = (uint8_t *)destination;
    buffer->capacity_bytes = capacity_samples * sizeof(*destination);
    buffer->captured_bytes = 0;
    buffer->ring_read_offset = 0;
    buffer->full = destination == NULL || capacity_samples == 0;
}

size_t recording_buffer_drain(RecordingBuffer *buffer,
                              const uint8_t *ring, uint32_t ring_size,
                              uint32_t write_offset)
{
    if (buffer == NULL || buffer->destination == NULL || ring == NULL
            || ring_size < sizeof(int16_t) || (ring_size & 1U) != 0
            || write_offset >= ring_size)
        return 0;

    write_offset &= ~1U;
    buffer->ring_read_offset &= ~1U;
    size_t copied = 0;
    while (!buffer->full && buffer->ring_read_offset != write_offset) {
        uint32_t available = write_offset > buffer->ring_read_offset
            ? write_offset - buffer->ring_read_offset
            : ring_size - buffer->ring_read_offset;
        available &= ~1U;
        size_t remaining = buffer->capacity_bytes - buffer->captured_bytes;
        size_t amount = available < remaining ? available : remaining;
        amount &= ~(sizeof(int16_t) - 1U);
        if (amount == 0) {
            buffer->full = remaining < sizeof(int16_t);
            break;
        }

        memcpy(buffer->destination + buffer->captured_bytes,
               ring + buffer->ring_read_offset, amount);
        buffer->captured_bytes += amount;
        copied += amount;
        buffer->ring_read_offset
            = (buffer->ring_read_offset + (uint32_t)amount) % ring_size;
        if (buffer->captured_bytes >= buffer->capacity_bytes)
            buffer->full = true;
    }
    return copied;
}

size_t recording_buffer_sample_count(const RecordingBuffer *buffer)
{
    return buffer == NULL ? 0
        : buffer->captured_bytes / sizeof(int16_t);
}
