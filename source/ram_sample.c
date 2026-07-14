#include "ram_sample.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool ram_sample_clone(RamSample *sample, const int16_t *source,
                      uint32_t sample_count, uint32_t sample_rate)
{
    if (sample == NULL || source == NULL || sample_count == 0
            || sample_rate == 0)
        return false;
    size_t byte_count = (size_t)sample_count * sizeof(*source);
    if (byte_count / sizeof(*source) != sample_count)
        return false;
    if (source == sample->samples && sample_count == sample->sample_count
            && sample_rate == sample->sample_rate)
        return true;

    if (sample_count > sample->capacity_samples) {
        int16_t *replacement = malloc(byte_count);
        if (replacement == NULL)
            return false;
        free(sample->samples);
        sample->samples = replacement;
        sample->capacity_samples = sample_count;
    }
    memcpy(sample->samples, source, byte_count);
    sample->sample_count = sample_count;
    sample->sample_rate = sample_rate;
    return true;
}

uint32_t ram_sample_position(uint32_t sample_count, int screen_x,
                             int screen_width)
{
    if (sample_count < 2 || screen_width < 2)
        return 0;
    if (screen_x < 0)
        screen_x = 0;
    if (screen_x >= screen_width)
        screen_x = screen_width - 1;
    return (uint32_t)((uint64_t)(uint32_t)screen_x * (sample_count - 1)
                    / (uint32_t)(screen_width - 1));
}

uint32_t ram_sample_punch_in(RamSample *sample, uint32_t destination_start,
                             const int16_t *recording,
                             uint32_t recording_samples,
                             uint32_t recording_rate)
{
    if (sample == NULL || sample->samples == NULL || sample->sample_count == 0
            || sample->sample_rate == 0 || destination_start >= sample->sample_count
            || recording == NULL || recording_samples == 0
            || recording_rate == 0)
        return 0;

    uint64_t converted = ((uint64_t)recording_samples * sample->sample_rate
                        + recording_rate / 2U) / recording_rate;
    if (converted < 1)
        converted = 1;
    uint32_t available = sample->sample_count - destination_start;
    if (converted > available)
        converted = available;

    uint64_t position_q32 = 0;
    uint64_t step_q32 = ((uint64_t)recording_rate << 32)
                      / sample->sample_rate;
    if (step_q32 == 0)
        step_q32 = 1;
    for (uint32_t index = 0; index < converted; index++) {
        uint32_t first = (uint32_t)(position_q32 >> 32);
        if (first >= recording_samples)
            first = recording_samples - 1;
        uint32_t second = first + 1;
        if (second >= recording_samples)
            second = first;
        int32_t fraction_q15 = (int32_t)(position_q32 >> 17) & 0x7FFF;
        int32_t difference = (int32_t)recording[second] - recording[first];
        int32_t value = recording[first]
                      + (difference * fraction_q15 >> 15);
        if (value < INT16_MIN)
            value = INT16_MIN;
        if (value > INT16_MAX)
            value = INT16_MAX;
        sample->samples[destination_start + index] = (int16_t)value;
        position_q32 += step_q32;
    }
    return (uint32_t)converted;
}

void ram_sample_free(RamSample *sample)
{
    if (sample == NULL)
        return;
    free(sample->samples);
    memset(sample, 0, sizeof(*sample));
}
