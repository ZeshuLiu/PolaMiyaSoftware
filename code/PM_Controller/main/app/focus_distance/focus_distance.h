#ifndef PM_FOCUS_DISTANCE_H
#define PM_FOCUS_DISTANCE_H
#include <stdint.h>
#include "focus_unit.h"

typedef enum {
    FOCUS_DISTANCE_READY,
    FOCUS_DISTANCE_INITIALIZING,
    FOCUS_DISTANCE_ERROR,
    FOCUS_DISTANCE_COMMUNICATION,
} focus_distance_state_t;
typedef struct {
    int32_t target_raw;
    focus_distance_state_t state;
} focus_distance_t;

/* RAW / 100000 is a display placeholder, not calibrated object distance. */
focus_distance_t focus_distance_get(const focus_unit_snapshot_t *snapshot);
#endif
