#ifndef GRANULATOR_RECORDING_BUFFER_H
#define GRANULATOR_RECORDING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *destination;
    size_t capacity_bytes;
    size_t captured_bytes;
    uint32_t ring_read_offset;
    bool full;
} RecordingBuffer;

void recording_buffer_begin(RecordingBuffer *buffer, int16_t *destination,
                            size_t capacity_samples);
size_t recording_buffer_drain(RecordingBuffer *buffer,
                              const uint8_t *ring, uint32_t ring_size,
                              uint32_t write_offset);
size_t recording_buffer_sample_count(const RecordingBuffer *buffer);
uint16_t recording_buffer_peak(const int16_t *samples, size_t sample_count);
uint16_t recording_buffer_ring_peak(const uint8_t *ring, uint32_t ring_size,
                                    uint32_t *read_offset,
                                    uint32_t write_offset);

#endif
