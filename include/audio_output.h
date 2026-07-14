#ifndef GRANULATOR_AUDIO_OUTPUT_H
#define GRANULATOR_AUDIO_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include <3ds.h>

#include "effects_chain.h"
#include "granular_engine.h"
#include "output_meter.h"

#define AUDIO_BUFFER_COUNT 4
#define AUDIO_BUFFER_FRAMES 512

typedef struct {
    GranularConfig granular;
    EffectsConfig effects;
} AudioRenderConfig;

typedef struct {
    bool ready;
    volatile bool quit;
    Result init_result;
    int16_t *samples;
    int32_t *mix_samples;
    AudioRenderConfig config;
    LightLock config_lock;
    LightLock state_lock;
    LightEvent event;
    Thread thread;
    ndspWaveBuf wave_buffers[AUDIO_BUFFER_COUNT];
    GranularEngine engine;
    EffectsChain effects;
    OutputMeter meter;
    uint32_t buffers_submitted;
    uint32_t underruns;
    uint32_t late_refills;
    uint32_t silent_blocks;
    uint32_t active_voices_snapshot;
    uint32_t grains_launched_snapshot;
    uint32_t peak_left_snapshot;
    uint32_t peak_right_snapshot;
    uint32_t clipping_snapshot;
} AudioOutput;

bool audio_output_init(AudioOutput *output,
                       const AudioRenderConfig *initial_config,
                       const int16_t *sample, uint32_t sample_count,
                       uint32_t sample_rate);
void audio_output_update(AudioOutput *output,
                         const AudioRenderConfig *config);
void audio_output_set_sample(AudioOutput *output, const int16_t *sample,
                             uint32_t sample_count, uint32_t sample_rate);
void audio_output_stop_grains(AudioOutput *output);
void audio_output_trigger(AudioOutput *output, int center_x,
                          int grain_count);
bool audio_output_pop_marker(AudioOutput *output, GranularMarker *marker);
int audio_output_active_voices(const AudioOutput *output);
uint32_t audio_output_grains_launched(const AudioOutput *output);
uint16_t audio_output_peak_left(const AudioOutput *output);
uint16_t audio_output_peak_right(const AudioOutput *output);
bool audio_output_clipping(const AudioOutput *output);
uint32_t audio_output_buffers_submitted(const AudioOutput *output);
uint32_t audio_output_underruns(const AudioOutput *output);
uint32_t audio_output_late_refills(const AudioOutput *output);
uint32_t audio_output_silent_blocks(const AudioOutput *output);
void audio_output_exit(AudioOutput *output);

#endif
