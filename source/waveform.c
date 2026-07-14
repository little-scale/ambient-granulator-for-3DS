#include "waveform.h"

#include <limits.h>

static void analyze_columns(const int16_t *samples, uint32_t sample_count,
                            int16_t *minimum, int16_t *maximum,
                            size_t columns, size_t first_column,
                            size_t end_column)
{
    for (size_t x = first_column; x < end_column; x++) {
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

void waveform_analyze(const int16_t *samples, uint32_t sample_count,
                      int16_t *minimum, int16_t *maximum, size_t columns)
{
    analyze_columns(samples, sample_count, minimum, maximum,
                    columns, 0, columns);
}

void waveform_analyze_changed(const int16_t *samples, uint32_t sample_count,
                              int16_t *minimum, int16_t *maximum,
                              size_t columns, uint32_t changed_first,
                              uint32_t changed_count)
{
    if (samples == NULL || sample_count == 0 || columns == 0
            || changed_count == 0 || changed_first >= sample_count)
        return;
    uint64_t changed_end = (uint64_t)changed_first + changed_count;
    if (changed_end > sample_count)
        changed_end = sample_count;
    size_t first_column = (size_t)((((uint64_t)changed_first + 1) * columns
                                   - 1) / sample_count);
    size_t last_column = (size_t)((changed_end * columns - 1)
                                 / sample_count);
    if (last_column >= columns)
        last_column = columns - 1;
    analyze_columns(samples, sample_count, minimum, maximum,
                    columns, first_column, last_column + 1);
}
