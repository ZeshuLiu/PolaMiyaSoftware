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
/* ST7789V3 write cycle >=16 ns (62.5 MHz max). ESP32-S3 APB=80 MHz:
 * 40 MHz is the highest divider output below that limit; 62.5 MHz requests
 * are rounded up to 80 MHz by this IDF clock calculation. */
#define LCD_ST7789_SPI_CLOCK_HZ  (40 * 1000 * 1000)
#define LCD_ST7789_BGR           false
#define LCD_ST7789_INVERT_COLORS true
#define LCD_ST7789_BACKLIGHT_PWM_HZ  20000
#define LCD_ST7789_BACKLIGHT_FADE_MS 300

/* 64 portrait rows per buffer; LVGL allocates two internal DMA buffers.
 * Each buffer is 11,008 RGB565 pixels (22,016 bytes); SPI uses the same limit. */
#define LCD_ST7789_DRAW_BUFFER_PIXELS (LCD_ST7789_SHORT_SIDE * 64)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
} lcd_st7789_t;

/* Initialize once at startup; initialization errors stop startup. */
void lcd_st7789_init(lcd_st7789_t *lcd);
/* Task context: on fades to the configured brightness; off is immediate. */
void lcd_st7789_backlight(bool on);
/* Task context; 0..100 percent, active-low PWM. Does not turn an off panel on. */
void lcd_st7789_set_brightness(unsigned percent);

#endif
