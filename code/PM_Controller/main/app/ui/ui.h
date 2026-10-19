#ifndef PM_UI_H
#define PM_UI_H

#include "lvgl.h"

/* Once after display_init(); owns KEY1-3 event consumption and an update task. */
void ui_start(lv_display_t *display);

/* Optional film-counter input. Values above 8 are clamped. Call from task
 * context; the UI task samples the value on its next 50 ms update. */
void ui_set_film_remaining(unsigned remaining);

#endif
