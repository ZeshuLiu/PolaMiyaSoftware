#ifndef PM_FOCUS_DISTANCE_H
#define PM_FOCUS_DISTANCE_H

#include <stdbool.h>
#include <stdint.h>
#include "sdm50.h"

typedef enum {
    FOCUS_DISTANCE_UNAVAILABLE,
    FOCUS_DISTANCE_FINITE,
    FOCUS_DISTANCE_INFINITY,
} focus_distance_state_t;

typedef struct {
    uint32_t distance_mm; /* Object distance, not raw FocusUnit encoder counts. */
    focus_distance_state_t state;
    bool using_actual;    /* Temporary SDM50 substitution, shown on the UI. */
} focus_distance_t;

/* Task context. Future FocusUnit reception/calibration belongs behind this API.
 * For now, use the caller's actual-distance snapshot; NO_TARGET displays INF.
 * TOO_CLOSE has no valid focus substitute and returns UNAVAILABLE, not NEAR.
 * Stale data and communication errors never become infinity. */
focus_distance_t focus_distance_get(const sdm50_sample_t *actual);

#endif
