#ifndef PM_FOCUS_DISTANCE_H
#define PM_FOCUS_DISTANCE_H
#include <stdbool.h>
#include <stdint.h>
#include "focus_unit.h"
#include "focus_calibration.h"

typedef enum {
    FOCUS_DISTANCE_READY,
    FOCUS_DISTANCE_INITIALIZING,
    FOCUS_DISTANCE_ERROR,
    FOCUS_DISTANCE_COMMUNICATION,
} focus_distance_state_t;
typedef struct {
    int32_t target_raw;
    focus_distance_state_t state;
    bool distance_valid;
    bool infinity;
    uint32_t distance_mm;
} focus_distance_t;

/* Continuous calibration, suitable for automatic focus. Reject distances below
 * the calibrated minimum; infinity is requested explicitly. */
bool focus_distance_to_raw_with_calibration(const focus_calibration_t *cal, uint32_t distance_mm, bool infinity, int32_t *raw);
bool focus_distance_to_raw(uint32_t distance_mm, bool infinity, int32_t *raw);
bool focus_distance_from_raw(int32_t raw, uint32_t *distance_mm, bool *infinity);
bool focus_distance_manual_step(int32_t current_raw, bool increase_distance, int32_t *target_raw);
focus_distance_t focus_distance_get(const focus_unit_snapshot_t *snapshot);
#endif
