#include "waveform.h"

#include <limits.h>

void waveform_analyze(const int16_t *samples, uint32_t sample_count,
                      int16_t *minimum, int16_t *maximum, size_t columns)
{
    for (size_t x = 0; x < columns; x++) {
        if (samples == NULL || sample_count == 0) {
            minimum[x] = 0;
            maximum[x] = 0;
            continue;
        }

        uint32_t first = (uint32_t)((uint64_t)x * sample_count / columns);
        uint32_t last = (uint32_t)((uint64_t)(x + 1) * sample_count
                                 / columns);
        if (last <= first)
            last = first + 1;
        if (last > sample_count)
            last = sample_count;

        int16_t low = INT16_MAX;
        int16_t high = INT16_MIN;
        for (uint32_t index = first; index < last; index++) {
            if (samples[index] < low)
                low = samples[index];
            if (samples[index] > high)
                high = samples[index];
        }
        minimum[x] = low;
        maximum[x] = high;
    }
}
