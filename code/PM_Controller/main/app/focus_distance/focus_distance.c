#include "focus_distance.h"
#include "focus_calibration.h"
#include <limits.h>
#include <math.h>

/* 17 rounded logarithmic distances from 1 to 10 m, then 15 m and
 * 20 m; the twentieth manual stop is infinity. */
#define FOCUS_MANUAL_FINITE_COUNT 19U
static const uint32_t focus_manual_distance_mm[FOCUS_MANUAL_FINITE_COUNT] = {
    1000U, 1200U, 1300U, 1500U, 1800U, 2100U, 2400U, 2700U, 3200U, 3700U, 4200U, 4900U, 5600U, 6500U, 7500U, 8700U, 10000U, 15000U, 20000U
};


bool focus_distance_to_raw(uint32_t distance_mm, bool infinity, int32_t *raw)
{
    if (!raw || (!infinity && distance_mm < FOCUS_CALIBRATION_MIN_DISTANCE_MM)) return false;
    const double value = infinity ? FOCUS_CALIBRATION_OFFSET_RAW :
        FOCUS_CALIBRATION_OFFSET_RAW + FOCUS_CALIBRATION_A_RAW_M /
        ((double)distance_mm / 1000.0 - FOCUS_CALIBRATION_B_M);
    if (!isfinite(value) || value < 0 || value > INT32_MAX) return false;
    *raw = (int32_t)lround(value);
    return true;
}

bool focus_distance_from_raw(int32_t raw, uint32_t *distance_mm, bool *infinity)
{
    if (!distance_mm || !infinity) return false;
    int32_t infinity_raw, minimum_raw;
    if (!focus_distance_to_raw(0, true, &infinity_raw) ||
        !focus_distance_to_raw(FOCUS_CALIBRATION_MIN_DISTANCE_MM, false, &minimum_raw) ||
        raw < infinity_raw - FOCUS_CALIBRATION_INFINITY_TOLERANCE_RAW || raw > minimum_raw) return false;
    *infinity = raw <= infinity_raw + FOCUS_CALIBRATION_INFINITY_TOLERANCE_RAW;
    if (*infinity) { *distance_mm = 0; return true; }
    const double mm = 1000.0 * (FOCUS_CALIBRATION_B_M +
                      FOCUS_CALIBRATION_A_RAW_M / (raw - FOCUS_CALIBRATION_OFFSET_RAW));
    if (!isfinite(mm) || mm < 0 || mm > UINT32_MAX) return false;
    *distance_mm = (uint32_t)lround(mm);
    return true;
}

bool focus_distance_manual_step(int32_t current_raw, bool increase_distance, int32_t *target_raw)
{
    if (!target_raw) return false;
    unsigned nearest = 0;
    int64_t smallest = INT64_MAX;
    for (unsigned i = 0; i <= FOCUS_MANUAL_FINITE_COUNT; ++i) {
        int32_t raw;
        const bool infinity = i == FOCUS_MANUAL_FINITE_COUNT;
        if (!focus_distance_to_raw(infinity ? 0 : focus_manual_distance_mm[i], infinity, &raw)) return false;
        int64_t difference = (int64_t)current_raw - raw;
        if (difference < 0) difference = -difference;
        if (difference < smallest) { smallest = difference; nearest = i; }
    }
    if (increase_distance && nearest < FOCUS_MANUAL_FINITE_COUNT) ++nearest;
    else if (!increase_distance && nearest > 0) --nearest;
    const bool infinity = nearest == FOCUS_MANUAL_FINITE_COUNT;
    return focus_distance_to_raw(infinity ? 0 : focus_manual_distance_mm[nearest], infinity, target_raw);
}

focus_distance_t focus_distance_get(const focus_unit_snapshot_t *s)
{
    focus_distance_state_t state;
    if (s->storage_error || (s->valid && (s->work_state == 2 || s->mode == 3 || s->move_state == 3)))
        state = FOCUS_DISTANCE_ERROR;
    else if (s->command_rejected && s->command_result != 0)
        state = s->command_result == 3 ? FOCUS_DISTANCE_INITIALIZING : FOCUS_DISTANCE_ERROR;
    else if (!s->valid) state = FOCUS_DISTANCE_COMMUNICATION;
    else if (s->initialization_required || s->work_state != 1 || !s->initialized || !s->zero_valid)
        state = FOCUS_DISTANCE_INITIALIZING;
    else state = FOCUS_DISTANCE_READY;
    focus_distance_t result = {.target_raw = s->desired_raw, .state = state};
    result.distance_valid = focus_distance_from_raw(result.target_raw, &result.distance_mm, &result.infinity);
    return result;
}
