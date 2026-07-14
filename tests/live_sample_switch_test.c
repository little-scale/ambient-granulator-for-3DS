#include "effects_chain.h"
#include "granular_engine.h"
#include "ram_sample.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SWITCH_TEST_FRAMES 4096
#define SWITCH_SAMPLE_COUNT 16384

static int16_t sample_a[SWITCH_SAMPLE_COUNT];
static int16_t sample_b[SWITCH_SAMPLE_COUNT];
static int16_t recording[1024];
static int16_t output[SWITCH_TEST_FRAMES * 2];
static int32_t mix_output[SWITCH_TEST_FRAMES * 2];

static bool buffer_has_audio(const int16_t *samples, size_t frames)
{
    for (size_t index = 0; index < frames * 2; index++) {
        if (samples[index] != 0)
            return true;
    }
    return false;
}

int main(void)
{
    for (int index = 0; index < SWITCH_SAMPLE_COUNT; index++) {
        sample_a[index] = (int16_t)(12000 - (index & 255));
        sample_b[index] = (int16_t)(-9000 + (index & 127));
    }
    for (int index = 0; index < 1024; index++)
        recording[index] = (int16_t)(index * 12 - 6000);

    GranularConfig grains = {
        .center_x = 64,
        .range = 0,
        .pitch_semitones = 0,
        .fine_cents = 0,
        .pitch_deviation = 0,
        .fine_deviation_cents = 0,
        .clock_sync = false,
        .bpm = 120,
        .division = 1,
        .interval_ms = 20,
        .jitter_percent = 0,
        .grain_count = 1,
        .voice_limit = GRANULAR_VOICE_COUNT,
        .length_ms = 200,
        .attack_percent = 0,
        .release_percent = 25,
        .gain_db = 0,
        .volume_percent = 100,
        .pan_percent = 0,
        .pan_divergence = 0,
        .gate = false,
    };
    EffectsConfig effects = {
        .wet_percent = 100,
        .feedback_tenths_percent = 999,
        .size_percent = 0,
        .damping_percent = 0,
        .freeze = false,
        .phaser_depth_percent = 20,
        .phaser_speed_hundredths_hz = 10,
        .highpass_hz = 0,
        .lowpass_hz = 8000,
    };

    GranularEngine engine;
    EffectsChain chain;
    RamSample ram_sample = { 0 };
    granular_engine_init(&engine);
    granular_engine_set_sample(&engine, sample_a, SWITCH_SAMPLE_COUNT, 16384);
    assert(effects_chain_init(&chain));
    granular_engine_trigger(&engine, grains.center_x, grains.grain_count);
    granular_engine_render_wide(&engine, mix_output,
                                SWITCH_TEST_FRAMES, &grains);
    effects_chain_process_wide(&chain, mix_output, output,
                               SWITCH_TEST_FRAMES, &effects);
    assert(buffer_has_audio(output, SWITCH_TEST_FRAMES));

    effects.freeze = true;
    assert(ram_sample_clone(&ram_sample, sample_b,
                            SWITCH_SAMPLE_COUNT, 16384));
    assert(ram_sample_punch_in(&ram_sample, 4096, recording, 1024, 32728)
           > 0);
    granular_engine_set_sample(&engine, ram_sample.samples,
                               ram_sample.sample_count,
                               ram_sample.sample_rate);
    memset(output, 0, sizeof(output));
    memset(mix_output, 0, sizeof(mix_output));
    effects_chain_process_wide(&chain, mix_output, output,
                               SWITCH_TEST_FRAMES, &effects);
    assert(buffer_has_audio(output, SWITCH_TEST_FRAMES));

    effects.freeze = false;
    granular_engine_trigger(&engine, 240, 1);
    granular_engine_render_wide(&engine, mix_output,
                                SWITCH_TEST_FRAMES, &grains);
    effects_chain_process_wide(&chain, mix_output, output,
                               SWITCH_TEST_FRAMES, &effects);
    assert(buffer_has_audio(output, SWITCH_TEST_FRAMES));
    ram_sample_free(&ram_sample);
    effects_chain_exit(&chain);
    puts("live_sample_switch_test: all checks passed");
    return 0;
}
