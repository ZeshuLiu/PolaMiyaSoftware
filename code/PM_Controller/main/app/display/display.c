#include "display.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"

static const char *TAG = "display";
static int64_t render_started_us;
static int64_t last_slow_report_us;

static void report_slow_render(lv_event_t *event)
{
    const int64_t now = esp_timer_get_time();
    if (lv_event_get_code(event) == LV_EVENT_RENDER_START) {
        render_started_us = now;
    } else if (render_started_us) {
        const int64_t elapsed = now - render_started_us;
        if (elapsed >= 100000 && now - last_slow_report_us >= 1000000) {
            ESP_LOGW(TAG, "LVGL render cycle took %lld ms", (long long)(elapsed / 1000));
            last_slow_report_us = now;
        }
        render_started_us = 0;
    }
}

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
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    /* Observe slow drawing without replacing the port's SPI/DMA callbacks. */
    lv_display_add_event_cb(display, report_slow_render, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(display, report_slow_render, LV_EVENT_RENDER_READY, NULL);
    /* CASE 7 was confirmed on the board. Clear all controller RAM while dark
     * before restoring the visible portrait window; the port owns DMA callbacks. */
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd->panel, 0, 0));
    lv_display_set_resolution(display, LCD_ST7789_RAM_WIDTH, LCD_ST7789_RAM_HEIGHT);
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_invalidate(screen);
    lv_refr_now(display);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(lcd->io, -1, NULL, 0));
    lv_display_set_resolution(display, LCD_ST7789_SHORT_SIDE, LCD_ST7789_LONG_SIDE);
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd->panel, 34, 0));
    lvgl_port_unlock();
    return display;
}
