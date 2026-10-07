#include "lcd_st7789.h"

#include "PIN.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_st7789.h"

void lcd_st7789_backlight(bool on)
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BLK, on ? 0 : 1));
}

void lcd_st7789_init(lcd_st7789_t *lcd)
{
    /* Preload the output latch before enabling the active-low backlight. */
    lcd_st7789_backlight(false);
    const gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << PIN_LCD_BLK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&backlight_config));

    const spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_ST7789_DRAW_BUFFER_PIXELS * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_ST7789_SPI_CLOCK_HZ,
        .trans_queue_depth = 2,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        /* esp_lvgl_port registers the transfer-completion callback. */
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &lcd->io));

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RES,
        .rgb_ele_order = LCD_ST7789_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd->io, &panel_config, &lcd->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd->panel, LCD_ST7789_INVERT_COLORS));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd->panel, true));
}
