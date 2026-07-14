#ifndef GRANULATOR_WAVEFORM_H
#define GRANULATOR_WAVEFORM_H

#include <stddef.h>
#include <stdint.h>

void waveform_analyze(const int16_t *samples, uint32_t sample_count,
                      int16_t *minimum, int16_t *maximum, size_t columns);
void waveform_analyze_changed(const int16_t *samples, uint32_t sample_count,
                              int16_t *minimum, int16_t *maximum,
                              size_t columns, uint32_t changed_first,
                              uint32_t changed_count);

#endif
