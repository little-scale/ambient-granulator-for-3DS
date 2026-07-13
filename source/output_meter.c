#include "output_meter.h"

#include <string.h>

static uint16_t absolute_sample(int16_t sample)
{
    return sample < 0 ? (uint16_t)(-(int32_t)sample) : (uint16_t)sample;
}

static uint16_t update_peak(uint16_t held, uint16_t block)
{
    uint16_t decayed = (uint16_t)((uint32_t)held * 31 / 32);
    return block > decayed ? block : decayed;
}

void output_meter_init(OutputMeter *meter)
{
    memset(meter, 0, sizeof(*meter));
}

void output_meter_process(OutputMeter *meter,
                          const int16_t *samples, size_t frames)
{
    uint16_t left = 0;
    uint16_t right = 0;
    bool clipped = false;
    for (size_t frame = 0; frame < frames; frame++) {
        uint16_t left_sample = absolute_sample(samples[frame * 2]);
        uint16_t right_sample = absolute_sample(samples[frame * 2 + 1]);
        if (left_sample > left)
            left = left_sample;
        if (right_sample > right)
            right = right_sample;
        clipped |= left_sample >= 32767 || right_sample >= 32767;
    }
    meter->peak_left = update_peak(meter->peak_left, left);
    meter->peak_right = update_peak(meter->peak_right, right);
    if (clipped)
        meter->clip_hold_blocks = OUTPUT_METER_CLIP_HOLD_BLOCKS;
    else if (meter->clip_hold_blocks > 0)
        meter->clip_hold_blocks--;
}

bool output_meter_clipping(const OutputMeter *meter)
{
    return meter->clip_hold_blocks > 0;
}

void output_meter_mark_clipped(OutputMeter *meter)
{
    meter->clip_hold_blocks = OUTPUT_METER_CLIP_HOLD_BLOCKS;
}
