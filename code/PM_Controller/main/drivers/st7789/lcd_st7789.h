#ifndef PM_LCD_ST7789_H
#define PM_LCD_ST7789_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_ST7789_SHORT_SIDE 172
#define LCD_ST7789_LONG_SIDE  320

typedef struct lcd_st7789 *lcd_st7789_handle_t;

typedef enum {
    LCD_ROTATION_0,
    LCD_ROTATION_90,
    LCD_ROTATION_180,
    LCD_ROTATION_270,
} lcd_rotation_t;

typedef struct {
    unsigned int spi_clock_hz;
    bool bgr;
    bool invert_colors;
} lcd_st7789_config_t;

/* Uses the LCD pins in PIN.h and owns SPI2. Call from one task only. */
esp_err_t lcd_st7789_create(const lcd_st7789_config_t *config,
                          lcd_st7789_handle_t *out_lcd);
void lcd_st7789_delete(lcd_st7789_handle_t lcd);
esp_err_t lcd_st7789_backlight(lcd_st7789_handle_t lcd, bool on);
esp_err_t lcd_st7789_set_view(lcd_st7789_handle_t lcd, lcd_rotation_t rotation,
                            int x_gap, int y_gap);
int lcd_st7789_width(lcd_st7789_handle_t lcd);
int lcd_st7789_height(lcd_st7789_handle_t lcd);

/* Clears the full 240x320 controller RAM, including pixels outside the glass. */
esp_err_t lcd_st7789_clear(lcd_st7789_handle_t lcd, uint16_t color);

/* Packed, row-major RGB565 in CPU byte order, no stride.
 * Blocks until DMA finishes; the caller can then reuse/free pixels.
 */
esp_err_t lcd_st7789_draw_rgb565(lcd_st7789_handle_t lcd, int x, int y,
                               int width, int height, const uint16_t *pixels);

#ifdef __cplusplus
}
#endif
#endif
