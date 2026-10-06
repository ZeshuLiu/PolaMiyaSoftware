#include "lcd_st7789.h"

#include <stdlib.h>
#include "PIN.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LCD_HOST SPI2_HOST
#define STRIP_ROWS 16
#define STRIP_BYTES (LCD_ST7789_LONG_SIDE * STRIP_ROWS * 2)

static const char *TAG = "lcd_st7789";

struct lcd_st7789 {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t transfer_done;
    uint8_t *dma_buffer;
    bool bus_initialized;
    bool backlight_initialized;
    int width;
    int height;
    int x_gap;
    int y_gap;
};

static bool color_transfer_done(esp_lcd_panel_io_handle_t io,
                               esp_lcd_panel_io_event_data_t *event,
                               void *user_ctx)
{
    (void)io;
    (void)event;
    struct lcd_st7789 *lcd = user_ctx;
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR(lcd->transfer_done, &task_woken);
    return task_woken == pdTRUE;
}

void lcd_st7789_delete(lcd_st7789_handle_t lcd)
{
    if (!lcd) {
        return;
    }
    if (lcd->backlight_initialized) {
        gpio_set_level(PIN_LCD_BLK, 1);
    }
    if (lcd->panel) {
        esp_lcd_panel_del(lcd->panel);
    }
    if (lcd->io) {
        esp_lcd_panel_io_del(lcd->io);
    }
    if (lcd->bus_initialized) {
        spi_bus_free(LCD_HOST);
    }
    if (lcd->transfer_done) {
        vSemaphoreDelete(lcd->transfer_done);
    }
    heap_caps_free(lcd->dma_buffer);
    free(lcd);
}

esp_err_t lcd_st7789_create(const lcd_st7789_config_t *config,
                          lcd_st7789_handle_t *out_lcd)
{
    if (!config || !out_lcd || config->spi_clock_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_lcd = NULL;
    esp_err_t ret = ESP_OK;
    struct lcd_st7789 *lcd = heap_caps_calloc(1, sizeof(*lcd), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!lcd) {
        return ESP_ERR_NO_MEM;
    }
    lcd->transfer_done = xSemaphoreCreateBinary();
    lcd->dma_buffer = heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_GOTO_ON_FALSE(lcd->transfer_done && lcd->dma_buffer, ESP_ERR_NO_MEM,
                      fail, TAG, "buffer allocation failed");

    /* Preload the output latch before enabling the active-low backlight. */
    ESP_GOTO_ON_ERROR(gpio_set_level(PIN_LCD_BLK, 1), fail, TAG, "backlight off");
    const gpio_config_t backlight = {
        .pin_bit_mask = 1ULL << PIN_LCD_BLK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&backlight), fail, TAG, "backlight GPIO");
    lcd->backlight_initialized = true;

    const spi_bus_config_t bus = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = STRIP_BYTES,
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO),
                      fail, TAG, "SPI bus initialization");
    lcd->bus_initialized = true;
    const esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = PIN_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = config->spi_clock_hz,
        .trans_queue_depth = 1,
        .on_color_trans_done = color_transfer_done,
        .user_ctx = lcd,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi(LCD_HOST, &io, &lcd->io),
                      fail, TAG, "SPI panel IO");
    const esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = PIN_LCD_RES,
        .rgb_ele_order = config->bgr ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd->io, &panel, &lcd->panel),
                      fail, TAG, "ST7789 creation");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(lcd->panel), fail, TAG, "reset");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(lcd->panel), fail, TAG, "initialization");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_invert_color(lcd->panel, config->invert_colors),
                      fail, TAG, "color inversion");
    ESP_GOTO_ON_ERROR(lcd_st7789_set_view(lcd, LCD_ROTATION_0, 34, 0),
                      fail, TAG, "initial view");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(lcd->panel, true),
                      fail, TAG, "display on");
    *out_lcd = lcd;
    return ESP_OK;

fail:
    lcd_st7789_delete(lcd);
    return ret;
}

