#ifndef GRANULATOR_KIOSK_MODE_H
#define GRANULATOR_KIOSK_MODE_H

#include <stdbool.h>
#include <stdint.h>

#define KIOSK_MIN_INTERVAL_MS UINT32_C(30000)
#define KIOSK_MAX_INTERVAL_MS UINT32_C(60000)

typedef struct {
    bool active;
    bool freeze_pending;
    uint32_t random_state;
    uint32_t target_grains;
    uint64_t next_change_ms;
} KioskMode;

void kiosk_mode_init(KioskMode *mode, uint32_t seed);
void kiosk_mode_cancel(KioskMode *mode);
bool kiosk_mode_input_recognized(uint32_t keys_down, uint32_t keys_up,
                                 uint32_t keys_held,
                                 uint32_t analog_directions);
uint32_t kiosk_mode_random(KioskMode *mode);
int kiosk_mode_choose_sample(KioskMode *mode, int sample_count,
                             int current_sample);
int kiosk_mode_choose_pitch(KioskMode *mode, int current_pitch);
void kiosk_mode_schedule(KioskMode *mode, uint64_t now_ms);
bool kiosk_mode_due(const KioskMode *mode, uint64_t now_ms);
void kiosk_mode_begin_burst(KioskMode *mode, uint32_t grains_launched,
                            int grain_count);
bool kiosk_mode_complete_burst(KioskMode *mode, uint32_t grains_launched,
                               int active_voices, uint64_t now_ms);

#endif
