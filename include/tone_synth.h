#ifndef GRANULATOR_TONE_SYNTH_H
#define GRANULATOR_TONE_SYNTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TONE_SYNTH_SAMPLE_RATE 48000

typedef enum {
    TONE_MODE_MONO_PAN,
    TONE_MODE_STEREO_ID,
} ToneMode;

typedef struct {
    int frequency_hz;
    int gain_percent;
    int pan_percent;
    ToneMode mode;
    bool gate;
} ToneRenderConfig;

typedef struct {
    uint32_t phase_left;
    uint32_t phase_right;
    uint32_t trigger_frames;
    int envelope_q15;
} ToneSynth;

void tone_synth_init(ToneSynth *synth);
void tone_synth_trigger_ms(ToneSynth *synth, unsigned int milliseconds);
void tone_synth_render(ToneSynth *synth, int16_t *interleaved_stereo,
                       size_t frames, const ToneRenderConfig *config);

#endif
