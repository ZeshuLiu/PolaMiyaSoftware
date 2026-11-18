#ifndef PM_FILM_RELOAD_H
#define PM_FILM_RELOAD_H

#include "esp_err.h"

/* Once before ui_start(); starts the low-priority FILM_KEY acknowledgement task. */
esp_err_t film_reload_start(void);

/* Serialize latch handling with an eject, discarding events during motion. */
esp_err_t film_reload_begin_eject(void);
void film_reload_end_eject(void);

#endif
