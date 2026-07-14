#include "granular_engine.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_SAMPLE_COUNT 32768
#define TEST_OUTPUT_FRAMES 16000

static int16_t sample[TEST_SAMPLE_COUNT];
static int16_t output[TEST_OUTPUT_FRAMES * 2];
static int32_t wide_output[TEST_OUTPUT_FRAMES * 2];

static GranularConfig default_config(void)
{
    GranularConfig config = {
        .center_x = 160,
        .range = 0,
        .pitch_semitones = 0,
        .fine_cents = 0,
        .pitch_deviation = 0,
        .fine_deviation_cents = 0,
        .clock_sync = false,
        .bpm = 120,
        .division = 1,
        .interval_ms = 10,
        .jitter_percent = 0,
        .grain_count = 4,
        .voice_limit = GRANULAR_VOICE_COUNT,
        .length_ms = 20,
        .attack_percent = 0,
        .release_percent = 0,
        .gain_db = 0,
        .volume_percent = 100,
        .pan_percent = 0,
        .pan_divergence = 0,
        .gate = false,
    };
    return config;
}

static void reset_engine(GranularEngine *engine)
{
    granular_engine_init(engine);
    granular_engine_set_sample(engine, sample, TEST_SAMPLE_COUNT, 16384);
}

static void test_contract_math(void)
{
    assert(granular_interval_frames(48000, true, 120, 16, 120) == 6000);
    assert(granular_interval_frames(48000, false, 120, 16, 125) == 6000);
    assert(granular_range_offset(24, 0) == -24);
    assert(granular_range_offset(-24, 0) == 0);
    for (uint32_t random = 0; random < 10000; random++) {
        int bipolar = granular_range_offset(24, random);
        int forward = granular_range_offset(-24, random);
        assert(bipolar >= -24 && bipolar <= 24);
        assert(forward >= 0 && forward <= 24);
    }
    uint32_t normal = granular_pitch_step_q16(16384, 48000, 0);
    uint32_t octave = granular_pitch_step_q16(16384, 48000, 12);
    assert(normal >= 22368 && normal <= 22370);
    assert(octave >= normal * 2 - 1 && octave <= normal * 2 + 1);
    uint32_t fine_up = granular_pitch_step_fine_q16(
        16384, 48000, 0, 100);
    uint32_t semitone_up = granular_pitch_step_q16(16384, 48000, 1);
    assert(fine_up >= semitone_up - 1 && fine_up <= semitone_up + 1);
    uint32_t fine_down = granular_pitch_step_fine_q16(
        16384, 48000, 0, -100);
    uint32_t semitone_down = granular_pitch_step_q16(16384, 48000, -1);
    assert(fine_down >= semitone_down - 1
        && fine_down <= semitone_down + 1);
}

static void test_fine_tuning(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 1;
    config.fine_cents = 37;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 1);
    granular_engine_render(&engine, output, 1, &config);
    assert(engine.voices[0].active);
    assert(engine.voices[0].step_q16 == granular_pitch_step_fine_q16(
        16384, GRANULAR_OUTPUT_RATE, 0, 37));
}

static void test_sample_clock_burst(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 4);
    granular_engine_render(&engine, output, 2000, &config);

    const uint64_t expected[] = { 0, 480, 960, 1440 };
    GranularMarker marker;
    for (int index = 0; index < 4; index++) {
        assert(granular_engine_pop_marker(&engine, &marker));
        assert(marker.x == 160);
        assert(marker.frame == expected[index]);
    }
    assert(!granular_engine_pop_marker(&engine, &marker));
}

static void test_sync_and_gate_repetition(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.clock_sync = true;
    config.division = 1;
    config.grain_count = 2;
    config.gate = true;
    reset_engine(&engine);
    granular_engine_render(&engine, output, 13000, &config);

    GranularMarker marker;
    assert(granular_engine_pop_marker(&engine, &marker));
    assert(marker.frame == 0);
    assert(granular_engine_pop_marker(&engine, &marker));
    assert(marker.frame == 6000);
    assert(granular_engine_pop_marker(&engine, &marker));
    assert(marker.frame == 12000);
}

static void test_gate_release_cancels_unstarted_repeat(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 1;
    config.gate = true;
    reset_engine(&engine);
    granular_engine_render(&engine, output, 1, &config);

    GranularMarker marker;
    assert(granular_engine_pop_marker(&engine, &marker));
    assert(marker.frame == 0);
    assert(engine.gate_repeat_pending);

    config.gate = false;
    granular_engine_render(&engine, output, 2000, &config);
    assert(!engine.gate_repeat_pending);
    assert(engine.grains_launched == 1);
    assert(!granular_engine_pop_marker(&engine, &marker));
}

