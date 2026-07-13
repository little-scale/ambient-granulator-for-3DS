#include "edit_repeat.h"

#include <string.h>

void edit_repeat_reset(EditRepeatState *state)
{
    memset(state, 0, sizeof(*state));
}

uint32_t edit_repeat_update(EditRepeatState *state, bool modifier_held,
                            uint32_t directions_down,
                            uint32_t directions_held)
{
    if (!modifier_held) {
        edit_repeat_reset(state);
        return 0;
    }

    if (state->active_directions != 0
            && directions_held != state->active_directions)
        edit_repeat_reset(state);

    if (state->active_directions == 0) {
        uint32_t newly_pressed = directions_down & directions_held;
        if (newly_pressed == 0)
            return 0;
        state->active_directions = newly_pressed;
        state->held_frames = 0;
        return newly_pressed;
    }

    state->held_frames++;
    if (state->held_frames < EDIT_REPEAT_DELAY_FRAMES)
        return 0;
    if (state->held_frames >= EDIT_REPEAT_FAST_FRAMES)
        return state->active_directions;
    if ((state->held_frames - EDIT_REPEAT_DELAY_FRAMES)
            % EDIT_REPEAT_INTERVAL_FRAMES == 0)
        return state->active_directions;
    return 0;
}
