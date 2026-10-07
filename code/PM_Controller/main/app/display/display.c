#include "display.h"

#include "esp_err.h"
#include "esp_lvgl_port.h"

lv_display_t *display_init(const lcd_st7789_t *lcd)
{
    const lvgl_port_cfg_t port_config = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_config));

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = lcd->io,
        .panel_handle = lcd->panel,
        .buffer_size = LCD_ST7789_DRAW_BUFFER_PIXELS,
        .double_buffer = true,
        .hres = LCD_ST7789_SHORT_SIDE,
        .vres = LCD_ST7789_LONG_SIDE,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
            /* ST7789 receives the high RGB565 byte first. */
            .swap_bytes = true,
        },
    };

    lv_display_t *display = lvgl_port_add_disp(&display_config);
    ESP_ERROR_CHECK(display ? ESP_OK : ESP_ERR_NO_MEM);
    return display;
}
