#ifndef GRANULATOR_OUTPUT_METER_H
#define GRANULATOR_OUTPUT_METER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OUTPUT_METER_CLIP_HOLD_BLOCKS 94

typedef struct {
    uint16_t peak_left;
    uint16_t peak_right;
    uint16_t clip_hold_blocks;
} OutputMeter;

void output_meter_init(OutputMeter *meter);
void output_meter_process(OutputMeter *meter,
                          const int16_t *interleaved_stereo, size_t frames);
void output_meter_mark_clipped(OutputMeter *meter);
bool output_meter_clipping(const OutputMeter *meter);

#endif
