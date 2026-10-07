#ifndef PM_FLASH_TRIGGER_H
#define PM_FLASH_TRIGGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define FLASH_TRIGGER_DEFAULT_US 20000U
#define FLASH_TRIGGER_MIN_US 1000U
#define FLASH_TRIGGER_MAX_US 30000U
#define FLASH_TRIGGER_RECOVERY_US 1000U

/* Initialize at startup before consumers. Initialization never fires a pulse. */
esp_err_t flash_trigger_init(void);

/* Task-context, nonblocking APIs. Busy requests return ESP_ERR_INVALID_STATE.
 * ESP_OK means accepted for transmission, not that the flash actually fired. */
esp_err_t flash_trigger_fire(void);
esp_err_t flash_trigger_fire_us(uint32_t pulse_us);
bool flash_trigger_is_busy(void);

#endif
