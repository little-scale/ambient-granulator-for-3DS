#include "granular_engine.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#define GRANULAR_SCREEN_WIDTH 320
#define GRANULAR_MAX_LENGTH_MS 500

static const int division_denominators[] = { 8, 16, 32, 64 };

/* Q14 semitone ratios from -24 through +24, retained from the DS engine. */
static const uint32_t pitch_ratios_q14[] = {
    4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889,
    7298, 7732, 8192, 8679, 9195, 9742, 10322, 10935, 11585, 12274,
    13004, 13777, 14596, 15464, 16384, 17358, 18390, 19484, 20643,
    21871, 23170, 24548, 26008, 27554, 29193, 30929, 32768, 34716,
    36781, 38968, 41285, 43742, 46341, 49097, 52016, 55109, 58386,
    61858, 65536,
};

static const int gain_q12[] = {
    258, 290, 325, 365, 410, 460, 516, 579, 649, 728, 817,
    917, 1029, 1154, 1295, 1453, 1631, 1830, 2053, 2303, 2584,
    2900, 3254, 3651, 4096, 4596, 5157, 5786, 6492, 7284, 8173,
    9170, 10289, 11544, 12953, 14533, 16306, 18296, 20529,
    23034, 25844, 28997, 32536,
};

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

static int32_t clamp_wide_sample(int64_t value)
{
    if (value < INT32_MIN)
        return INT32_MIN;
    if (value > INT32_MAX)
        return INT32_MAX;
    return (int32_t)value;
}

int granular_interval_frames(int output_rate, bool synced, int bpm,
                             int denominator, int interval_ms)
{
    int64_t numerator;
    int divisor;
    if (synced) {
        numerator = (int64_t)output_rate * 240;
        divisor = clamp_int(bpm, 1, 1000) * clamp_int(denominator, 1, 256);
    } else {
        numerator = (int64_t)output_rate * clamp_int(interval_ms, 1, 60000);
        divisor = 1000;
    }
    int frames = (int)((numerator + divisor / 2) / divisor);
    return frames < 1 ? 1 : frames;
}

int granular_range_offset(int range, uint32_t random_value)
{
    range = clamp_int(range, -128, 128);
    if (range >= 0)
        return (int)(random_value % (uint32_t)(range * 2 + 1)) - range;
    int magnitude = -range;
    return (int)(random_value % (uint32_t)(magnitude + 1));
}

uint32_t granular_pitch_step_q16(uint32_t source_rate, int output_rate,
                                 int semitones)
{
    semitones = clamp_int(semitones, -24, 24);
    if (source_rate == 0 || output_rate < 1)
        return 1;
    uint64_t ratio_q16 = (uint64_t)pitch_ratios_q14[semitones + 24] * 4;
    uint64_t step = ratio_q16 * source_rate / (uint32_t)output_rate;
    if (step < 1)
        step = 1;
    if (step > UINT32_MAX)
        step = UINT32_MAX;
    return (uint32_t)step;
}

uint32_t granular_pitch_step_fine_q16(uint32_t source_rate, int output_rate,
                                      int semitones, int cents)
{
    semitones = clamp_int(semitones, -24, 24);
    cents = clamp_int(cents, -1200, 1200);
    if (cents == 0)
        return granular_pitch_step_q16(source_rate, output_rate, semitones);
    if (source_rate == 0 || output_rate < 1)
        return 1;
    double pitch = (double)semitones + (double)cents / 100.0;
    double ratio = pow(2.0, pitch / 12.0);
    double step = ratio * (double)source_rate * 65536.0
                / (double)output_rate;
    if (step < 1.0)
        return 1;
    if (step > (double)UINT32_MAX)
        return UINT32_MAX;
    return (uint32_t)lround(step);
}

static uint32_t next_random(GranularEngine *engine)
{
    engine->random_state ^= engine->random_state << 13;
    engine->random_state ^= engine->random_state >> 17;
    engine->random_state ^= engine->random_state << 5;
    return engine->random_state;
}

