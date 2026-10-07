#ifndef PM_LCD_ST7789_H
#define PM_LCD_ST7789_H

#include <stdbool.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#define LCD_ST7789_SHORT_SIDE 172
#define LCD_ST7789_LONG_SIDE  320
#define LCD_ST7789_RAM_WIDTH 240
#define LCD_ST7789_RAM_HEIGHT 320

/* Board settings, shared by initialization and the diagnostic log. */
#define LCD_ST7789_SPI_CLOCK_HZ  (20 * 1000 * 1000)
#define LCD_ST7789_BGR           false
#define LCD_ST7789_INVERT_COLORS true

/* Enough for 16 landscape rows; both SPI and LVGL use this size. */
#define LCD_ST7789_DRAW_BUFFER_PIXELS (LCD_ST7789_LONG_SIDE * 16)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
} lcd_st7789_t;

/* Initialize once at startup; initialization errors stop startup. */
void lcd_st7789_init(lcd_st7789_t *lcd);
void lcd_st7789_backlight(bool on);

#endif
