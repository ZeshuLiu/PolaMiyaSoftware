#include "lcd_test.h"

#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd_st7789.h"

/* Set AUTO_CYCLE to 0 to hold the selected case after identifying the screen. */
#define LCD_TEST_AUTO_CYCLE     1
#define LCD_TEST_ROTATION_INDEX 0   /* 0, 1, 2, 3 -> 0, 90, 180, 270 degrees */
#define LCD_TEST_GAP_INDEX      0   /* 0, 1, 2 -> 34, 0, 68 pixels */
#define LCD_TEST_HOLD_MS        6000
#define LCD_TEST_BGR            false
#define LCD_TEST_INVERT         true

#define BLACK  0x0000
#define WHITE  0xFFFF
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define YELLOW 0xFFE0
#define GRID   0x2104

static const char *TAG = "lcd_test";
static const int gaps[] = {34, 0, 68};

typedef struct {
    char character;
    uint8_t rows[7];
} glyph_t;

/* Small diagnostic font, drawn by the application rather than the driver. */
static const glyph_t font[] = {
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
};

static void rectangle(uint16_t *frame, int width, int height,
                      int x, int y, int w, int h, uint16_t color)
{
    for (int row = y; row < y + h; ++row) {
        if (row < 0 || row >= height) {
            continue;
        }
        for (int col = x; col < x + w; ++col) {
            if (col >= 0 && col < width) {
                frame[(size_t)row * width + col] = color;
            }
        }
    }
}

static void text(uint16_t *frame, int width, int height,
                 int x, int y, const char *string, int scale, uint16_t color)
{
    for (; *string; ++string, x += 6 * scale) {
        const glyph_t *glyph = NULL;
        for (size_t i = 0; i < sizeof(font) / sizeof(font[0]); ++i) {
            if (font[i].character == *string) {
                glyph = &font[i];
                break;
            }
        }
        if (!glyph) {
            continue;
        }
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph->rows[row] & (1U << (4 - col))) {
                    rectangle(frame, width, height, x + col * scale,
                              y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

static void pattern(uint16_t *frame, int width, int height, unsigned int case_number,
                    int degrees, int x_gap, int y_gap)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint16_t color = x % 20 == 0 || y % 20 == 0 ? GRID : BLACK;
            if (x == 0 || y == 0 || x == width - 1 || y == height - 1) {
                color = WHITE;
            }
            if (((x == 4 || x == width - 5) && y >= 4 && y < height - 4) ||
                ((y == 4 || y == height - 5) && x >= 4 && x < width - 4)) {
                color = YELLOW;
            }
            frame[(size_t)y * width + x] = color;
        }
    }

    rectangle(frame, width, height, 8, 8, 20, 20, RED);
    rectangle(frame, width, height, width - 28, 8, 20, 20, GREEN);
    rectangle(frame, width, height, 8, height - 28, 20, 20, BLUE);
    rectangle(frame, width, height, width - 28, height - 28, 20, 20, WHITE);
    text(frame, width, height, 12, 14, "TL", 1, WHITE);
    text(frame, width, height, width - 24, 14, "TR", 1, BLACK);
    text(frame, width, height, 12, height - 22, "BL", 1, WHITE);
    text(frame, width, height, width - 24, height - 22, "BR", 1, BLACK);
    text(frame, width, height, (width - 36) / 2, 12, "TOP", 2, WHITE);

    const int label_x = width > height ? 38 : 20;
    char label[24];
    rectangle(frame, width, height, label_x - 2, 38, 136, 82, BLACK);
    snprintf(label, sizeof(label), "CASE %u", case_number);
    text(frame, width, height, label_x, 40, label, 2, WHITE);
    snprintf(label, sizeof(label), "ROT %d", degrees);
    text(frame, width, height, label_x, 60, label, 2, WHITE);
    snprintf(label, sizeof(label), "X%d Y%d", x_gap, y_gap);
    text(frame, width, height, label_x, 80, label, 2, WHITE);
    snprintf(label, sizeof(label), "%dX%d", width, height);
    text(frame, width, height, label_x, 100, label, 2, WHITE);

    const int arrow_x = width > height ? width - 64 : width / 2;
    const int arrow_y = width > height ? height / 2 : height * 2 / 3;
    rectangle(frame, width, height, arrow_x - 2, arrow_y - 14, 5, 35, YELLOW);
    for (int i = 0; i < 13; ++i) {
        rectangle(frame, width, height, arrow_x - i, arrow_y - 22 + i, 2, 2, YELLOW);
        rectangle(frame, width, height, arrow_x + i, arrow_y - 22 + i, 2, 2, YELLOW);
    }
    text(frame, width, height, arrow_x - 11, arrow_y + 25, "UP", 2, YELLOW);

    const int band_width = (width - 64) / 3;
    const uint16_t colors[] = {RED, GREEN, BLUE};
    const char *names[] = {"R", "G", "B"};
    for (int i = 0; i < 3; ++i) {
        const int left = 32 + i * band_width;
        rectangle(frame, width, height, left, height - 42, band_width, 18, colors[i]);
        text(frame, width, height, left + (band_width - 10) / 2,
             height - 40, names[i], 2, i == 1 ? BLACK : WHITE);
    }
}

