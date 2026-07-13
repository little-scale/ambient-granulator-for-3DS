#include "sample_library.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "waveform.h"

void sample_library_free(SampleLibrary *library)
{
    if (library == NULL)
        return;
    for (uint32_t index = 0; index < library->sample_count; index++)
        loaded_sample_free(&library->samples[index]);
    free(library->waveform_minimum);
    free(library->waveform_maximum);
    memset(library, 0, sizeof(*library));
}

bool sample_library_load(const SampleBank *bank, size_t waveform_columns,
                         SampleLibrary *library)
{
    if (library == NULL)
        return false;
    memset(library, 0, sizeof(*library));
    if (bank == NULL || bank->file == NULL || bank->sample_count == 0
            || waveform_columns == 0
            || bank->sample_count > SIZE_MAX / waveform_columns)
        return false;

    size_t cells = (size_t)bank->sample_count * waveform_columns;
    if (cells > SIZE_MAX / sizeof(int16_t))
        return false;
    library->waveform_minimum = malloc(cells * sizeof(int16_t));
    library->waveform_maximum = malloc(cells * sizeof(int16_t));
    if (library->waveform_minimum == NULL
            || library->waveform_maximum == NULL) {
        sample_library_free(library);
        return false;
    }
    library->waveform_columns = waveform_columns;

    for (uint32_t index = 0; index < bank->sample_count; index++) {
        if (!sample_bank_load(bank, index, &library->samples[index])) {
            sample_library_free(library);
            return false;
        }
        library->sample_count++;
        library->total_pcm_bytes += bank->entries[index].byte_length;
        size_t offset = (size_t)index * waveform_columns;
        waveform_analyze(library->samples[index].samples,
                         library->samples[index].sample_count,
                         library->waveform_minimum + offset,
                         library->waveform_maximum + offset,
                         waveform_columns);
    }
    return true;
}

const LoadedSample *sample_library_get(const SampleLibrary *library,
                                       uint32_t index)
{
    if (library == NULL || index >= library->sample_count)
        return NULL;
    return &library->samples[index];
}

bool sample_library_copy_waveform(const SampleLibrary *library,
                                  uint32_t index, int16_t *minimum,
                                  int16_t *maximum, size_t columns)
{
    if (library == NULL || minimum == NULL || maximum == NULL
            || index >= library->sample_count
            || columns != library->waveform_columns)
        return false;
    size_t offset = (size_t)index * columns;
    memcpy(minimum, library->waveform_minimum + offset,
           columns * sizeof(*minimum));
    memcpy(maximum, library->waveform_maximum + offset,
           columns * sizeof(*maximum));
    return true;
}
