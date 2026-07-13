#ifndef GRANULATOR_GRANULAR_ENGINE_H
#define GRANULATOR_GRANULAR_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GRANULAR_OUTPUT_RATE 48000
#define GRANULAR_VOICE_COUNT 16
#define GRANULAR_MARKER_QUEUE_SIZE 64

typedef struct {
    int center_x;
    int range;
    int pitch_semitones;
    int fine_cents;
    int pitch_deviation;
    int fine_deviation_cents;
    bool clock_sync;
    int bpm;
    int division;
    int interval_ms;
    int jitter_percent;
    int grain_count;
    int voice_limit;
    int length_ms;
    int attack_percent;
    int release_percent;
    int gain_db;
    int volume_percent;
    int pan_percent;
    int pan_divergence;
    bool gate;
} GranularConfig;

typedef struct {
    int x;
    uint64_t frame;
} GranularMarker;

typedef struct {
    bool active;
    const int16_t *source;
    uint64_t position_q16;
    uint32_t step_q16;
    uint32_t length;
    uint32_t attack_samples;
    uint32_t release_samples;
    int32_t left_gain_q8;
    int32_t right_gain_q8;
} GranularVoice;

typedef struct {
    const int16_t *sample;
    uint32_t sample_count;
    uint32_t source_rate;
    GranularVoice voices[GRANULAR_VOICE_COUNT];
    int next_voice;
    int burst_center;
    int burst_remaining;
    int frames_until_grain;
    bool previous_gate;
    uint32_t random_state;
    uint64_t rendered_frames;
    uint32_t grains_launched;
    GranularMarker markers[GRANULAR_MARKER_QUEUE_SIZE];
    uint8_t marker_read;
    uint8_t marker_write;
} GranularEngine;

int granular_interval_frames(int output_rate, bool synced, int bpm,
                             int denominator, int interval_ms);
int granular_range_offset(int range, uint32_t random_value);
uint32_t granular_pitch_step_q16(uint32_t source_rate, int output_rate,
                                 int semitones);
uint32_t granular_pitch_step_fine_q16(uint32_t source_rate, int output_rate,
                                      int semitones, int cents);

void granular_engine_init(GranularEngine *engine);
void granular_engine_set_sample(GranularEngine *engine,
                                const int16_t *sample,
                                uint32_t sample_count,
                                uint32_t source_rate);
void granular_engine_trigger(GranularEngine *engine, int center_x,
                             int grain_count);
void granular_engine_render(GranularEngine *engine,
                            int16_t *interleaved_stereo, size_t frames,
                            const GranularConfig *config);
void granular_engine_render_wide(GranularEngine *engine,
                                 int32_t *interleaved_stereo, size_t frames,
                                 const GranularConfig *config);
void granular_engine_render_silent(GranularEngine *engine,
                                   int16_t *interleaved_stereo,
                                   size_t frames,
                                   const GranularConfig *config);
void granular_engine_render_silent_wide(GranularEngine *engine,
                                        int32_t *interleaved_stereo,
                                        size_t frames,
                                        const GranularConfig *config);
bool granular_engine_pop_marker(GranularEngine *engine,
                                GranularMarker *marker);
int granular_engine_active_voices(const GranularEngine *engine);

#endif
