#include "lcd_test.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

/* Set AUTO_CYCLE to 0 to hold the selected case after identifying the screen. */
#define LCD_TEST_AUTO_CYCLE     1
#define LCD_TEST_ROTATION_INDEX 0   /* 0, 1, 2, 3 -> 0, 90, 180, 270 degrees */
#define LCD_TEST_GAP_INDEX      0   /* 0, 1, 2 -> 34, 0, 68 pixels */
#define LCD_TEST_HOLD_MS        6000

#define BLACK  0x000000
#define WHITE  0xFFFFFF
#define RED    0xFF0000
#define GREEN  0x00FF00
#define BLUE   0x0000FF
#define YELLOW 0xFFFF00
#define GRID   0x202020

static const char *TAG = "lcd_test";
static const int gaps[] = {34, 0, 68};
static const lv_display_rotation_t rotations[] = {
    LV_DISPLAY_ROTATION_0,
    LV_DISPLAY_ROTATION_90,
    LV_DISPLAY_ROTATION_180,
    LV_DISPLAY_ROTATION_270,
};

/* This context outlives app_main(); timer callbacks run on the LVGL task. */
static struct {
    lv_display_t *display;
    lcd_st7789_t lcd;
    unsigned int rotation_index;
    unsigned int gap_index;
} test;

static lv_obj_t *rectangle(lv_obj_t *parent, int x, int y, int width, int height,
                           uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}

static void outline(lv_obj_t *parent, int inset, int width, int height, uint32_t color)
{
    lv_obj_t *object = rectangle(parent, inset, inset,
                                 width - 2 * inset, height - 2 * inset, BLACK);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(object, 1, 0);
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, const char *text, uint32_t color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    return object;
}

static void draw_pattern(unsigned int case_number, int x_gap, int y_gap)
{
    lv_obj_t *screen = lv_display_get_screen_active(test.display);
    const int width = lv_display_get_horizontal_resolution(test.display);
    const int height = lv_display_get_vertical_resolution(test.display);

    for (int x = 20; x < width; x += 20) {
        rectangle(screen, x, 0, 1, height, GRID);
    }
    for (int y = 20; y < height; y += 20) {
        rectangle(screen, 0, y, width, 1, GRID);
    }
    outline(screen, 0, width, height, WHITE);
    outline(screen, 4, width, height, YELLOW);

    rectangle(screen, 8, 8, 20, 20, RED);
    rectangle(screen, width - 28, 8, 20, 20, GREEN);
    rectangle(screen, 8, height - 28, 20, 20, BLUE);
    rectangle(screen, width - 28, height - 28, 20, 20, WHITE);
    label(screen, 10, 10, "TL", WHITE);
    label(screen, width - 26, 10, "TR", BLACK);
    label(screen, 10, height - 26, "BL", WHITE);
    label(screen, width - 26, height - 26, "BR", BLACK);
    lv_obj_t *top = label(screen, 0, 0, "TOP", WHITE);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 10);

    const int label_x = width > height ? 38 : 20;
    rectangle(screen, label_x - 2, 38, 134, 72, BLACK);
    lv_obj_t *details = label(screen, label_x, 40, "", WHITE);
    lv_label_set_text_fmt(details, "CASE %u\nROT %u\nX%d Y%d\n%dx%d",
                          case_number, test.rotation_index * 90, x_gap, y_gap, width, height);

    const int arrow_x = width > height ? width - 64 : (width - 24) / 2;
    const int arrow_y = height / 2 - 24;
    /* LVGL keeps the point array by reference, so it has static storage. */
    static const lv_point_precise_t arrow_points[] = {{0, 12}, {12, 0}, {24, 12}};
    lv_obj_t *arrow = lv_line_create(screen);
    lv_line_set_points(arrow, arrow_points, 3);
    lv_obj_set_pos(arrow, arrow_x, arrow_y);
    lv_obj_set_style_line_color(arrow, lv_color_hex(YELLOW), 0);
    lv_obj_set_style_line_width(arrow, 3, 0);
    rectangle(screen, arrow_x + 11, arrow_y + 2, 3, 34, YELLOW);
    label(screen, arrow_x + 3, arrow_y + 38, "UP", YELLOW);

    const int band_width = (width - 64) / 3;
    const uint32_t colors[] = {RED, GREEN, BLUE};
    const char *names[] = {"R", "G", "B"};
    for (int i = 0; i < 3; ++i) {
        const int left = 32 + i * band_width;
        rectangle(screen, left, height - 42, band_width, 18, colors[i]);
        label(screen, left + (band_width - 10) / 2, height - 41,
              names[i], i == 1 ? BLACK : WHITE);
    }
}

