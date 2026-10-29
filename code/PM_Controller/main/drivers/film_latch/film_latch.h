#ifndef PM_FILM_LATCH_H
#define PM_FILM_LATCH_H

#include <stdbool.h>
#include "esp_err.h"

/* Call once before the owning task. Initialization preserves an existing event. */
esp_err_t film_latch_init(void);
bool film_latch_pending(void);
/* Task context: pulse GPIO7 high for at least 100 ms, release, then check GPIO6. */
esp_err_t film_latch_clear(void);

#endif
