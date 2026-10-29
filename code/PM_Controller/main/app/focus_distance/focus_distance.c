#include "focus_distance.h"

focus_distance_t focus_distance_get(const focus_unit_snapshot_t *s)
{
    focus_distance_state_t state;
    if (s->valid && (s->work_state == 2 || s->mode == 3 || s->move_state == 3))
        state = FOCUS_DISTANCE_ERROR;
    else if (s->command_rejected && s->command_result != 0)
        state = s->command_result == 3 ? FOCUS_DISTANCE_INITIALIZING : FOCUS_DISTANCE_ERROR;
    else if (!s->valid) state = FOCUS_DISTANCE_COMMUNICATION;
    else if (s->work_state != 1 || !s->initialized || !s->zero_valid)
        state = FOCUS_DISTANCE_INITIALIZING;
    else state = FOCUS_DISTANCE_READY;
    return (focus_distance_t){.target_raw = s->desired_raw, .state = state};
}
