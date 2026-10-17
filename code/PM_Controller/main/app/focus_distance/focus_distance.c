#include "focus_distance.h"

focus_distance_t focus_distance_get(const sdm50_sample_t *actual)
{
    /* TODO: receive FocusUnit position, then apply the calibrated position-to-
     * object-distance mapping. Encoder counts must never be displayed as mm. */
    return (focus_distance_t) {
        .distance_mm = actual->distance_mm,
        .state = actual->status == SDM50_VALID ? FOCUS_DISTANCE_FINITE :
                 actual->status == SDM50_NO_TARGET ? FOCUS_DISTANCE_INFINITY :
                                                    FOCUS_DISTANCE_UNAVAILABLE,
        .using_actual = true,
    };
}
