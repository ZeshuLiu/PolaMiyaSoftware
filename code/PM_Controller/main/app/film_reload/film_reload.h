#ifndef PM_FILM_RELOAD_H
#define PM_FILM_RELOAD_H

#include "esp_err.h"

/* Once after ui_start(); starts the low-priority FILM_KEY acknowledgement task. */
esp_err_t film_reload_start(void);

#endif
