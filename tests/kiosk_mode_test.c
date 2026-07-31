#include "kiosk_mode.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool valid_pitch(int pitch)
{
    return pitch == -12 || pitch == -7 || pitch == 0
        || pitch == 7 || pitch == 12;
}

static void test_random_choices_change_texture(void)
{
    KioskMode mode;
    kiosk_mode_init(&mode, UINT32_C(0x12345678));

    for (int index = 0; index < 100; index++) {
        int sample = kiosk_mode_choose_sample(&mode, 5, 2);
        assert(sample >= 0 && sample < 5);
        assert(sample != 2);

        int pitch = kiosk_mode_choose_pitch(&mode, 0);
        assert(valid_pitch(pitch));
        assert(pitch != 0);
    }
    assert(kiosk_mode_choose_sample(&mode, 1, 0) == 0);
    assert(kiosk_mode_choose_sample(&mode, 0, 0) == -1);
}

static void test_random_interval_bounds(void)
{
    KioskMode mode;
    kiosk_mode_init(&mode, UINT32_C(0x87654321));
    bool saw_different_interval = false;
    uint64_t previous_interval = 0;
    for (int index = 0; index < 100; index++) {
        uint64_t now = UINT64_C(1000000) + (uint64_t)index * 100000;
        kiosk_mode_schedule(&mode, now);
        uint64_t interval = mode.next_change_ms - now;
        assert(interval >= KIOSK_MIN_INTERVAL_MS);
        assert(interval <= KIOSK_MAX_INTERVAL_MS);
        if (previous_interval != 0 && interval != previous_interval)
            saw_different_interval = true;
        previous_interval = interval;
    }
    assert(saw_different_interval);
}

static void test_burst_freeze_and_repeat_lifecycle(void)
{
    KioskMode mode;
    kiosk_mode_init(&mode, UINT32_C(0xCAFEBABE));
    kiosk_mode_begin_burst(&mode, 10, 8);
    assert(mode.freeze_pending);
    assert(mode.target_grains == 18);
    assert(!kiosk_mode_due(&mode, UINT64_MAX));
    assert(!kiosk_mode_complete_burst(&mode, 17, 0, 1000));
    assert(!kiosk_mode_complete_burst(&mode, 18, 1, 1000));
    assert(kiosk_mode_complete_burst(&mode, 18, 0, 1000));
    assert(!mode.freeze_pending);
    assert(mode.next_change_ms >= 1000 + KIOSK_MIN_INTERVAL_MS);
    assert(mode.next_change_ms <= 1000 + KIOSK_MAX_INTERVAL_MS);
    assert(!kiosk_mode_due(&mode, mode.next_change_ms - 1));
    assert(kiosk_mode_due(&mode, mode.next_change_ms));
}

static void test_human_input_cancels_permanently(void)
{
    KioskMode mode;
    assert(!kiosk_mode_input_recognized(0, 0, 0, 0));
    assert(kiosk_mode_input_recognized(1, 0, 0, 0));
    assert(kiosk_mode_input_recognized(0, 1, 0, 0));
    assert(kiosk_mode_input_recognized(0, 0, 1, 0));
    assert(kiosk_mode_input_recognized(0, 0, 0, 1));
    kiosk_mode_init(&mode, 0);
    assert(mode.active);
    assert(mode.random_state != 0);
    kiosk_mode_begin_burst(&mode, 0, 8);
    kiosk_mode_cancel(&mode);
    assert(!mode.active);
    assert(!mode.freeze_pending);
    assert(mode.next_change_ms == 0);
    assert(!kiosk_mode_due(&mode, UINT64_MAX));
    assert(!kiosk_mode_complete_burst(&mode, 8, 0, 1000));
    kiosk_mode_schedule(&mode, 1000);
    assert(mode.next_change_ms == 0);
}

int main(void)
{
    test_random_choices_change_texture();
    test_random_interval_bounds();
    test_burst_freeze_and_repeat_lifecycle();
    test_human_input_cancels_permanently();
    puts("kiosk_mode_test: all checks passed");
    return 0;
}
