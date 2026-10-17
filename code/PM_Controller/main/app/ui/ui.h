#ifndef PM_UI_H
#define PM_UI_H

#include "lvgl.h"

/* Once after display_init(); owns KEY1-3 event consumption and an update task. */
void ui_start(lv_display_t *display);

#endif
