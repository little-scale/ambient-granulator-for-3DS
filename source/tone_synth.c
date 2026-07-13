#include "tone_synth.h"

#include <limits.h>
#include <string.h>

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}
static int32_t triangle_sample(uint32_t phase)
{
    uint32_t position = phase >> 16;
    if (position < 16384)
        return (int32_t)(position * 2);
    if (position < 49152)
        return 32767 - (int32_t)((position - 16384) * 2);
    return -32768 + (int32_t)((position - 49152) * 2);
}

static uint32_t phase_step(int frequency_hz)
{
    int frequency = clamp_int(frequency_hz, 1,
                              TONE_SYNTH_SAMPLE_RATE / 2);
    return (uint32_t)(((uint64_t)(unsigned int)frequency << 32)
                    / TONE_SYNTH_SAMPLE_RATE);
}

static int16_t clamp_sample(int64_t sample)
{
    if (sample < INT16_MIN)
        return INT16_MIN;
    if (sample > INT16_MAX)
        return INT16_MAX;
    return (int16_t)sample;
}

void tone_synth_init(ToneSynth *synth)
{
    memset(synth, 0, sizeof(*synth));
}

void tone_synth_trigger_ms(ToneSynth *synth, unsigned int milliseconds)
{
    uint64_t frames = (uint64_t)TONE_SYNTH_SAMPLE_RATE * milliseconds / 1000;
    if (frames < 1)
        frames = 1;
    if (frames > UINT32_MAX)
        frames = UINT32_MAX;
    synth->trigger_frames = (uint32_t)frames;
}

void tone_synth_render(ToneSynth *synth, int16_t *output, size_t frames,
                       const ToneRenderConfig *config)
{
    int gain_percent = clamp_int(config->gain_percent, 0, 100);
    int pan_percent = clamp_int(config->pan_percent, -100, 100);
    int left_pan = pan_percent <= 0 ? 100 : 100 - pan_percent;
    int right_pan = pan_percent >= 0 ? 100 : 100 + pan_percent;
    uint32_t mono_step = phase_step(config->frequency_hz);
    uint32_t left_step = config->mode == TONE_MODE_STEREO_ID
        ? phase_step(440) : mono_step;
    uint32_t right_step = config->mode == TONE_MODE_STEREO_ID
        ? phase_step(660) : mono_step;
    const int attack_step = 96;
    const int release_step = 64;

    for (size_t frame = 0; frame < frames; frame++) {
        bool sounding = config->gate || synth->trigger_frames > 0;
        if (sounding) {
            synth->envelope_q15 += attack_step;
            if (synth->envelope_q15 > 32767)
                synth->envelope_q15 = 32767;
        } else {
            synth->envelope_q15 -= release_step;
            if (synth->envelope_q15 < 0)
                synth->envelope_q15 = 0;
        }

        if (synth->trigger_frames > 0)
            synth->trigger_frames--;

        int32_t left_wave = triangle_sample(synth->phase_left);
        int32_t right_wave = config->mode == TONE_MODE_STEREO_ID
            ? triangle_sample(synth->phase_right) : left_wave;
        synth->phase_left += left_step;
        synth->phase_right += right_step;

        int64_t left = (int64_t)left_wave * synth->envelope_q15
                     * gain_percent;
        int64_t right = (int64_t)right_wave * synth->envelope_q15
                      * gain_percent;
        left /= (int64_t)32767 * 100;
        right /= (int64_t)32767 * 100;

        if (config->mode == TONE_MODE_MONO_PAN) {
            left = left * left_pan / 100;
            right = right * right_pan / 100;
        }

        output[frame * 2] = clamp_sample(left);
        output[frame * 2 + 1] = clamp_sample(right);
    }
}