static int random_signed(GranularEngine *engine, int amplitude)
{
    amplitude = clamp_int(amplitude, 0, 1000000);
    if (amplitude == 0)
        return 0;
    return (int)(next_random(engine) % (uint32_t)(amplitude * 2 + 1))
         - amplitude;
}

static void queue_marker(GranularEngine *engine, int x)
{
    uint8_t next = (uint8_t)((engine->marker_write + 1)
                           % GRANULAR_MARKER_QUEUE_SIZE);
    if (next == engine->marker_read)
        return;
    engine->markers[engine->marker_write].x = x;
    engine->markers[engine->marker_write].frame = engine->rendered_frames;
    engine->marker_write = next;
}

static int voice_limit(const GranularConfig *config)
{
    return clamp_int(config->voice_limit, 1, GRANULAR_VOICE_COUNT);
}

static GranularVoice *select_voice(GranularEngine *engine,
                                   const GranularConfig *config)
{
    int limit = voice_limit(config);
    int first = engine->next_voice % limit;
    for (int offset = 0; offset < limit; offset++) {
        int index = (first + offset) % limit;
        if (!engine->voices[index].active) {
            engine->next_voice = (index + 1) % limit;
            return &engine->voices[index];
        }
    }

    /* All slots are busy: replace the grain with least audible time left. */
    int best = first;
    uint64_t shortest = UINT64_MAX;
    for (int index = 0; index < limit; index++) {
        GranularVoice *voice = &engine->voices[index];
        uint64_t end_q16 = (uint64_t)voice->length << 16;
        uint64_t remaining_q16 = voice->position_q16 < end_q16
                               ? end_q16 - voice->position_q16 : 0;
        uint64_t remaining_frames = remaining_q16 / voice->step_q16;
        if (remaining_frames < shortest) {
            best = index;
            shortest = remaining_frames;
        }
    }
    engine->next_voice = (best + 1) % limit;
    return &engine->voices[best];
}

static void launch_grain(GranularEngine *engine, const GranularConfig *config,
                         int x, int pitch, int fine_cents)
{
    if (engine->sample == NULL || engine->sample_count < 2
            || engine->source_rate == 0)
        return;

    uint32_t length = (uint32_t)((uint64_t)engine->source_rate
                              * clamp_int(config->length_ms, 1,
                                          GRANULAR_MAX_LENGTH_MS)
                              / 1000);
    if (length < 2)
        length = 2;
    if (length > engine->sample_count)
        length = engine->sample_count;

    x = clamp_int(x, 0, GRANULAR_SCREEN_WIDTH - 1);
    uint32_t start = (uint32_t)((uint64_t)x * (engine->sample_count - 1)
                              / (GRANULAR_SCREEN_WIDTH - 1));
    if (start + length > engine->sample_count)
        start = engine->sample_count - length;
    start &= ~UINT32_C(1);

    int pan = clamp_int(config->pan_percent
                      + random_signed(engine, config->pan_divergence),
                        -100, 100);
    int left_pan = pan <= 0 ? 100 : 100 - pan;
    int right_pan = pan >= 0 ? 100 : 100 + pan;
    int volume = clamp_int(config->volume_percent, 0, 100);
    int db = clamp_int(config->gain_db, -24, 18);
    int left_q8 = volume * left_pan * 256 / 10000;
    int right_q8 = volume * right_pan * 256 / 10000;

    GranularVoice *voice = select_voice(engine, config);
    memset(voice, 0, sizeof(*voice));
    voice->source = engine->sample + start;
    voice->step_q16 = granular_pitch_step_fine_q16(
        engine->source_rate, GRANULAR_OUTPUT_RATE, pitch, fine_cents);
    voice->length = length;
    voice->attack_samples = length
        * (uint32_t)clamp_int(config->attack_percent, 0, 50) / 100;
    voice->release_samples = length
        * (uint32_t)clamp_int(config->release_percent, 0, 50) / 100;
    voice->left_gain_q8 = left_q8 * gain_q12[db + 24] / 4096;
    voice->right_gain_q8 = right_q8 * gain_q12[db + 24] / 4096;
    voice->active = true;
    queue_marker(engine, x);
    engine->grains_launched++;
}

