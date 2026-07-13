#include "edit_repeat.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define LEFT UINT32_C(1)
#define RIGHT UINT32_C(2)

static void test_requires_modifier_before_direction(void)
{
    EditRepeatState state = { 0 };
    assert(edit_repeat_update(&state, false, LEFT, LEFT) == 0);
    assert(edit_repeat_update(&state, true, 0, LEFT) == 0);
    assert(edit_repeat_update(&state, true, 0, 0) == 0);
    assert(edit_repeat_update(&state, true, LEFT, LEFT) == LEFT);
}

static void test_delay_repeat_and_acceleration(void)
{
    EditRepeatState state = { 0 };
    assert(edit_repeat_update(&state, true, RIGHT, RIGHT) == RIGHT);
    for (int frame = 1; frame < EDIT_REPEAT_DELAY_FRAMES; frame++)
        assert(edit_repeat_update(&state, true, 0, RIGHT) == 0);
    assert(edit_repeat_update(&state, true, 0, RIGHT) == RIGHT);
    for (int frame = 1; frame < EDIT_REPEAT_INTERVAL_FRAMES; frame++)
        assert(edit_repeat_update(&state, true, 0, RIGHT) == 0);
    assert(edit_repeat_update(&state, true, 0, RIGHT) == RIGHT);

    while (state.held_frames < EDIT_REPEAT_FAST_FRAMES)
        (void)edit_repeat_update(&state, true, 0, RIGHT);
    assert(edit_repeat_update(&state, true, 0, RIGHT) == RIGHT);
    assert(edit_repeat_update(&state, true, 0, RIGHT) == RIGHT);
}

static void test_release_and_direction_change_reset(void)
{
    EditRepeatState state = { 0 };
    assert(edit_repeat_update(&state, true, LEFT, LEFT) == LEFT);
    assert(edit_repeat_update(&state, true, 0, 0) == 0);
    assert(edit_repeat_update(&state, true, RIGHT, RIGHT) == RIGHT);
    assert(edit_repeat_update(&state, false, 0, RIGHT) == 0);
    assert(state.active_directions == 0);
}

int main(void)
{
    test_requires_modifier_before_direction();
    test_delay_repeat_and_acceleration();
    test_release_and_direction_change_reset();
    puts("edit_repeat_test: all checks passed");
    return 0;
}
