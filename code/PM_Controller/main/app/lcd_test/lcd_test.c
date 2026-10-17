#include "lcd_test.h"

#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"

#define PAGE_HOLD_US  (10LL * 1000 * 1000)
#define REPORT_US     (1LL * 1000 * 1000)
#define WIDTH         LCD_ST7789_SHORT_SIDE
#define HEIGHT        LCD_ST7789_LONG_SIDE

static const char *TAG = "lcd_test";

/* All UI and statistics run on the LVGL task under the port's mutex. */
static struct {
    lv_display_t *display;
    lcd_st7789_t lcd;
    lv_obj_t *screen;
    lv_obj_t *stats;
    lv_obj_t *frame_label;
    lv_obj_t *vertical_bar;
    lv_obj_t *horizontal_bar;
    bool corners_page;
    int64_t page_start_us;
    int64_t report_start_us;
    int64_t last_frame_us;
    int64_t longest_interval_us;
    uint32_t sample_frames;
    uint32_t total_frames;
} test;

static lv_obj_t *box(int x, int y, int width, int height, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(test.screen);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *text(int x, int y, const char *value, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(test.screen);
    lv_label_set_text(obj, value);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static void border(int inset, int radius, uint32_t color)
{
    lv_obj_t *obj = box(inset, inset, WIDTH - 2 * inset, HEIGHT - 2 * inset, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, radius, 0);
}

static void refresh_and_wait(void)
{
    /* Force a complete frame: moving a small object alone only measures partial updates. */
    lv_obj_invalidate(test.screen);
    lv_refr_now(test.display);
    /* Command -1 drains pending SPI transfers without sending a command.
     * Count the frame only after the last DMA strip completes. Keep the port callback. */
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(test.lcd.io, -1, NULL, 0));
    /* Invalidation resumes LVGL's refresh timer; keep this diagnostic in control. */
    lv_timer_pause(lv_display_get_refr_timer(test.display));
}

static void draw_fps_page(void)
{
    text(16, 16, "FULL FRAME FPS", 0xffffff);
    test.stats = text(16, 42, "FPS --.-\nAVG --.- ms\nMAX --.- ms", 0xffff00);
    test.frame_label = text(16, 104, "FRAME 0", 0xffffff);
    box(16, 132, 46, 144, 0xb82020);
    box(62, 132, 47, 144, 0x209830);
    box(109, 132, 47, 144, 0x2048b8);
    test.vertical_bar = box(16, 132, 6, 144, 0xffffff);
    test.horizontal_bar = box(16, 132, 140, 4, 0xffff00);
    text(16, 284, "40MHz / CASE 7", 0xffffff);
}

static void draw_corners_page(void)
{
    /* Square outlines deliberately reach into the glass's clipped corners. */
    const uint32_t colors[] = {0xffffff, 0xff4040, 0xffff00, 0x40ff60, 0x40cfff};
    for (int i = 0; i < 5; ++i) {
        border(i * 4, 0, colors[i]);
    }
    /* Candidate rounded outline, not an assumed physical glass radius. */
    border(2, 24, 0xff40ff);
    text(20, 22, "TL", 0xffffff);
    text(132, 22, "TR", 0xffffff);
    text(20, 280, "BL", 0xffffff);
    text(132, 280, "BR", 0xffffff);
    text(24, 62, "CORNER / EDGE", 0xffffff);
    text(24, 96, "WHITE   0 px", colors[0]);
    text(24, 118, "RED     4 px", colors[1]);
    text(24, 140, "YELLOW  8 px", colors[2]);
    text(24, 162, "GREEN  12 px", colors[3]);
    text(24, 184, "CYAN   16 px", colors[4]);
    text(24, 218, "MAGENTA R24", 0xff40ff);
    text(24, 242, "172x320 / 180", 0xffffff);
}

static void show_page(bool corners)
{
    test.corners_page = corners;
    lv_obj_clean(test.screen);
    if (corners) {
        draw_corners_page();
    } else {
        draw_fps_page();
    }
    refresh_and_wait();
    /* Exclude page construction and its first frame from measured intervals. */
    test.page_start_us = esp_timer_get_time();
    test.report_start_us = test.page_start_us;
    test.last_frame_us = test.page_start_us;
    test.longest_interval_us = 0;
    test.sample_frames = 0;
    test.total_frames = 0;
    ESP_LOGI(TAG, "PAGE %s: CASE 7, ROT=180, gap=(34,0), hold=10s",
             corners ? "CORNERS" : "FPS");
}

static int bounce(int64_t elapsed_us, int64_t period_us, int distance)
{
    const int64_t phase = elapsed_us % period_us;
    const int64_t ramp = phase < period_us / 2 ? phase : period_us - phase;
    return (int)(ramp * distance * 2 / period_us);
}

static void update_page(lv_timer_t *timer)
{
    (void)timer;
    const int64_t now = esp_timer_get_time();
    const int64_t elapsed = now - test.page_start_us;
    if (elapsed >= PAGE_HOLD_US) {
        show_page(!test.corners_page);
        return;
    }
    if (test.corners_page) {
        return;
    }

    lv_obj_set_x(test.vertical_bar, 16 + bounce(elapsed, 2000000, 134));
    lv_obj_set_y(test.horizontal_bar, 132 + bounce(elapsed, 3000000, 140));
    lv_label_set_text_fmt(test.frame_label, "FRAME %lu", (unsigned long)++test.total_frames);
    refresh_and_wait();
    const int64_t done = esp_timer_get_time();
    const int64_t interval = done - test.last_frame_us;
    test.last_frame_us = done;
    if (interval > test.longest_interval_us) {
        test.longest_interval_us = interval;
    }
    ++test.sample_frames;
    const int64_t window = done - test.report_start_us;
    if (window >= REPORT_US) {
        /* Fixed-point tenths avoid depending on LVGL's float printf configuration. */
        const unsigned fps10 = (unsigned)((int64_t)test.sample_frames * 10000000 / window);
        const unsigned avg10 = (unsigned)(window / test.sample_frames / 100);
        const unsigned max10 = (unsigned)(test.longest_interval_us / 100);
        lv_label_set_text_fmt(test.stats, "FPS %u.%u\nAVG %u.%u ms\nMAX %u.%u ms",
                              fps10 / 10, fps10 % 10, avg10 / 10, avg10 % 10,
                              max10 / 10, max10 % 10);
        ESP_LOGI(TAG, "FPS %u.%u, avg=%u.%u ms, max=%u.%u ms, frames=%lu (full frame, SPI complete)",
                 fps10 / 10, fps10 % 10, avg10 / 10, avg10 % 10,
                 max10 / 10, max10 % 10, (unsigned long)test.sample_frames);
        test.report_start_us = done;
        test.sample_frames = 0;
        test.longest_interval_us = 0;
        /* Updating the statistics label requests another refresh; draw it next frame. */
        lv_timer_pause(lv_display_get_refr_timer(test.display));
    }
}

void lcd_test_start(lv_display_t *display, const lcd_st7789_t *lcd)
{
    test.display = display;
    test.lcd = *lcd;
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    /* This diagnostic drives refresh itself, so the default refresh timer must not
     * produce extra uncounted frames or impose its normal frame-rate limit. */
    lv_timer_pause(lv_display_get_refr_timer(display));
    lcd_st7789_backlight(false);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(lcd->io, -1, NULL, 0));
    test.screen = lv_display_get_screen_active(display);
    lv_obj_clean(test.screen);
    lv_obj_remove_style_all(test.screen);
    lv_obj_remove_flag(test.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(test.screen, lv_color_hex(0x080c14), 0);
    lv_obj_set_style_bg_opa(test.screen, LV_OPA_COVER, 0);
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd->panel, 0, 0));
    lv_display_set_resolution(display, LCD_ST7789_RAM_WIDTH, LCD_ST7789_RAM_HEIGHT);
    refresh_and_wait();
    lv_display_set_resolution(display, WIDTH, HEIGHT);
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd->panel, 34, 0));
    show_page(false);
    lcd_st7789_backlight(true);
    lv_timer_t *timer = lv_timer_create(update_page, 1, NULL);
    ESP_ERROR_CHECK(timer ? ESP_OK : ESP_ERR_NO_MEM);
    lvgl_port_unlock();
}