static void update_scheduler(GranularEngine *engine,
                             const GranularConfig *config)
{
    if (config->gate)
        engine->burst_center = clamp_int(config->center_x, 0,
                                         GRANULAR_SCREEN_WIDTH - 1);
    if (engine->burst_remaining <= 0)
        return;

    if (engine->frames_until_grain > 0) {
        engine->frames_until_grain--;
        if (engine->frames_until_grain > 0)
            return;
    }

    int x = engine->burst_center
          + granular_range_offset(config->range, next_random(engine));
    x = clamp_int(x, 0, GRANULAR_SCREEN_WIDTH - 1);
    int pitch = config->pitch_semitones
              + random_signed(engine, config->pitch_deviation);
    int fine_cents = config->fine_cents
                   + random_signed(engine, config->fine_deviation_cents);
    launch_grain(engine, config, x, pitch, fine_cents);
    engine->burst_remaining--;

    if (engine->burst_remaining <= 0 && config->gate)
        engine->burst_remaining = clamp_int(config->grain_count, 1, 32);

    if (engine->burst_remaining > 0) {
        int division = clamp_int(config->division, 0, 3);
        int base = granular_interval_frames(
            GRANULAR_OUTPUT_RATE, config->clock_sync,
            config->bpm, division_denominators[division],
            config->interval_ms);
        int jitter = base * clamp_int(config->jitter_percent, 0, 100) / 100;
        int delay = base + random_signed(engine, jitter);
        engine->frames_until_grain = delay < 1 ? 1 : delay;
    }
}

void granular_engine_init(GranularEngine *engine)
{
    memset(engine, 0, sizeof(*engine));
    engine->random_state = UINT32_C(0x6D2B79F5);
}

void granular_engine_set_sample(GranularEngine *engine,
                                const int16_t *sample,
                                uint32_t sample_count,
                                uint32_t source_rate)
{
    engine->sample = sample;
    engine->sample_count = sample_count;
    engine->source_rate = source_rate;
    memset(engine->voices, 0, sizeof(engine->voices));
    engine->burst_remaining = 0;
    engine->frames_until_grain = 0;
    engine->previous_gate = false;
    engine->marker_read = 0;
    engine->marker_write = 0;
}

void granular_engine_trigger(GranularEngine *engine, int center_x,
                             int grain_count)
{
    engine->burst_center = clamp_int(center_x, 0,
                                     GRANULAR_SCREEN_WIDTH - 1);
    engine->burst_remaining = clamp_int(grain_count, 1, 32);
    engine->frames_until_grain = 0;
    engine->random_state ^= ((uint32_t)engine->burst_center << 16)
                          ^ engine->grains_launched;
}

static int prepare_render(GranularEngine *engine,
                          const GranularConfig *config)
{
    int limit = voice_limit(config);
    for (int index = limit; index < GRANULAR_VOICE_COUNT; index++)
        engine->voices[index].active = false;
    if (engine->next_voice >= limit)
        engine->next_voice = 0;

    if (config->gate && !engine->previous_gate)
        granular_engine_trigger(engine, config->center_x,
                                config->grain_count);
    engine->previous_gate = config->gate;
    return limit;
}

