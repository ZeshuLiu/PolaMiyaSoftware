#ifndef PM_USB_POWER_H
#define PM_USB_POWER_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t usb_power_init(void);
/* Task context; present must be non-null. High means U18 has released /RESET. */
esp_err_t usb_power_get(bool *present);

#endif
