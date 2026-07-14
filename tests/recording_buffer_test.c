#include "recording_buffer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_linear_and_wrapped_drain(void)
{
    uint8_t ring[16];
    int16_t destination[16];
    RecordingBuffer buffer;
    for (uint8_t index = 0; index < sizeof(ring); index++)
        ring[index] = index;
    memset(destination, 0, sizeof(destination));
    recording_buffer_begin(&buffer, destination, 16);

    assert(recording_buffer_drain(&buffer, ring, sizeof(ring), 8) == 8);
    assert(recording_buffer_sample_count(&buffer) == 4);
    assert(memcmp(destination, ring, 8) == 0);

    ring[0] = 16;
    ring[1] = 17;
    ring[2] = 18;
    ring[3] = 19;
    assert(recording_buffer_drain(&buffer, ring, sizeof(ring), 4) == 12);
    const uint8_t expected[] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19,
    };
    assert(recording_buffer_sample_count(&buffer) == 10);
    assert(memcmp(destination, expected, sizeof(expected)) == 0);
}

static void test_capacity_and_reset(void)
{
    uint8_t ring[16];
    int16_t first[3];
    int16_t second[4];
    RecordingBuffer buffer;
    for (uint8_t index = 0; index < sizeof(ring); index++)
        ring[index] = (uint8_t)(32 + index);

    recording_buffer_begin(&buffer, first, 3);
    assert(recording_buffer_drain(&buffer, ring, sizeof(ring), 12) == 6);
    assert(recording_buffer_sample_count(&buffer) == 3);
    assert(buffer.full);
    assert(memcmp(first, ring, sizeof(first)) == 0);

    recording_buffer_begin(&buffer, second, 4);
    assert(!buffer.full);
    assert(recording_buffer_sample_count(&buffer) == 0);
    assert(recording_buffer_drain(&buffer, ring, sizeof(ring), 8) == 8);
    assert(buffer.full);
    assert(memcmp(second, ring, sizeof(second)) == 0);
}

static void test_invalid_ring_inputs(void)
{
    uint8_t ring[8] = { 0 };
    int16_t destination[4];
    RecordingBuffer buffer;
    recording_buffer_begin(&buffer, destination, 4);
    assert(recording_buffer_drain(&buffer, ring, 7, 4) == 0);
    assert(recording_buffer_drain(&buffer, ring, sizeof(ring), 8) == 0);
    assert(recording_buffer_sample_count(&buffer) == 0);
}

int main(void)
{
    test_linear_and_wrapped_drain();
    test_capacity_and_reset();
    test_invalid_ring_inputs();
    puts("recording_buffer_test: all checks passed");
    return 0;
}
