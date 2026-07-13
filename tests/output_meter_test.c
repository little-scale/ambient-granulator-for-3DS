#include "output_meter.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int16_t samples[32];

int main(void)
{
    OutputMeter meter;
    output_meter_init(&meter);
    assert(meter.peak_left == 0);
    assert(meter.peak_right == 0);
    assert(!output_meter_clipping(&meter));

    memset(samples, 0, sizeof(samples));
    samples[4] = -1000;
    samples[11] = 2000;
    output_meter_process(&meter, samples, 16);
    assert(meter.peak_left == 1000);
    assert(meter.peak_right == 2000);

    memset(samples, 0, sizeof(samples));
    output_meter_process(&meter, samples, 16);
    assert(meter.peak_left == 968);
    assert(meter.peak_right == 1937);

    samples[0] = 32767;
    samples[1] = -32768;
    output_meter_process(&meter, samples, 16);
    assert(meter.peak_left == 32767);
    assert(meter.peak_right == 32768);
    assert(output_meter_clipping(&meter));

    memset(samples, 0, sizeof(samples));
    for (int block = 0; block < OUTPUT_METER_CLIP_HOLD_BLOCKS - 1; block++)
        output_meter_process(&meter, samples, 16);
    assert(output_meter_clipping(&meter));
    output_meter_process(&meter, samples, 16);
    assert(!output_meter_clipping(&meter));
    puts("output_meter_test: all checks passed");
    return 0;
}
