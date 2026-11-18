#include "focus_distance.h"
#include "focus_calibration.h"
#include "settings.h"
#include <limits.h>
#include <math.h>

/* 17 rounded logarithmic distances from 1 to 10 m, then 15 m and
 * 20 m; the twentieth manual stop is infinity. */
#define FOCUS_MANUAL_FINITE_COUNT 19U
static const uint32_t focus_manual_distance_mm[FOCUS_MANUAL_FINITE_COUNT] = {
    1000U, 1200U, 1300U, 1500U, 1800U, 2100U, 2400U, 2700U, 3200U, 3700U, 4200U, 4900U, 5600U, 6500U, 7500U, 8700U, 10000U, 15000U, 20000U
};


focus_calibration_t focus_calibration_defaults(void)
{
    return (focus_calibration_t){FOCUS_CALIBRATION_OFFSET_RAW, FOCUS_CALIBRATION_A_RAW_M,
        FOCUS_CALIBRATION_B_M, FOCUS_CALIBRATION_MIN_DISTANCE_MM,
        FOCUS_CALIBRATION_INFINITY_TOLERANCE_RAW};
}

bool focus_calibration_valid(const focus_calibration_t *c)
{
    if (!c || !isfinite(c->offset_raw) || !isfinite(c->a_raw_m) || !isfinite(c->b_m) ||
        c->offset_raw < 0 || c->offset_raw > INT32_MAX || c->a_raw_m <= 0 ||
        c->min_distance_mm < 1 || c->min_distance_mm > 20000 ||
        c->infinity_tolerance_raw > 10000 ||
        c->b_m >= (double)c->min_distance_mm / 1000.0) return false;
    const double max_raw = c->offset_raw + c->a_raw_m /
        ((double)c->min_distance_mm / 1000.0 - c->b_m);
    return isfinite(max_raw) && max_raw <= INT32_MAX &&
           max_raw > c->offset_raw + c->infinity_tolerance_raw;
}

esp_err_t focus_calibration_get(focus_calibration_t *cal) { return settings_get_calibration(cal); }
esp_err_t focus_calibration_save(const focus_calibration_t *cal) { return settings_save_calibration(cal); }

static bool to_raw(const focus_calibration_t *cal, uint32_t distance_mm, bool infinity, int32_t *raw)
{
    if (!raw || (!infinity && distance_mm < cal->min_distance_mm)) return false;
    const double value = infinity ? cal->offset_raw : cal->offset_raw + cal->a_raw_m /
                         ((double)distance_mm / 1000.0 - cal->b_m);
    if (!isfinite(value) || value < 0 || value > INT32_MAX) return false;
    *raw = (int32_t)lround(value);
    return true;
}

bool focus_distance_to_raw_with_calibration(const focus_calibration_t *cal, uint32_t distance_mm, bool infinity, int32_t *raw)
{
    return focus_calibration_valid(cal) && to_raw(cal, distance_mm, infinity, raw);
}

bool focus_distance_to_raw(uint32_t distance_mm, bool infinity, int32_t *raw)
{
    focus_calibration_t cal;
    return focus_calibration_get(&cal) == ESP_OK && to_raw(&cal, distance_mm, infinity, raw);
}

bool focus_distance_from_raw(int32_t raw, uint32_t *distance_mm, bool *infinity)
{
    focus_calibration_t cal;
    if (!distance_mm || !infinity || focus_calibration_get(&cal) != ESP_OK) return false;
    int32_t infinity_raw, minimum_raw;
    if (!to_raw(&cal, 0, true, &infinity_raw) ||
        !to_raw(&cal, cal.min_distance_mm, false, &minimum_raw) ||
        (int64_t)raw < (int64_t)infinity_raw - cal.infinity_tolerance_raw || raw > minimum_raw) return false;
    *infinity = (int64_t)raw <= (int64_t)infinity_raw + cal.infinity_tolerance_raw;
    if (*infinity) { *distance_mm = 0; return true; }
    const double mm = 1000.0 * (cal.b_m + cal.a_raw_m / (raw - cal.offset_raw));
    if (!isfinite(mm) || mm < 0 || mm > UINT32_MAX) return false;
    *distance_mm = (uint32_t)lround(mm);
    return true;
}

bool focus_distance_manual_step(int32_t current_raw, bool increase_distance, int32_t *target_raw)
{
    focus_calibration_t cal;
    if (!target_raw || focus_calibration_get(&cal) != ESP_OK) return false;
    uint32_t distances[FOCUS_MANUAL_FINITE_COUNT + 1];
    unsigned count = 0;
    distances[count++] = cal.min_distance_mm;
    for (unsigned i = 0; i < FOCUS_MANUAL_FINITE_COUNT; ++i)
        if (focus_manual_distance_mm[i] > cal.min_distance_mm) distances[count++] = focus_manual_distance_mm[i];
    unsigned nearest = 0;
    int64_t smallest = INT64_MAX;
    for (unsigned i = 0; i <= count; ++i) {
        int32_t raw;
        if (!to_raw(&cal, i == count ? 0 : distances[i], i == count, &raw)) return false;
        int64_t difference = (int64_t)current_raw - raw;
        if (difference < 0) difference = -difference;
        if (difference < smallest) { smallest = difference; nearest = i; }
    }
    if (increase_distance && nearest < count) ++nearest;
    else if (!increase_distance && nearest > 0) --nearest;
    return to_raw(&cal, nearest == count ? 0 : distances[nearest], nearest == count, target_raw);
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
