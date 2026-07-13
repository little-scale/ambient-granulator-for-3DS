#include "sample_player.h"

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

static int16_t clamp_sample(int64_t sample)
{
    if (sample < INT16_MIN)
        return INT16_MIN;
    if (sample > INT16_MAX)
        return INT16_MAX;
    return (int16_t)sample;
}

static uint32_t clamp_start(const SamplePlayer *player, uint32_t start)
{
    if (player->sample_count < 2)
        return 0;
    if (start >= player->sample_count - 1)
        return player->sample_count - 2;
    return start;
}

static void restart(SamplePlayer *player, uint32_t start)
{
    player->last_start_sample = clamp_start(player, start);
    player->position_q16 = (uint64_t)player->last_start_sample << 16;
    player->envelope_q15 = 0;
}

void sample_player_init(SamplePlayer *player)
{
    memset(player, 0, sizeof(*player));
}

void sample_player_set_sample(SamplePlayer *player, const int16_t *samples,
                              uint32_t sample_count, uint32_t source_rate)
{
    player->samples = samples;
    player->sample_count = sample_count;
    player->source_rate = source_rate;
    player->trigger_frames = 0;
    player->previous_gate = false;
    restart(player, 0);
}

void sample_player_trigger_ms(SamplePlayer *player, uint32_t start_sample,
                              unsigned int milliseconds)
{
    uint64_t frames = (uint64_t)SAMPLE_PLAYER_OUTPUT_RATE
                    * milliseconds / 1000;
    if (frames < 1)
        frames = 1;
    if (frames > UINT32_MAX)
        frames = UINT32_MAX;
    restart(player, start_sample);
    player->trigger_frames = (uint32_t)frames;
}

void sample_player_render(SamplePlayer *player, int16_t *output,
                          size_t frames, const SampleRenderConfig *config)
{
    int gain_percent = clamp_int(config->gain_percent, 0, 100);
    int pan_percent = clamp_int(config->pan_percent, -100, 100);
    int rate_percent = clamp_int(config->rate_percent, 25, 400);
    int left_pan = pan_percent <= 0 ? 100 : 100 - pan_percent;
    int right_pan = pan_percent >= 0 ? 100 : 100 + pan_percent;
    uint32_t requested_start = clamp_start(player, config->start_sample);
    uint64_t step_q16 = 0;
    if (player->source_rate > 0) {
        step_q16 = ((uint64_t)player->source_rate * rate_percent << 16)
                 / ((uint64_t)SAMPLE_PLAYER_OUTPUT_RATE * 100);
    }
    if (step_q16 < 1)
        step_q16 = 1;

    if (config->gate && (!player->previous_gate
            || requested_start != player->last_start_sample))
        restart(player, requested_start);
    player->previous_gate = config->gate;

    for (size_t frame = 0; frame < frames; frame++) {
        bool has_sample = player->samples != NULL
                       && player->sample_count >= 2;
        bool sounding = has_sample
                     && (config->gate || player->trigger_frames > 0);
        if (sounding) {
            player->envelope_q15 += 192;
            if (player->envelope_q15 > 32767)
                player->envelope_q15 = 32767;
        } else {
            player->envelope_q15 -= 128;
            if (player->envelope_q15 < 0)
                player->envelope_q15 = 0;
        }

        if (player->trigger_frames > 0)
            player->trigger_frames--;

        int32_t mono = 0;
        if (has_sample && player->envelope_q15 > 0) {
            uint32_t index = (uint32_t)(player->position_q16 >> 16);
            if (index >= player->sample_count - 1) {
                if (config->gate) {
                    player->position_q16
                        = (uint64_t)requested_start << 16;
                    index = requested_start;
                } else {
                    player->trigger_frames = 0;
                    player->envelope_q15 = 0;
                }
            }
            if (player->envelope_q15 > 0) {
                uint32_t fraction = (uint32_t)player->position_q16 & 0xFFFFU;
                int32_t first = player->samples[index];
                int32_t second = player->samples[index + 1];
                mono = first + (int32_t)(((int64_t)(second - first)
                                         * fraction) >> 16);
                player->position_q16 += step_q16;
            }
        }

        int64_t scaled = (int64_t)mono * player->envelope_q15
                       * gain_percent;
        scaled /= (int64_t)32767 * 100;
        output[frame * 2] = clamp_sample(scaled * left_pan / 100);
        output[frame * 2 + 1] = clamp_sample(scaled * right_pan / 100);
    }
}