static void refresh_and_wait(void)
{
    lv_obj_invalidate(lv_display_get_screen_active(test.display));
    lv_refr_now(test.display);
    /* With command -1, the SPI IO only waits for queued DMA transfers.
     * Drain the last strip before changing the view or lighting the backlight. */
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(test.lcd.io, -1, NULL, 0));
}

static void show_case(void)
{
    lcd_st7789_backlight(false);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(test.lcd.io, -1, NULL, 0));

    lv_obj_t *screen = lv_display_get_screen_active(test.display);
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(BLACK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_display_set_rotation(test.display, rotations[test.rotation_index]);
    /* Clear the whole controller RAM through LVGL, including the hidden margins.
     * Then restore the glass size and test the selected offset. */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(test.lcd.panel, 0, 0));
    lv_display_set_resolution(test.display, LCD_ST7789_RAM_WIDTH, LCD_ST7789_RAM_HEIGHT);
    refresh_and_wait();
    lv_display_set_resolution(test.display, LCD_ST7789_SHORT_SIDE, LCD_ST7789_LONG_SIDE);

    const bool landscape = test.rotation_index % 2 != 0;
    const int x_gap = landscape ? 0 : gaps[test.gap_index];
    const int y_gap = landscape ? gaps[test.gap_index] : 0;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(test.lcd.panel, x_gap, y_gap));

    const unsigned int case_number = test.rotation_index * 3 + test.gap_index + 1;
    draw_pattern(case_number, x_gap, y_gap);
    refresh_and_wait();
    lcd_st7789_backlight(true);
    ESP_LOGI(TAG, "CASE %u: ROT=%u size=%ldx%ld gap=(%d,%d) BGR=%d INVERT=%d",
             case_number, test.rotation_index * 90,
             (long)lv_display_get_horizontal_resolution(test.display),
             (long)lv_display_get_vertical_resolution(test.display), x_gap, y_gap,
             LCD_ST7789_BGR, LCD_ST7789_INVERT_COLORS);
}

#if LCD_TEST_AUTO_CYCLE
static void next_case(lv_timer_t *timer)
{
    (void)timer;
    test.gap_index = (test.gap_index + 1) % 3;
    if (test.gap_index == 0) {
        test.rotation_index = (test.rotation_index + 1) % 4;
    }
    /* LVGL invokes timer callbacks with the official port's mutex held. */
    show_case();
}
#endif

void lcd_test_start(lv_display_t *display, const lcd_st7789_t *lcd)
{
    _Static_assert(LCD_TEST_ROTATION_INDEX >= 0 && LCD_TEST_ROTATION_INDEX < 4,
                   "Invalid LCD rotation index");
    _Static_assert(LCD_TEST_GAP_INDEX >= 0 && LCD_TEST_GAP_INDEX < 3,
                   "Invalid LCD gap index");
    test.display = display;
    test.lcd = *lcd;
    test.rotation_index = LCD_TEST_ROTATION_INDEX;
    test.gap_index = LCD_TEST_GAP_INDEX;

    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    show_case();
#if LCD_TEST_AUTO_CYCLE
    lv_timer_t *timer = lv_timer_create(next_case, LCD_TEST_HOLD_MS, NULL);
    ESP_ERROR_CHECK(timer ? ESP_OK : ESP_ERR_NO_MEM);
#endif
    lvgl_port_unlock();
}
