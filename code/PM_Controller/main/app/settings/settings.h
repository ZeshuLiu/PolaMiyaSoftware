#ifndef PM_SETTINGS_H
#define PM_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Persistent IDs: never renumber or reuse an existing ID for a new meaning. */
typedef enum {
    SETTINGS_REBOOT_COUNT = 1,
    SETTINGS_SHUTTER_COUNT = 2,
    SETTINGS_MOTOR_RUN_MS = 3,
    SETTINGS_BACKLIGHT_PERCENT = 4,
} settings_key_t;

typedef struct {
    uint64_t reboot_count;
    uint64_t shutter_count;
    uint64_t motor_run_ms;
    uint64_t backlight_percent;
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

/* Explicitly import Rev1 version-2 bytes at 0x20 into RAM; then call commit. */
esp_err_t settings_import_rev1(void);

#endif