static void test_gate_release_finishes_current_burst(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 4;
    config.gate = true;
    reset_engine(&engine);
    granular_engine_render(&engine, output, 1, &config);

    config.gate = false;
    granular_engine_render(&engine, output, 2000, &config);
    const uint64_t expected[] = { 0, 480, 960, 1440 };
    GranularMarker marker;
    for (int index = 0; index < 4; index++) {
        assert(granular_engine_pop_marker(&engine, &marker));
        assert(marker.frame == expected[index]);
    }
    assert(engine.grains_launched == 4);
    assert(!granular_engine_pop_marker(&engine, &marker));
}

static void test_range_modes(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 32;
    config.interval_ms = 1;
    config.range = 24;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 100, 32);
    granular_engine_render(&engine, output, 2000, &config);
    GranularMarker marker;
    while (granular_engine_pop_marker(&engine, &marker))
        assert(marker.x >= 76 && marker.x <= 124);

    config.range = -24;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 100, 32);
    granular_engine_render(&engine, output, 2000, &config);
    while (granular_engine_pop_marker(&engine, &marker))
        assert(marker.x >= 100 && marker.x <= 124);
}

static void test_audio_pan_and_envelope(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 1;
    config.pan_percent = -100;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 1);
    granular_engine_render(&engine, output, 256, &config);
    bool heard = false;
    for (int frame = 0; frame < 256; frame++) {
        heard |= output[frame * 2] != 0;
        assert(output[frame * 2 + 1] == 0);
    }
    assert(heard);

    config.pan_percent = 0;
    config.attack_percent = 25;
    config.release_percent = 25;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 1);
    granular_engine_render(&engine, output, 1200, &config);
    assert(output[0] == 0);
    bool middle_heard = false;
    for (int frame = 200; frame < 700; frame++)
        middle_heard |= output[frame * 2] != 0;
    assert(middle_heard);
    for (int frame = 1100; frame < 1200; frame++) {
        assert(output[frame * 2] == 0);
        assert(output[frame * 2 + 1] == 0);
    }
}

static void test_polyphony_limit(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 32;
    config.interval_ms = 1;
    config.length_ms = 500;

    config.voice_limit = 4;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, config.grain_count);
    granular_engine_render(&engine, output, 769, &config);
    assert(engine.grains_launched == 17);
    assert(granular_engine_active_voices(&engine) == 4);

    config.voice_limit = GRANULAR_VOICE_COUNT;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, config.grain_count);
    granular_engine_render(&engine, output, 769, &config);
    assert(engine.grains_launched == 17);
    assert(granular_engine_active_voices(&engine) == GRANULAR_VOICE_COUNT);

    config.voice_limit = 4;
    granular_engine_render(&engine, output, 1, &config);
    assert(granular_engine_active_voices(&engine) == 4);
}

static void test_stop_preserves_source_and_clock(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 4);
    granular_engine_render(&engine, output, 64, &config);
    const int16_t *source = engine.sample;
    uint32_t sample_count = engine.sample_count;
    uint32_t source_rate = engine.source_rate;
    uint64_t rendered_frames = engine.rendered_frames;
    granular_engine_stop(&engine);
    assert(granular_engine_active_voices(&engine) == 0);
    assert(engine.tails[0].voice.active);
    assert(engine.tails[0].frames_remaining == GRANULAR_DECLICK_FRAMES);
    assert(engine.burst_remaining == 0);
    assert(engine.sample == source);
    assert(engine.sample_count == sample_count);
    assert(engine.source_rate == source_rate);
    assert(engine.rendered_frames == rendered_frames);
}

static void test_minimum_declick_envelope(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 1;
    config.length_ms = 500;
    config.attack_percent = 0;
    config.release_percent = 0;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 1);
    granular_engine_render(&engine, output, GRANULAR_DECLICK_FRAMES, &config);

    assert(output[0] == 0);
    assert(output[1] == 0);
    assert(output[(GRANULAR_DECLICK_FRAMES - 1) * 2] != 0);
    assert(engine.voices[0].age_frames == GRANULAR_DECLICK_FRAMES);
}

