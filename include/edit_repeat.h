#ifndef GRANULATOR_EDIT_REPEAT_H
#define GRANULATOR_EDIT_REPEAT_H

#include <stdbool.h>
#include <stdint.h>

#define EDIT_REPEAT_DELAY_FRAMES 15
#define EDIT_REPEAT_INTERVAL_FRAMES 3
#define EDIT_REPEAT_FAST_FRAMES 60

typedef struct {
    uint32_t active_directions;
    unsigned int held_frames;
} EditRepeatState;

void edit_repeat_reset(EditRepeatState *state);
uint32_t edit_repeat_update(EditRepeatState *state, bool modifier_held,
                            uint32_t directions_down,
                            uint32_t directions_held);

#endif
