#include "effects_chain.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DS_EFFECTS_SAMPLE_RATE 16384
#define SOFT_LIMIT_KNEE 30000
#define SOFT_LIMIT_RANGE (INT16_MAX - SOFT_LIMIT_KNEE)

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static int16_t clamp_sample(int64_t value)
{
    if (value < INT16_MIN)
        return INT16_MIN;
    if (value > INT16_MAX)
        return INT16_MAX;
    return (int16_t)value;
}

static int16_t soft_limit_sample(int64_t value, bool *overloaded)
{
    bool negative = value < 0;
    uint64_t magnitude = negative
        ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    if (value < INT16_MIN || value > INT16_MAX)
        *overloaded = true;
    if (magnitude <= SOFT_LIMIT_KNEE)
        return (int16_t)value;

    uint64_t excess = magnitude - SOFT_LIMIT_KNEE;
    uint64_t compressed = SOFT_LIMIT_KNEE
        + (uint64_t)SOFT_LIMIT_RANGE * excess
        / (excess + SOFT_LIMIT_RANGE);
    if (compressed > INT16_MAX)
        compressed = INT16_MAX;
    return (int16_t)(negative ? -(int64_t)compressed
                              : (int64_t)compressed);
}

static int32_t limit_input(EffectsChain *chain, int64_t value)
{
    bool overloaded = false;
    int16_t limited = soft_limit_sample(value, &overloaded);
    if (overloaded) {
        chain->block_overloaded = true;
        chain->input_overload_events++;
    }
    return limited;
}

static int32_t limit_fdn(EffectsChain *chain, int64_t value, bool freeze)
{
    if (freeze) {
        bool overloaded = value < INT16_MIN || value > INT16_MAX;
        if (overloaded) {
            chain->block_overloaded = true;
            chain->fdn_overload_events++;
        }
        return clamp_sample(value);
    }

    bool overloaded = false;
    int16_t limited = soft_limit_sample(value, &overloaded);
    if (overloaded) {
        chain->block_overloaded = true;
        chain->fdn_overload_events++;
    }
    return limited;
}

int effects_lowpass_alpha_q15(int cutoff_hz, int sample_rate)
{
    cutoff_hz = clamp_int(cutoff_hz, 1, sample_rate / 2 - 1);
    int omega_q15 = (int)(((int64_t)cutoff_hz * 205887) / sample_rate);
    return (int)(((int64_t)omega_q15 << 15) / (32768 + omega_q15));
}

int effects_highpass_alpha_q15(int cutoff_hz, int sample_rate)
{
    cutoff_hz = clamp_int(cutoff_hz, 1, sample_rate / 2 - 1);
    int omega_q15 = (int)(((int64_t)cutoff_hz * 205887) / sample_rate);
    return (int)(((int64_t)32768 << 15) / (32768 + omega_q15));
}

int effects_damping_alpha_q15(int damping_percent, int sample_rate)
{
    damping_percent = clamp_int(damping_percent, 0, 100);
    sample_rate = clamp_int(sample_rate, 1, 192000);
    double ds_alpha = (32767.0 - damping_percent * 260.0) / 32768.0;
    double exponent = (double)DS_EFFECTS_SAMPLE_RATE / sample_rate;
    double native_alpha = 1.0 - pow(1.0 - ds_alpha, exponent);
    int q15 = (int)lround(native_alpha * 32768.0);
    return clamp_int(q15, 1, 32767);
}

uint32_t effects_phaser_increment(int speed_hundredths_hz, int sample_rate)
{
    speed_hundredths_hz = clamp_int(speed_hundredths_hz, 1, 100);
    sample_rate = clamp_int(sample_rate, 1, 192000);
    return (uint32_t)((UINT64_C(1) << 32)
        * (uint32_t)speed_hundredths_hz
        / ((uint64_t)sample_rate * 100));
}

static int phaser_coefficient_q15(uint32_t phase)
{
    uint32_t triangle = phase < UINT32_C(0x80000000)
        ? phase >> 15 : (UINT32_MAX - phase) >> 15;
    return 8192 + (int)((uint64_t)triangle * 16384 >> 16);
}

static int32_t process_allpass(EffectsChain *chain, int channel,
                               int32_t input, int coefficient_q15)
{
    int32_t value = input;
    for (int stage = 0; stage < EFFECTS_PHASER_STAGES; stage++) {
        EffectsAllpassState *state = &chain->phaser[channel][stage];
        int64_t numerator = -(int64_t)coefficient_q15 * value
                          + (int64_t)state->previous_input * 32768
                          + (int64_t)coefficient_q15
                          * state->previous_output;
        int32_t output = (int32_t)(numerator >> 15);
        state->previous_input = value;
        state->previous_output = output;
        value = output;
    }
    return value;
}

