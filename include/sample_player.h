#ifndef GRANULATOR_SAMPLE_PLAYER_H
#define GRANULATOR_SAMPLE_PLAYER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAMPLE_PLAYER_OUTPUT_RATE 48000

typedef struct {
    uint32_t start_sample;
    int rate_percent;
    int gain_percent;
    int pan_percent;
    bool gate;
} SampleRenderConfig;

typedef struct {
    const int16_t *samples;
    uint32_t sample_count;
    uint32_t source_rate;
    uint64_t position_q16;
    uint32_t trigger_frames;
    uint32_t last_start_sample;
    int envelope_q15;
    bool previous_gate;
} SamplePlayer;

void sample_player_init(SamplePlayer *player);
void sample_player_set_sample(SamplePlayer *player, const int16_t *samples,
                              uint32_t sample_count, uint32_t source_rate);
void sample_player_trigger_ms(SamplePlayer *player, uint32_t start_sample,
                              unsigned int milliseconds);
void sample_player_render(SamplePlayer *player, int16_t *interleaved_stereo,
                          size_t frames, const SampleRenderConfig *config);

#endif