static void render_frame(GranularEngine *engine,
                         const GranularConfig *config, int limit,
                         int64_t *left, int64_t *right)
{
    update_scheduler(engine, config);
    *left = 0;
    *right = 0;
    for (int index = 0; index < limit; index++) {
        GranularVoice *voice = &engine->voices[index];
        if (!voice->active)
            continue;

        uint32_t sample_index = (uint32_t)(voice->position_q16 >> 16);
        if (sample_index >= voice->length) {
            voice->active = false;
            continue;
        }
        uint32_t next_index = sample_index + 1;
        if (next_index >= voice->length)
            next_index = sample_index;
        uint32_t fraction = (uint32_t)voice->position_q16 & 0xFFFFU;
        int32_t first = voice->source[sample_index];
        int32_t second = voice->source[next_index];
        int32_t value = first + (int32_t)(((int64_t)(second - first)
                                         * fraction) >> 16);

        int32_t envelope = 32767;
        if (voice->attack_samples > 0
                && sample_index < voice->attack_samples)
            envelope = (int32_t)((uint64_t)sample_index * 32767
                               / voice->attack_samples);
        uint32_t from_end = voice->length - 1 - sample_index;
        if (voice->release_samples > 0
                && from_end < voice->release_samples) {
            int32_t release = (int32_t)((uint64_t)from_end * 32767
                                      / voice->release_samples);
            if (release < envelope)
                envelope = release;
        }

        value = (int32_t)((int64_t)value * envelope / 32767);
        *left += (int64_t)value * voice->left_gain_q8 / 256;
        *right += (int64_t)value * voice->right_gain_q8 / 256;
        voice->position_q16 += voice->step_q16;
    }
    engine->rendered_frames++;
}

void granular_engine_render(GranularEngine *engine, int16_t *output,
                            size_t frames, const GranularConfig *config)
{
    int limit = prepare_render(engine, config);
    for (size_t frame = 0; frame < frames; frame++) {
        int64_t left;
        int64_t right;
        render_frame(engine, config, limit, &left, &right);
        output[frame * 2] = clamp_sample(left);
        output[frame * 2 + 1] = clamp_sample(right);
    }
}

void granular_engine_render_wide(GranularEngine *engine, int32_t *output,
                                 size_t frames,
                                 const GranularConfig *config)
{
    int limit = prepare_render(engine, config);
    for (size_t frame = 0; frame < frames; frame++) {
        int64_t left;
        int64_t right;
        render_frame(engine, config, limit, &left, &right);
        output[frame * 2] = clamp_wide_sample(left);
        output[frame * 2 + 1] = clamp_wide_sample(right);
    }
}

static void render_silent_frames(GranularEngine *engine, size_t frames,
                                 const GranularConfig *config, int limit)
{
    for (size_t frame = 0; frame < frames; frame++) {
        update_scheduler(engine, config);
        for (int index = 0; index < limit; index++) {
            GranularVoice *voice = &engine->voices[index];
            if (!voice->active)
                continue;
            uint32_t sample_index = (uint32_t)(voice->position_q16 >> 16);
            if (sample_index >= voice->length) {
                voice->active = false;
                continue;
            }
            voice->position_q16 += voice->step_q16;
        }
        engine->rendered_frames++;
    }
}

void granular_engine_render_silent(GranularEngine *engine, int16_t *output,
                                   size_t frames,
                                   const GranularConfig *config)
{
    int limit = prepare_render(engine, config);
    memset(output, 0, frames * 2 * sizeof(*output));
    render_silent_frames(engine, frames, config, limit);
}

void granular_engine_render_silent_wide(GranularEngine *engine,
                                        int32_t *output, size_t frames,
                                        const GranularConfig *config)
{
    int limit = prepare_render(engine, config);
    memset(output, 0, frames * 2 * sizeof(*output));
    render_silent_frames(engine, frames, config, limit);
}

bool granular_engine_pop_marker(GranularEngine *engine,
                                GranularMarker *marker)
{
    if (engine->marker_read == engine->marker_write)
        return false;
    *marker = engine->markers[engine->marker_read];
    engine->marker_read = (uint8_t)((engine->marker_read + 1)
                                  % GRANULAR_MARKER_QUEUE_SIZE);
    return true;
}

int granular_engine_active_voices(const GranularEngine *engine)
{
    int count = 0;
    for (int index = 0; index < GRANULAR_VOICE_COUNT; index++)
        count += engine->voices[index].active ? 1 : 0;
    return count;
}