static int32_t process_phaser(EffectsChain *chain, int channel,
                              int32_t input, int coefficient_q15, int depth)
{
    int32_t phased = process_allpass(chain, channel, input,
                                     coefficient_q15);
    int32_t notched = (input + phased) / 2;
    return (input * (100 - depth) + notched * depth) / 100;
}

static int16_t *delay_cell(EffectsChain *chain, int line, int position)
{
    return chain->delay + line * EFFECTS_FDN_MAX_DELAY + position;
}

static void configure(EffectsChain *chain, const EffectsConfig *config)
{
    int size = clamp_int(config->size_percent, 0, 100);
    if (size != chain->configured_size) {
        static const int ds_base_lengths[EFFECTS_FDN_LINES] = {
            421, 613, 809, 1013
        };
        uint64_t scale_q16;
        if (size <= 55) {
            scale_q16 = (uint64_t)(55 + size) << 16;
        } else {
            uint64_t above_default = (uint64_t)(size - 55);
            scale_q16 = UINT64_C(110) << 16;
            scale_q16 += ((UINT64_C(690) << 16)
                        * above_default * above_default) / (45 * 45);
        }
        for (int line = 0; line < EFFECTS_FDN_LINES; line++) {
            int native_base = (ds_base_lengths[line] * EFFECTS_SAMPLE_RATE
                             + DS_EFFECTS_SAMPLE_RATE / 2)
                            / DS_EFFECTS_SAMPLE_RATE;
            int length = (int)((uint64_t)native_base * scale_q16
                             / (UINT64_C(100) << 16));
            chain->lengths[line] = clamp_int(
                length, 64, EFFECTS_FDN_MAX_DELAY);
            if (chain->positions[line] >= chain->lengths[line])
                chain->positions[line] = 0;
        }
        chain->configured_size = size;
    }

    int damping = clamp_int(config->damping_percent, 0, 100);
    if (damping != chain->configured_damping) {
        chain->damping_q15 = effects_damping_alpha_q15(
            damping, EFFECTS_SAMPLE_RATE);
        chain->configured_damping = damping;
    }
    chain->feedback_q15 = config->freeze ? 32768
        : clamp_int(config->feedback_tenths_percent, 0, 999) * 32767 / 1000;
    chain->highpass_alpha_q15 = effects_highpass_alpha_q15(
        config->highpass_hz > 0 ? config->highpass_hz : 1,
        EFFECTS_SAMPLE_RATE);
    chain->lowpass_alpha_q15 = effects_lowpass_alpha_q15(
        config->lowpass_hz, EFFECTS_SAMPLE_RATE);
}

static int32_t process_filter(EffectsFilterState *state, int32_t input,
                              const EffectsChain *chain,
                              const EffectsConfig *config)
{
    int32_t filtered = input;
    if (config->highpass_hz > 0) {
        int64_t difference = (int64_t)state->highpass_output + input
                           - state->highpass_previous_input;
        state->highpass_output = (int32_t)(difference
                                      * chain->highpass_alpha_q15 >> 15);
        state->highpass_previous_input = input;
        filtered = state->highpass_output;
    } else {
        state->highpass_previous_input = input;
        state->highpass_output = 0;
    }

    if (config->lowpass_hz < 8000) {
        state->lowpass_output += (int32_t)(((int64_t)(filtered
            - state->lowpass_output) * chain->lowpass_alpha_q15) >> 15);
        filtered = state->lowpass_output;
    } else {
        state->lowpass_output = filtered;
    }
    return filtered;
}

bool effects_chain_init(EffectsChain *chain)
{
    memset(chain, 0, sizeof(*chain));
    chain->delay = calloc(EFFECTS_FDN_LINES * EFFECTS_FDN_MAX_DELAY,
                          sizeof(*chain->delay));
    if (chain->delay == NULL)
        return false;
    chain->configured_size = -1;
    chain->configured_damping = -1;
    chain->initialized = true;
    return true;
}

void effects_chain_reset(EffectsChain *chain)
{
    if (!chain->initialized)
        return;
    memset(chain->delay, 0, EFFECTS_FDN_LINES * EFFECTS_FDN_MAX_DELAY
                              * sizeof(*chain->delay));
    memset(chain->positions, 0, sizeof(chain->positions));
    memset(chain->damped, 0, sizeof(chain->damped));
    memset(chain->phaser, 0, sizeof(chain->phaser));
    chain->phaser_phase = 0;
    memset(chain->filters, 0, sizeof(chain->filters));
    chain->input_overload_events = 0;
    chain->fdn_overload_events = 0;
    chain->block_overloaded = false;
}

