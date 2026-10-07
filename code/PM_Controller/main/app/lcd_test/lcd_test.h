#ifndef PM_LCD_TEST_H
#define PM_LCD_TEST_H

#include "lcd_st7789.h"
#include "lvgl.h"

/* Call once after display_init(); the diagnostic continues on LVGL's timer. */
void lcd_test_start(lv_display_t *display, const lcd_st7789_t *lcd);

#endif
