#ifndef GRANULATOR_SAMPLE_BANK_H
#define GRANULATOR_SAMPLE_BANK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define SAMPLE_BANK_MAX_ENTRIES 64
#define SAMPLE_BANK_NAME_SIZE 32
#define SAMPLE_BANK_PATH_SIZE 96
#define SAMPLE_BANK_ERROR_SIZE 64

typedef struct {
    char name[SAMPLE_BANK_NAME_SIZE + 1];
    uint32_t offset;
    uint32_t byte_length;
    uint32_t crc32;
} SampleBankEntry;

typedef struct {
    FILE *file;
    uint32_t file_size;
    uint32_t capacity;
    uint32_t used_bytes;
    uint32_t sample_count;
    uint32_t sample_rate;
    SampleBankEntry entries[SAMPLE_BANK_MAX_ENTRIES];
    char path[SAMPLE_BANK_PATH_SIZE];
    char error[SAMPLE_BANK_ERROR_SIZE];
} SampleBank;

typedef struct {
    int16_t *samples;
    uint32_t sample_count;
    uint32_t sample_rate;
    char name[SAMPLE_BANK_NAME_SIZE + 1];
} LoadedSample;

bool sample_bank_open(SampleBank *bank, const char *path);
bool sample_bank_load(const SampleBank *bank, uint32_t index,
                      LoadedSample *sample);
void sample_bank_close(SampleBank *bank);
void loaded_sample_free(LoadedSample *sample);

#endif
