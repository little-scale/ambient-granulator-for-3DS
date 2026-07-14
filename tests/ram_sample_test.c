#include "ram_sample.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_clone_and_position(void)
{
    const int16_t source[] = { -4, -3, -2, -1, 0, 1, 2, 3 };
    RamSample sample = { 0 };
    assert(ram_sample_clone(&sample, source, 8, 48000));
    assert(sample.sample_count == 8);
    assert(sample.sample_rate == 48000);
    assert(memcmp(sample.samples, source, sizeof(source)) == 0);
    assert(ram_sample_position(sample.sample_count, 0, 320) == 0);
    assert(ram_sample_position(sample.sample_count, 319, 320) == 7);
    assert(ram_sample_position(3200, 160, 320) == 1604);
    ram_sample_free(&sample);
}

static void test_resampled_punch_preserves_other_regions(void)
{
    int16_t source[12];
    const int16_t recording[] = { 0, 1000, 2000, 3000 };
    for (int index = 0; index < 12; index++)
        source[index] = -12000;
    RamSample sample = { 0 };
    assert(ram_sample_clone(&sample, source, 12, 8));
    assert(ram_sample_punch_in(&sample, 2, recording, 4, 4) == 8);
    const int16_t expected[] = {
        -12000, -12000, 0, 500, 1000, 1500,
        2000, 2500, 3000, 3000, -12000, -12000,
    };
    assert(memcmp(sample.samples, expected, sizeof(expected)) == 0);

    const int16_t second[] = { -2000, -1000 };
    assert(ram_sample_punch_in(&sample, 0, second, 2, 8) == 2);
    assert(sample.samples[0] == -2000);
    assert(sample.samples[1] == -1000);
    assert(sample.samples[2] == 0);
    assert(sample.samples[10] == -12000);
    ram_sample_free(&sample);
}

static void test_punch_truncates_at_end(void)
{
    int16_t source[4] = { 1, 2, 3, 4 };
    int16_t recording[4] = { 10, 20, 30, 40 };
    RamSample sample = { 0 };
    assert(ram_sample_clone(&sample, source, 4, 48000));
    assert(ram_sample_punch_in(&sample, 3, recording, 4, 48000) == 1);
    assert(sample.samples[0] == 1);
    assert(sample.samples[2] == 3);
    assert(sample.samples[3] == 10);
    ram_sample_free(&sample);
}

int main(void)
{
    test_clone_and_position();
    test_resampled_punch_preserves_other_regions();
    test_punch_truncates_at_end();
    puts("ram_sample_test: all checks passed");
    return 0;
}