void lcd_test_run(void)
{
    _Static_assert(LCD_TEST_ROTATION_INDEX >= 0 && LCD_TEST_ROTATION_INDEX < 4,
                   "Invalid LCD rotation index");
    _Static_assert(LCD_TEST_GAP_INDEX >= 0 && LCD_TEST_GAP_INDEX < 3,
                   "Invalid LCD gap index");
    const lcd_st7789_config_t config = {
        .spi_clock_hz = 20 * 1000 * 1000,
        .bgr = LCD_TEST_BGR,
        .invert_colors = LCD_TEST_INVERT,
    };
    lcd_st7789_handle_t lcd = NULL;
    esp_err_t result = lcd_st7789_create(&config, &lcd);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "LCD initialization failed: %s", esp_err_to_name(result));
        return;
    }
    uint16_t *frame = malloc(LCD_ST7789_SHORT_SIDE * LCD_ST7789_LONG_SIDE * sizeof(*frame));
    if (!frame) {
        ESP_LOGE(TAG, "Frame allocation failed");
        lcd_st7789_delete(lcd);
        return;
    }

    unsigned int rotation = LCD_TEST_ROTATION_INDEX;
    unsigned int gap_index = LCD_TEST_GAP_INDEX;
    ESP_LOGI(TAG, "Check all four edges, upright text/arrow, and R/G/B bars");
    while (true) {
        const bool landscape = rotation % 2 != 0;
        const int x_gap = landscape ? 0 : gaps[gap_index];
        const int y_gap = landscape ? gaps[gap_index] : 0;
        const unsigned int case_number = rotation * 3 + gap_index + 1;
        result = lcd_st7789_backlight(lcd, false);
        if (result != ESP_OK) {
            break;
        }
        result = lcd_st7789_set_view(lcd, (lcd_rotation_t)rotation, x_gap, y_gap);
        if (result != ESP_OK) {
            break;
        }
        /* Remove the preceding case even when its visible window was different. */
        result = lcd_st7789_clear(lcd, BLACK);
        if (result != ESP_OK) {
            break;
        }
        const int width = lcd_st7789_width(lcd);
        const int height = lcd_st7789_height(lcd);
        pattern(frame, width, height, case_number, (int)rotation * 90, x_gap, y_gap);
        result = lcd_st7789_draw_rgb565(lcd, 0, 0, width, height, frame);
        if (result != ESP_OK) {
            break;
        }
        result = lcd_st7789_backlight(lcd, true);
        if (result != ESP_OK) {
            break;
        }
        ESP_LOGI(TAG, "CASE %u: ROT=%u size=%dx%d gap=(%d,%d) BGR=%d INVERT=%d",
                 case_number, rotation * 90, width, height, x_gap, y_gap,
                 LCD_TEST_BGR, LCD_TEST_INVERT);
        vTaskDelay(pdMS_TO_TICKS(LCD_TEST_HOLD_MS));
        if (LCD_TEST_AUTO_CYCLE) {
            gap_index = (gap_index + 1) % 3;
            if (gap_index == 0) {
                rotation = (rotation + 1) % 4;
            }
        }
    }
    ESP_LOGE(TAG, "Display test failed: %s", esp_err_to_name(result));
    free(frame);
    lcd_st7789_delete(lcd);
}
