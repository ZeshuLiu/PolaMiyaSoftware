#ifndef PM_FILM_MOTOR_H
#define PM_FILM_MOTOR_H

#include <stdbool.h>
#include "esp_err.h"

#define FILM_MOTOR_PWM_HZ            20000
/* Physical GPIO1 high-level fraction. 0 = IN1 always low, full drive. */
#define FILM_MOTOR_IN1_HIGH_PERCENT  0
#define FILM_MOTOR_RAMP_MS           1000

/* Task context. Initialize once before UI startup; starts with IN1=IN2=1. */
esp_err_t film_motor_init(void);
/* Fixed direction: IN1=inverted PWM, IN2=1. Ramp from current duty for 1 s;
 * false ramps to braking at 11. Requires the fade service installed by LCD init. */
esp_err_t film_motor_set_running(bool running);

#endif