esp_err_t lcd_st7789_backlight(lcd_st7789_handle_t lcd, bool on)
{
    if (!lcd || !lcd->backlight_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    return gpio_set_level(PIN_LCD_BLK, on ? 0 : 1);
}

esp_err_t lcd_st7789_set_view(lcd_st7789_handle_t lcd, lcd_rotation_t rotation,
                            int x_gap, int y_gap)
{
    if (!lcd || !lcd->panel || rotation < LCD_ROTATION_0 || rotation > LCD_ROTATION_270) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool swap = rotation == LCD_ROTATION_90 || rotation == LCD_ROTATION_270;
    const bool mirror_x = rotation == LCD_ROTATION_90 || rotation == LCD_ROTATION_180;
    const bool mirror_y = rotation == LCD_ROTATION_180 || rotation == LCD_ROTATION_270;
    const int width = swap ? LCD_ST7789_LONG_SIDE : LCD_ST7789_SHORT_SIDE;
    const int height = swap ? LCD_ST7789_SHORT_SIDE : LCD_ST7789_LONG_SIDE;
    const int ram_width = swap ? 320 : 240;
    const int ram_height = swap ? 240 : 320;
    if (x_gap < 0 || y_gap < 0 || x_gap > ram_width - width || y_gap > ram_height - height) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(lcd->panel, swap), TAG, "swap axes");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(lcd->panel, mirror_x, mirror_y), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(lcd->panel, x_gap, y_gap), TAG, "window gap");
    lcd->width = width;
    lcd->height = height;
    lcd->x_gap = x_gap;
    lcd->y_gap = y_gap;
    return ESP_OK;
}

int lcd_st7789_width(lcd_st7789_handle_t lcd)
{
    return lcd ? lcd->width : 0;
}

int lcd_st7789_height(lcd_st7789_handle_t lcd)
{
    return lcd ? lcd->height : 0;
}

esp_err_t lcd_st7789_clear(lcd_st7789_handle_t lcd, uint16_t color)
{
    if (!lcd || !lcd->panel || lcd->width == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool landscape = lcd->width > lcd->height;
    const int width = landscape ? 320 : 240;
    const int height = landscape ? 240 : 320;
    for (size_t i = 0; i < STRIP_BYTES / 2; ++i) {
        lcd->dma_buffer[2 * i] = (uint8_t)(color >> 8);
        lcd->dma_buffer[2 * i + 1] = (uint8_t)color;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(lcd->panel, 0, 0), TAG, "clear gap");
    esp_err_t result = ESP_OK;
    for (int row = 0; row < height; row += STRIP_ROWS) {
        const int rows = height - row < STRIP_ROWS ? height - row : STRIP_ROWS;
        result = esp_lcd_panel_draw_bitmap(lcd->panel, 0, row, width, row + rows,
                                          lcd->dma_buffer);
        if (result != ESP_OK) {
            break;
        }
        xSemaphoreTake(lcd->transfer_done, portMAX_DELAY);
    }
    const esp_err_t restore = esp_lcd_panel_set_gap(lcd->panel, lcd->x_gap, lcd->y_gap);
    return result != ESP_OK ? result : restore;
}

esp_err_t lcd_st7789_draw_rgb565(lcd_st7789_handle_t lcd, int x, int y,
                               int width, int height, const uint16_t *pixels)
{
    if (!lcd || !pixels || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > lcd->width || height > lcd->height ||
        x > lcd->width - width || y > lcd->height - height) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int row = 0; row < height; row += STRIP_ROWS) {
        const int rows = height - row < STRIP_ROWS ? height - row : STRIP_ROWS;
        const size_t count = (size_t)width * rows;
        const uint16_t *source = pixels + (size_t)row * width;
        for (size_t i = 0; i < count; ++i) {
            lcd->dma_buffer[2 * i] = (uint8_t)(source[i] >> 8);
            lcd->dma_buffer[2 * i + 1] = (uint8_t)source[i];
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(lcd->panel, x, y + row,
                            x + width, y + row + rows, lcd->dma_buffer), TAG, "draw");
        /* The DMA buffer is reused only after its transfer-completion callback. */
        xSemaphoreTake(lcd->transfer_done, portMAX_DELAY);
    }
    return ESP_OK;
}
