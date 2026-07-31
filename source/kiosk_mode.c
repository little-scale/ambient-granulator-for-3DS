#include "kiosk_mode.h"

#include <stddef.h>

#define KIOSK_FALLBACK_SEED UINT32_C(0x6D2B79F5)

static const int kiosk_pitches[] = { -12, -7, 0, 7, 12 };

void kiosk_mode_init(KioskMode *mode, uint32_t seed)
{
    mode->active = true;
    mode->freeze_pending = false;
    mode->random_state = seed != 0 ? seed : KIOSK_FALLBACK_SEED;
    mode->target_grains = 0;
    mode->next_change_ms = 0;
}

void kiosk_mode_cancel(KioskMode *mode)
{
    mode->active = false;
    mode->freeze_pending = false;
    mode->target_grains = 0;
    mode->next_change_ms = 0;
}

bool kiosk_mode_input_recognized(uint32_t keys_down, uint32_t keys_up,
                                 uint32_t keys_held,
                                 uint32_t analog_directions)
{
    return keys_down != 0 || keys_up != 0 || keys_held != 0
        || analog_directions != 0;
}

uint32_t kiosk_mode_random(KioskMode *mode)
{
    mode->random_state ^= mode->random_state << 13;
    mode->random_state ^= mode->random_state >> 17;
    mode->random_state ^= mode->random_state << 5;
    if (mode->random_state == 0)
        mode->random_state = KIOSK_FALLBACK_SEED;
    return mode->random_state;
}

int kiosk_mode_choose_sample(KioskMode *mode, int sample_count,
                             int current_sample)
{
    if (sample_count < 1)
        return -1;
    if (sample_count == 1)
        return 0;
    if (current_sample < 0 || current_sample >= sample_count)
        return (int)(kiosk_mode_random(mode) % (uint32_t)sample_count);

    int candidate = (int)(kiosk_mode_random(mode)
                        % (uint32_t)(sample_count - 1));
    if (candidate >= current_sample)
        candidate++;
    return candidate;
}

int kiosk_mode_choose_pitch(KioskMode *mode, int current_pitch)
{
    const int count = (int)(sizeof(kiosk_pitches) / sizeof(kiosk_pitches[0]));
    int current_index = -1;
    for (int index = 0; index < count; index++) {
        if (kiosk_pitches[index] == current_pitch) {
            current_index = index;
            break;
        }
    }
    if (current_index < 0)
        return kiosk_pitches[kiosk_mode_random(mode) % (uint32_t)count];

    int candidate = (int)(kiosk_mode_random(mode) % (uint32_t)(count - 1));
    if (candidate >= current_index)
        candidate++;
    return kiosk_pitches[candidate];
}

void kiosk_mode_schedule(KioskMode *mode, uint64_t now_ms)
{
    if (!mode->active)
        return;
    uint32_t range = KIOSK_MAX_INTERVAL_MS - KIOSK_MIN_INTERVAL_MS + 1;
    uint32_t interval = KIOSK_MIN_INTERVAL_MS
                      + kiosk_mode_random(mode) % range;
    mode->next_change_ms = now_ms + interval;
}

bool kiosk_mode_due(const KioskMode *mode, uint64_t now_ms)
{
    return mode->active && !mode->freeze_pending
        && mode->next_change_ms != 0 && now_ms >= mode->next_change_ms;
}

void kiosk_mode_begin_burst(KioskMode *mode, uint32_t grains_launched,
                            int grain_count)
{
    if (!mode->active)
        return;
    if (grain_count < 1)
        grain_count = 1;
    if (grain_count > 32)
        grain_count = 32;
    mode->target_grains = grains_launched + (uint32_t)grain_count;
    mode->freeze_pending = true;
    mode->next_change_ms = 0;
}

bool kiosk_mode_complete_burst(KioskMode *mode, uint32_t grains_launched,
                               int active_voices, uint64_t now_ms)
{
    if (!mode->active || !mode->freeze_pending
            || grains_launched < mode->target_grains
            || active_voices > 0)
        return false;
    mode->freeze_pending = false;
    kiosk_mode_schedule(mode, now_ms);
    return true;
}