static void process(EffectsChain *chain, const int16_t *narrow_input,
                    const int32_t *wide_input, int16_t *output,
                    size_t frames, const EffectsConfig *config)
{
    if (!chain->initialized)
        return;
    chain->block_overloaded = false;
    configure(chain, config);
    int wet = clamp_int(config->wet_percent, 0, 100);
    int phaser_depth = clamp_int(config->phaser_depth_percent, 0, 100);
    uint32_t phaser_increment = effects_phaser_increment(
        config->phaser_speed_hundredths_hz, EFFECTS_SAMPLE_RATE);

    for (size_t frame = 0; frame < frames; frame++) {
        int64_t raw_left = wide_input != NULL
            ? wide_input[frame * 2] : narrow_input[frame * 2];
        int64_t raw_right = wide_input != NULL
            ? wide_input[frame * 2 + 1] : narrow_input[frame * 2 + 1];
        int32_t dry_left = limit_input(chain, raw_left);
        int32_t dry_right = limit_input(chain, raw_right);
        int32_t source_left = dry_left;
        int32_t source_right = dry_right;
        if (phaser_depth > 0) {
            int left_coefficient = phaser_coefficient_q15(
                chain->phaser_phase);
            int right_coefficient = phaser_coefficient_q15(
                chain->phaser_phase + UINT32_C(0x40000000));
            source_left = process_phaser(chain, 0, dry_left,
                                         left_coefficient, phaser_depth);
            source_right = process_phaser(chain, 1, dry_right,
                                          right_coefficient, phaser_depth);
            chain->phaser_phase += phaser_increment;
        }
        int32_t input[EFFECTS_FDN_LINES] = { 0, 0, 0, 0 };
        if (!config->freeze) {
            input[0] = (source_left + source_right) >> 2;
            input[1] = (source_left - source_right) >> 2;
            input[2] = (source_left + source_right) >> 2;
            input[3] = (-source_left + source_right) >> 2;
        }

        int32_t delayed[EFFECTS_FDN_LINES];
        for (int line = 0; line < EFFECTS_FDN_LINES; line++) {
            if (chain->positions[line] >= chain->lengths[line])
                chain->positions[line] = 0;
            delayed[line] = *delay_cell(chain, line, chain->positions[line]);
        }

        int32_t matrix[EFFECTS_FDN_LINES] = {
            (delayed[0] + delayed[1] + delayed[2] + delayed[3]) >> 1,
            (delayed[0] - delayed[1] + delayed[2] - delayed[3]) >> 1,
            (delayed[0] + delayed[1] - delayed[2] - delayed[3]) >> 1,
            (delayed[0] - delayed[1] - delayed[2] + delayed[3]) >> 1,
        };

        for (int line = 0; line < EFFECTS_FDN_LINES; line++) {
            int32_t target = limit_fdn(chain, matrix[line], config->freeze);
            if (config->freeze)
                chain->damped[line] = target;
            else
                chain->damped[line] += (int32_t)(((int64_t)(target
                    - chain->damped[line]) * chain->damping_q15) >> 15);
            int32_t feedback = (int32_t)((int64_t)chain->damped[line]
                                       * chain->feedback_q15 >> 15);
            *delay_cell(chain, line, chain->positions[line]) = (int16_t)
                limit_fdn(chain, (int64_t)input[line] + feedback,
                          config->freeze);
            chain->positions[line]++;
        }

        int32_t wet_left = (delayed[0] + delayed[1]
                          + delayed[2] - delayed[3]) >> 2;
        int32_t wet_right = (delayed[0] - delayed[1]
                           + delayed[2] + delayed[3]) >> 2;
        int32_t mixed_left = (source_left * (100 - wet)
                            + wet_left * wet) / 100;
        int32_t mixed_right = (source_right * (100 - wet)
                             + wet_right * wet) / 100;
        mixed_left = process_filter(&chain->filters[0], mixed_left,
                                    chain, config);
        mixed_right = process_filter(&chain->filters[1], mixed_right,
                                     chain, config);
        output[frame * 2] = (int16_t)limit_fdn(
            chain, mixed_left, config->freeze);
        output[frame * 2 + 1] = (int16_t)limit_fdn(
            chain, mixed_right, config->freeze);
    }
}

void effects_chain_process(EffectsChain *chain, int16_t *samples,
                           size_t frames, const EffectsConfig *config)
{
    process(chain, samples, NULL, samples, frames, config);
}

void effects_chain_process_wide(EffectsChain *chain,
                                const int32_t *input, int16_t *output,
                                size_t frames, const EffectsConfig *config)
{
    process(chain, NULL, input, output, frames, config);
}

bool effects_chain_overloaded(const EffectsChain *chain)
{
    return chain->block_overloaded;
}

void effects_chain_exit(EffectsChain *chain)
{
    free(chain->delay);
    memset(chain, 0, sizeof(*chain));
}
