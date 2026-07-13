#include "tone_synth.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_FRAMES 4096

static int16_t output[TEST_FRAMES * 2];

static ToneRenderConfig default_config(void)
{
    ToneRenderConfig config = {
        .frequency_hz = 440,
        .gain_percent = 50,
        .pan_percent = 0,
        .mode = TONE_MODE_MONO_PAN,
        .gate = false,
    };
    return config;
}
static void test_silence_without_gate(void)
{
    ToneSynth synth;
    ToneRenderConfig config = default_config();
    tone_synth_init(&synth);
    memset(output, 1, sizeof(output));
    tone_synth_render(&synth, output, 512, &config);
    for (int i = 0; i < 1024; i++)
        assert(output[i] == 0);
}

static void test_centered_gate(void)
{
    ToneSynth synth;
    ToneRenderConfig config = default_config();
    config.gate = true;
    tone_synth_init(&synth);
    tone_synth_render(&synth, output, 1024, &config);

    bool heard = false;
    for (int frame = 0; frame < 1024; frame++) {
        assert(output[frame * 2] == output[frame * 2 + 1]);
        if (output[frame * 2] != 0)
            heard = true;
    }
    assert(heard);
}

static void test_hard_pan(void)
{
    ToneSynth synth;
    ToneRenderConfig config = default_config();
    config.gate = true;
    config.pan_percent = -100;
    tone_synth_init(&synth);
    tone_synth_render(&synth, output, 1024, &config);

    bool heard_left = false;
    for (int frame = 0; frame < 1024; frame++) {
        if (output[frame * 2] != 0)
            heard_left = true;
        assert(output[frame * 2 + 1] == 0);
    }
    assert(heard_left);
}

static void test_timed_trigger_releases(void)
{
    ToneSynth synth;
    ToneRenderConfig config = default_config();
    tone_synth_init(&synth);
    tone_synth_trigger_ms(&synth, 20);
    tone_synth_render(&synth, output, TEST_FRAMES, &config);

    bool heard = false;
    for (int frame = 0; frame < 1500; frame++) {
        if (output[frame * 2] != 0)
            heard = true;
    }
    assert(heard);
    for (int frame = TEST_FRAMES - 256; frame < TEST_FRAMES; frame++) {
        assert(output[frame * 2] == 0);
        assert(output[frame * 2 + 1] == 0);
    }
}

static void test_stereo_identification_mode(void)
{
    ToneSynth synth;
    ToneRenderConfig config = default_config();
    config.gate = true;
    config.mode = TONE_MODE_STEREO_ID;
    tone_synth_init(&synth);
    tone_synth_render(&synth, output, 2048, &config);

    bool left_heard = false;
    bool right_heard = false;
    bool channels_differ = false;
    for (int frame = 0; frame < 2048; frame++) {
        left_heard |= output[frame * 2] != 0;
        right_heard |= output[frame * 2 + 1] != 0;
        channels_differ |= output[frame * 2] != output[frame * 2 + 1];
    }
    assert(left_heard);
    assert(right_heard);
    assert(channels_differ);
}

int main(void)
{
    test_silence_without_gate();
    test_centered_gate();
    test_hard_pan();
    test_timed_trigger_releases();
    test_stereo_identification_mode();
    puts("tone_synth_test: all checks passed");
    return 0;
}
