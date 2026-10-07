#ifndef PM_EEPROM_H
#define PM_EEPROM_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* U12: user-confirmed 24C64, WP tied low, no exposed address pins. */
#define EEPROM_CAPACITY_BYTES 8192U
#define EEPROM_PAGE_BYTES 32U
#define EEPROM_DEVICE_ADDRESS 0x50U

/* Initialize at startup before consumers. Repeated calls reuse the existing bus.
 * Task-context access; writes wait for completion. */
esp_err_t eeprom_init(void);
esp_err_t eeprom_read(uint32_t address, void *data, size_t length);
esp_err_t eeprom_write(uint32_t address, const void *data, size_t length);

#endif
