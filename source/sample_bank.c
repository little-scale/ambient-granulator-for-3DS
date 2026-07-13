#include "sample_bank.h"

#include <stdlib.h>
#include <string.h>

#define BANK_HEADER_SIZE 64
#define BANK_ENTRY_SIZE 64
#define BANK_ENTRIES_OFFSET 64
#define BANK_DATA_OFFSET (BANK_ENTRIES_OFFSET \
                          + BANK_ENTRY_SIZE * SAMPLE_BANK_MAX_ENTRIES)
#define BANK_FORMAT_VERSION 1
#define BANK_LEGACY_SAMPLE_RATE 16384
#define BANK_NATIVE_SAMPLE_RATE 48000

static bool valid_sample_rate(uint32_t sample_rate)
{
    return sample_rate == BANK_LEGACY_SAMPLE_RATE
        || sample_rate == BANK_NATIVE_SAMPLE_RATE;
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

static void set_error(SampleBank *bank, const char *message)
{
    snprintf(bank->error, sizeof(bank->error), "%s", message);
}

static uint32_t crc32_bytes(const uint8_t *bytes, size_t length)
{
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t index = 0; index < length; index++) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1)
                ^ ((crc & 1U) ? UINT32_C(0xEDB88320) : 0U);
    }
    return crc ^ UINT32_C(0xFFFFFFFF);
}

bool sample_bank_open(SampleBank *bank, const char *path)
{
    uint8_t header[BANK_DATA_OFFSET];
    memset(bank, 0, sizeof(*bank));
    snprintf(bank->path, sizeof(bank->path), "%s", path);

    bank->file = fopen(path, "rb");
    if (bank->file == NULL) {
        set_error(bank, "OPEN FAILED");
        return false;
    }
    if (fseek(bank->file, 0, SEEK_END) != 0) {
        set_error(bank, "SIZE FAILED");
        sample_bank_close(bank);
        return false;
    }
    long file_size = ftell(bank->file);
    if (file_size < 0 || (uint64_t)file_size < BANK_DATA_OFFSET
            || (uint64_t)file_size > UINT32_MAX) {
        set_error(bank, "BAD FILE SIZE");
        sample_bank_close(bank);
        return false;
    }
    bank->file_size = (uint32_t)file_size;
    if (fseek(bank->file, 0, SEEK_SET) != 0
            || fread(header, 1, sizeof(header), bank->file)
               != sizeof(header)) {
        set_error(bank, "HEADER READ FAILED");
        sample_bank_close(bank);
        return false;
    }

    bank->capacity = read_u32_le(header + 12);
    bank->sample_count = read_u32_le(header + 16);
    bank->used_bytes = read_u32_le(header + 20);
    bank->sample_rate = read_u32_le(header + 24);
    if (memcmp(header, "NDSGRN01", 8) != 0
            || read_u32_le(header + 8) != BANK_FORMAT_VERSION
            || bank->capacity < BANK_DATA_OFFSET
            || bank->capacity > bank->file_size
            || bank->used_bytes < BANK_DATA_OFFSET
            || bank->used_bytes > bank->capacity
            || bank->sample_count < 1
            || bank->sample_count > SAMPLE_BANK_MAX_ENTRIES
            || !valid_sample_rate(bank->sample_rate)
            || read_u32_le(header + 28) != BANK_ENTRY_SIZE
            || read_u32_le(header + 32) != SAMPLE_BANK_MAX_ENTRIES
            || read_u32_le(header + 36) != BANK_DATA_OFFSET) {
        set_error(bank, "INVALID NDSGRN01");
        sample_bank_close(bank);
        return false;
    }

    for (uint32_t index = 0; index < bank->sample_count; index++) {
        const uint8_t *entry = header + BANK_ENTRIES_OFFSET
                             + index * BANK_ENTRY_SIZE;
        SampleBankEntry *destination = &bank->entries[index];
        memcpy(destination->name, entry, SAMPLE_BANK_NAME_SIZE);
        destination->name[SAMPLE_BANK_NAME_SIZE] = '\0';
        destination->offset = read_u32_le(entry + SAMPLE_BANK_NAME_SIZE);
        destination->byte_length = read_u32_le(
            entry + SAMPLE_BANK_NAME_SIZE + 4);
        destination->crc32 = read_u32_le(entry + SAMPLE_BANK_NAME_SIZE + 8);

        if (destination->name[0] == '\0'
                || destination->byte_length < sizeof(int16_t)
                || (destination->byte_length & 1U) != 0
                || destination->offset < BANK_DATA_OFFSET
                || destination->offset > bank->used_bytes
                || destination->byte_length
                   > bank->used_bytes - destination->offset) {
            set_error(bank, "INVALID ENTRY");
            sample_bank_close(bank);
            return false;
        }
    }

    bank->error[0] = '\0';
    return true;
}

bool sample_bank_load(const SampleBank *bank, uint32_t index,
                      LoadedSample *sample)
{
    memset(sample, 0, sizeof(*sample));
    if (bank == NULL || bank->file == NULL || index >= bank->sample_count)
        return false;

    const SampleBankEntry *entry = &bank->entries[index];
    uint8_t *bytes = malloc(entry->byte_length);
    if (bytes == NULL)
        return false;
    if (fseek(bank->file, (long)entry->offset, SEEK_SET) != 0
            || fread(bytes, 1, entry->byte_length, bank->file)
               != entry->byte_length
            || crc32_bytes(bytes, entry->byte_length) != entry->crc32) {
        free(bytes);
        return false;
    }

    sample->samples = (int16_t *)bytes;
    sample->sample_count = entry->byte_length / sizeof(int16_t);
    sample->sample_rate = bank->sample_rate;
    snprintf(sample->name, sizeof(sample->name), "%s", entry->name);
    return true;
}

void sample_bank_close(SampleBank *bank)
{
    if (bank->file != NULL)
        fclose(bank->file);
    bank->file = NULL;
}

void loaded_sample_free(LoadedSample *sample)
{
    free(sample->samples);
    memset(sample, 0, sizeof(*sample));
}
