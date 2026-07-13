#ifndef GRANULATOR_EFFECTS_CHAIN_H
#define GRANULATOR_EFFECTS_CHAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EFFECTS_SAMPLE_RATE 48000
#define EFFECTS_FDN_LINES 4
#define EFFECTS_FDN_MAX_DELAY 24000
#define EFFECTS_PHASER_STAGES 4

typedef struct {
    int wet_percent;
    int feedback_tenths_percent;
    int size_percent;
    int damping_percent;
    bool freeze;
    int phaser_depth_percent;
    int phaser_speed_hundredths_hz;
    int highpass_hz;
    int lowpass_hz;
} EffectsConfig;

typedef struct {
    int32_t highpass_previous_input;
    int32_t highpass_output;
    int32_t lowpass_output;
} EffectsFilterState;

typedef struct {
    int32_t previous_input;
    int32_t previous_output;
} EffectsAllpassState;

typedef struct {
    int16_t *delay;
    int positions[EFFECTS_FDN_LINES];
    int32_t damped[EFFECTS_FDN_LINES];
    int lengths[EFFECTS_FDN_LINES];
    EffectsAllpassState phaser[2][EFFECTS_PHASER_STAGES];
    uint32_t phaser_phase;
    EffectsFilterState filters[2];
    int feedback_q15;
    int damping_q15;
    int highpass_alpha_q15;
    int lowpass_alpha_q15;
    int configured_size;
    int configured_damping;
    bool initialized;
} EffectsChain;

int effects_lowpass_alpha_q15(int cutoff_hz, int sample_rate);
int effects_highpass_alpha_q15(int cutoff_hz, int sample_rate);
int effects_damping_alpha_q15(int damping_percent, int sample_rate);
uint32_t effects_phaser_increment(int speed_hundredths_hz, int sample_rate);

bool effects_chain_init(EffectsChain *chain);
void effects_chain_reset(EffectsChain *chain);
void effects_chain_process(EffectsChain *chain,
                           int16_t *interleaved_stereo, size_t frames,
                           const EffectsConfig *config);
void effects_chain_exit(EffectsChain *chain);

#endif
