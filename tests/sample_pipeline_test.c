#include "sample_bank.h"
#include "sample_library.h"
#include "sample_player.h"
#include "waveform.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_FRAMES 4096
#define WAVEFORM_COLUMNS 320

static int16_t output[TEST_FRAMES * 2];
static int16_t minimum[WAVEFORM_COLUMNS];
static int16_t maximum[WAVEFORM_COLUMNS];

static SampleRenderConfig default_config(void)
{
    SampleRenderConfig config = {
        .start_sample = 0,
        .rate_percent = 100,
        .gain_percent = 80,
        .pan_percent = 0,
        .gate = false,
    };
    return config;
}

static void test_bank_and_waveform(const char *path, SampleBank *bank,
                                   LoadedSample *sample)
{
    assert(sample_bank_open(bank, path));
    assert(bank->sample_count == 9);
    assert(bank->sample_rate == 16384 || bank->sample_rate == 48000);
    assert(strcmp(bank->entries[0].name, "1") == 0);
    assert(sample_bank_load(bank, 0, sample));
    assert(sample->sample_count > 1000);
    assert(sample->sample_rate == bank->sample_rate);
    assert(strcmp(sample->name, "1") == 0);

    waveform_analyze(sample->samples, sample->sample_count,
                     minimum, maximum, WAVEFORM_COLUMNS);
    bool has_amplitude = false;
    for (int x = 0; x < WAVEFORM_COLUMNS; x++) {
        assert(minimum[x] <= maximum[x]);
        has_amplitude |= minimum[x] != 0 || maximum[x] != 0;
    }
    assert(has_amplitude);
}

static void test_silence_and_playback(const LoadedSample *sample)
{
    SamplePlayer player;
    SampleRenderConfig config = default_config();
    sample_player_init(&player);
    sample_player_set_sample(&player, sample->samples, sample->sample_count,
                             sample->sample_rate);

    memset(output, 1, sizeof(output));
    sample_player_render(&player, output, 512, &config);
    for (int index = 0; index < 1024; index++)
        assert(output[index] == 0);

    config.gate = true;
    sample_player_render(&player, output, 2048, &config);
    bool heard = false;
    for (int frame = 0; frame < 2048; frame++) {
        assert(output[frame * 2] == output[frame * 2 + 1]);
        heard |= output[frame * 2] != 0;
    }
    assert(heard);
    assert(player.position_q16 > 0);
}

static void test_pan_and_trigger(const LoadedSample *sample)
{
    SamplePlayer player;
    SampleRenderConfig config = default_config();
    config.gate = true;
    config.pan_percent = -100;
    config.start_sample = sample->sample_count / 3;
    sample_player_init(&player);
    sample_player_set_sample(&player, sample->samples, sample->sample_count,
                             sample->sample_rate);
    sample_player_render(&player, output, 2048, &config);

    bool heard_left = false;
    for (int frame = 0; frame < 2048; frame++) {
        heard_left |= output[frame * 2] != 0;
        assert(output[frame * 2 + 1] == 0);
    }
    assert(heard_left);

    config.gate = false;
    config.pan_percent = 0;
    sample_player_trigger_ms(&player, sample->sample_count / 2, 20);
    sample_player_render(&player, output, TEST_FRAMES, &config);
    bool triggered = false;
    for (int frame = 0; frame < 1500; frame++)
        triggered |= output[frame * 2] != 0;
    assert(triggered);
    for (int frame = TEST_FRAMES - 128; frame < TEST_FRAMES; frame++) {
        assert(output[frame * 2] == 0);
        assert(output[frame * 2 + 1] == 0);
    }
}

static void test_resident_library(const char *path)
{
    SampleBank bank;
    SampleLibrary library;
    assert(sample_bank_open(&bank, path));
    assert(sample_library_load(&bank, WAVEFORM_COLUMNS, &library));
    assert(library.sample_count == bank.sample_count);

    uint32_t expected_bytes = 0;
    for (uint32_t index = 0; index < bank.sample_count; index++)
        expected_bytes += bank.entries[index].byte_length;
    assert(library.total_pcm_bytes == expected_bytes);

    /* The live library must remain usable after all file I/O is closed. */
    sample_bank_close(&bank);
    const LoadedSample *last = sample_library_get(
        &library, library.sample_count - 1);
    assert(last != NULL && last->samples != NULL);
    assert(last->sample_count > 1000);
    assert(sample_library_copy_waveform(
        &library, library.sample_count - 1, minimum, maximum,
        WAVEFORM_COLUMNS));
    bool has_amplitude = false;
    for (int x = 0; x < WAVEFORM_COLUMNS; x++) {
        assert(minimum[x] <= maximum[x]);
        has_amplitude |= minimum[x] != 0 || maximum[x] != 0;
    }
    assert(has_amplitude);
    assert(sample_library_get(&library, library.sample_count) == NULL);
    sample_library_free(&library);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    SampleBank bank;
    LoadedSample sample;
    test_bank_and_waveform(argv[1], &bank, &sample);
    test_silence_and_playback(&sample);
    test_pan_and_trigger(&sample);
    loaded_sample_free(&sample);
    sample_bank_close(&bank);
    test_resident_library(argv[1]);
    puts("sample_pipeline_test: all checks passed");
    return 0;
}
