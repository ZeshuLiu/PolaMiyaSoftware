#ifndef PM_SETTINGS_H
#define PM_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "focus_calibration.h"

#define SETTINGS_FILM_DARK_SLIDE 9U /* Eight sheets plus the unremoved cover. */

/* Persistent IDs: never renumber or reuse an existing ID for a new meaning. */
typedef enum {
    SETTINGS_REBOOT_COUNT = 1,
    SETTINGS_SHUTTER_COUNT = 2,
    SETTINGS_MOTOR_RUN_MS = 3,
    SETTINGS_BACKLIGHT_PERCENT = 4,
    SETTINGS_FILM_REMAINING = 5,
    SETTINGS_FOCUS_RAW = 6,
    SETTINGS_FOCUS_VALID = 7,
    SETTINGS_CAL_OFFSET = 8,
    SETTINGS_CAL_A = 9,
    SETTINGS_CAL_B = 10,
    SETTINGS_CAL_MIN_MM = 11,
    SETTINGS_CAL_INF_TOL = 12,
    SETTINGS_WIFI_ENABLED = 13,
    SETTINGS_BLE_REMOTE_ENABLED = 14,
} settings_key_t;

typedef struct {
    uint64_t reboot_count;
    uint64_t shutter_count;
    uint64_t motor_run_ms;
    uint64_t backlight_percent;
    uint64_t film_remaining;
    uint64_t focus_raw;       /* int32 RAW encoded as uint32 two's complement. */
    uint64_t cal_offset, cal_a, cal_b; /* IEEE-754 binary64 bits. */
    uint64_t cal_min_mm, cal_inf_tol;
    uint64_t focus_valid;     /* 0 before motion; 1 only after confirmed STOP. */
    uint64_t wifi_enabled;    /* Saved user preference, 0 or 1. */
    uint64_t ble_remote_enabled; /* Saved BLE REMOTE preference, 0 or 1. */
} settings_values_t;

typedef struct {
    bool dirty;
    bool loaded_from_eeprom;
    bool writable;
    uint32_t sequence;
    esp_err_t storage_error;
} settings_status_t;

/* Call once at startup before consumers; this also initializes the EEPROM driver.
 * Init only reads EEPROM; defaults stay in RAM until an explicit commit.
 * All APIs are for task context, including getters. */
esp_err_t settings_init(void);
esp_err_t settings_get(settings_values_t *values);
esp_err_t settings_get_status(settings_status_t *status);
esp_err_t settings_set(settings_key_t key, uint64_t value);
esp_err_t settings_increment(settings_key_t key, uint64_t amount);
esp_err_t settings_commit(void);
/* Persist only shutter_count; leave other uncommitted RAM parameters untouched. */
esp_err_t settings_commit_shutter(void);
/* Update and persist the Wi-Fi preference only. On write failure the RAM
 * value remains dirty and can be retried with settings_commit(). */
esp_err_t settings_save_wifi_enabled(bool enabled);
/* Same selected-field save semantics as the Wi-Fi preference. */
esp_err_t settings_save_ble_remote_enabled(bool enabled);
/* Persist only film_remaining; leave other uncommitted RAM parameters untouched. */
esp_err_t settings_commit_film(void);
/* One-shot decrement (dark slide 9 -> 8, otherwise saturating at 0) and film-only save.
 * On a write failure retry commit_film(), never this decrement. */
esp_err_t settings_consume_film(void);
/* Atomically update and persist only the focus pair, under the parameter lock.
 * Motion is forbidden unless save_focus(false, ...) succeeds. */
esp_err_t settings_save_focus(bool valid, int32_t raw);
/* Group save persists first and publishes all five fields only on success. */
esp_err_t settings_get_calibration(focus_calibration_t *cal);
esp_err_t settings_save_calibration(const focus_calibration_t *cal);
/* Read the committed pair, never an uncommitted RAM value. */
esp_err_t settings_load_focus(bool *valid, int32_t *raw);

/* Explicitly import Rev1 version-2 bytes at 0x20 into RAM; then call commit. */
esp_err_t settings_import_rev1(void);

#endif
