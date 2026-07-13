#ifndef GRANULATOR_SAMPLE_LIBRARY_H
#define GRANULATOR_SAMPLE_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sample_bank.h"

typedef struct {
    LoadedSample samples[SAMPLE_BANK_MAX_ENTRIES];
    int16_t *waveform_minimum;
    int16_t *waveform_maximum;
    uint32_t sample_count;
    uint32_t total_pcm_bytes;
    size_t waveform_columns;
} SampleLibrary;

bool sample_library_load(const SampleBank *bank, size_t waveform_columns,
                         SampleLibrary *library);
const LoadedSample *sample_library_get(const SampleLibrary *library,
                                       uint32_t index);
bool sample_library_copy_waveform(const SampleLibrary *library,
                                  uint32_t index, int16_t *minimum,
                                  int16_t *maximum, size_t columns);
void sample_library_free(SampleLibrary *library);

#endif
