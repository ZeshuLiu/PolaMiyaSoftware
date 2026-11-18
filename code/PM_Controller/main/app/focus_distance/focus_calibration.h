#ifndef PM_FOCUS_CALIBRATION_H
#define PM_FOCUS_CALIBRATION_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Device 90DA724F2FBC, Cal/cal90DA724F2FBC.csv. Distance in metres.
 * One-device fit: finite measurements 1..9 m and an infinity anchor.
 * Distances beyond 9 m are extrapolated; cross-device use is unverified. */
#define FOCUS_CALIBRATION_OFFSET_RAW 1016.0
#define FOCUS_CALIBRATION_A_RAW_M 14898.9422517
#define FOCUS_CALIBRATION_B_M 0.25014189161
#define FOCUS_CALIBRATION_MIN_DISTANCE_MM 1000U
/* FocusUnit move acceptance is ±50 RAW; this is a display classification,
 * not a change to the calibrated infinity target or a measured distance. */
#define FOCUS_CALIBRATION_INFINITY_TOLERANCE_RAW 50


typedef struct {
    double offset_raw, a_raw_m, b_m;
    uint32_t min_distance_mm, infinity_tolerance_raw;
} focus_calibration_t;

focus_calibration_t focus_calibration_defaults(void);
bool focus_calibration_valid(const focus_calibration_t *cal);
/* All readers receive a single settings snapshot; save commits the entire group. */
esp_err_t focus_calibration_get(focus_calibration_t *cal);
esp_err_t focus_calibration_save(const focus_calibration_t *cal);

#endif