static void test_voice_steal_and_stop_are_declicked(void)
{
    GranularEngine engine;
    GranularConfig config = default_config();
    config.grain_count = 1;
    config.voice_limit = 1;
    config.length_ms = 500;
    config.attack_percent = 0;
    config.release_percent = 0;
    reset_engine(&engine);
    granular_engine_trigger(&engine, 160, 1);
    granular_engine_render(&engine, output, GRANULAR_DECLICK_FRAMES, &config);
    int before_steal = output[(GRANULAR_DECLICK_FRAMES - 1) * 2];

    granular_engine_trigger(&engine, 160, 1);
    granular_engine_render(&engine, output, 1, &config);
    assert(engine.tails[0].voice.active);
    assert(engine.tails[0].frames_remaining == GRANULAR_DECLICK_FRAMES - 1);
    int steal_delta = output[0] - before_steal;
    assert(steal_delta >= -2 && steal_delta <= 2);

    granular_engine_stop(&engine);
    assert(granular_engine_active_voices(&engine) == 0);
    granular_engine_render(&engine, output, GRANULAR_DECLICK_FRAMES, &config);
    assert(output[(GRANULAR_DECLICK_FRAMES - 1) * 2] == 0);
    for (int index = 0; index < GRANULAR_TAIL_COUNT; index++)
        assert(!engine.tails[index].voice.active);
}

static void test_wide_mix_preserves_overload_headroom(void)
{
    GranularEngine narrow;
    GranularEngine wide;
    GranularConfig config = default_config();
    config.grain_count = GRANULAR_VOICE_COUNT;
    config.interval_ms = 1;
    config.length_ms = 500;
    config.attack_percent = 0;
    config.release_percent = 0;
    reset_engine(&narrow);
    reset_engine(&wide);
    granular_engine_trigger(&narrow, 160, config.grain_count);
    granular_engine_trigger(&wide, 160, config.grain_count);

    granular_engine_render(&narrow, output, 1000, &config);
    granular_engine_render_wide(&wide, wide_output, 1000, &config);
    assert(memcmp(&narrow, &wide, sizeof(narrow)) == 0);

    bool narrow_clipped = false;
    bool wide_exceeded_pcm16 = false;
    for (int frame = 0; frame < 1000; frame++) {
        narrow_clipped |= output[frame * 2] == 32767
                       || output[frame * 2 + 1] == 32767;
        wide_exceeded_pcm16 |= wide_output[frame * 2] > 32767
                           || wide_output[frame * 2 + 1] > 32767;
    }
    assert(narrow_clipped);
    assert(wide_exceeded_pcm16);
}

static void test_silent_render_preserves_engine_timing(void)
{
    GranularEngine audible;
    GranularEngine silent;
    GranularEngine silent_wide;
    GranularConfig config = default_config();
    int16_t silent_output[2048 * 2];
    int32_t silent_wide_output[2048 * 2];
    config.pitch_semitones = -12;
    config.length_ms = 500;
    config.grain_count = 16;
    config.interval_ms = 1;
    reset_engine(&audible);
    reset_engine(&silent);
    reset_engine(&silent_wide);
    granular_engine_trigger(&audible, 160, config.grain_count);
    granular_engine_trigger(&silent, 160, config.grain_count);
    granular_engine_trigger(&silent_wide, 160, config.grain_count);

    granular_engine_render(&audible, output, 2048, &config);
    memset(silent_output, 0x7f, sizeof(silent_output));
    granular_engine_render_silent(&silent, silent_output, 2048, &config);
    memset(silent_wide_output, 0x7f, sizeof(silent_wide_output));
    granular_engine_render_silent_wide(&silent_wide, silent_wide_output,
                                       2048, &config);

    assert(memcmp(&audible, &silent, sizeof(audible)) == 0);
    assert(memcmp(&audible, &silent_wide, sizeof(audible)) == 0);
    for (size_t index = 0;
            index < sizeof(silent_output) / sizeof(silent_output[0]); index++)
        assert(silent_output[index] == 0);
    for (size_t index = 0;
            index < sizeof(silent_wide_output)
                  / sizeof(silent_wide_output[0]); index++)
        assert(silent_wide_output[index] == 0);
}

int main(void)
{
    for (int index = 0; index < TEST_SAMPLE_COUNT; index++)
        sample[index] = (int16_t)(12000 + (index & 255));
    memset(output, 0, sizeof(output));
    test_contract_math();
    test_fine_tuning();
    test_sample_clock_burst();
    test_sync_and_gate_repetition();
    test_gate_release_cancels_unstarted_repeat();
    test_gate_release_finishes_current_burst();
    test_range_modes();
    test_audio_pan_and_envelope();
    test_polyphony_limit();
    test_stop_preserves_source_and_clock();
    test_minimum_declick_envelope();
    test_voice_steal_and_stop_are_declicked();
    test_wide_mix_preserves_overload_headroom();
    test_silent_render_preserves_engine_timing();
    puts("granular_engine_test: all checks passed");
    return 0;
}
