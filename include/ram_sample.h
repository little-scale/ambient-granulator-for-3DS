#ifndef GRANULATOR_RAM_SAMPLE_H
#define GRANULATOR_RAM_SAMPLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *samples;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint32_t capacity_samples;
} RamSample;

bool ram_sample_clone(RamSample *sample, const int16_t *source,
                      uint32_t sample_count, uint32_t sample_rate);
uint32_t ram_sample_position(uint32_t sample_count, int screen_x,
                             int screen_width);
uint32_t ram_sample_punch_in(RamSample *sample, uint32_t destination_start,
                             const int16_t *recording,
                             uint32_t recording_samples,
                             uint32_t recording_rate);
void ram_sample_free(RamSample *sample);

#endif
