#ifndef PM_DISPLAY_H
#define PM_DISPLAY_H

#include "lcd_st7789.h"
#include "lvgl.h"

/* Call once at startup. The official port owns LVGL's task and draw buffers. */
lv_display_t *display_init(const lcd_st7789_t *lcd);

#endif
