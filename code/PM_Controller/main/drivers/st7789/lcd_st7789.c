#include "lcd_st7789.h"

#include "PIN.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_st7789.h"

#define BACKLIGHT_MODE       LEDC_LOW_SPEED_MODE
#define BACKLIGHT_TIMER      LEDC_TIMER_0
#define BACKLIGHT_CHANNEL    LEDC_CHANNEL_0
#define BACKLIGHT_OFF_DUTY   (1U << 10)

static bool backlight_ready;
static bool backlight_on;
static unsigned backlight_percent = 100;

static unsigned backlight_duty(void)
{
    return BACKLIGHT_OFF_DUTY * (100 - backlight_percent) / 100;
}

void lcd_st7789_set_brightness(unsigned percent)
{
    if (percent > 100) percent = 100;
    backlight_percent = percent;
    if (backlight_ready && backlight_on) {
        ESP_ERROR_CHECK(ledc_fade_stop(BACKLIGHT_MODE, BACKLIGHT_CHANNEL));
        ESP_ERROR_CHECK(ledc_set_duty_and_update(
            BACKLIGHT_MODE, BACKLIGHT_CHANNEL, backlight_duty(), 0));
    }
}

void lcd_st7789_backlight(bool on)
{
    if (!backlight_ready || on == backlight_on) {
        return;
    }
    ESP_ERROR_CHECK(ledc_fade_stop(BACKLIGHT_MODE, BACKLIGHT_CHANNEL));
    if (on) {
        /* Active-low: fade from constant high (off) to the configured brightness.
         * Hardware fading runs without blocking the LVGL task. */
        ESP_ERROR_CHECK(ledc_set_fade_time_and_start(
            BACKLIGHT_MODE, BACKLIGHT_CHANNEL, backlight_duty(),
            LCD_ST7789_BACKLIGHT_FADE_MS, LEDC_FADE_NO_WAIT));
    } else {
        ESP_ERROR_CHECK(ledc_set_duty_and_update(
            BACKLIGHT_MODE, BACKLIGHT_CHANNEL, BACKLIGHT_OFF_DUTY, 0));
    }
    backlight_on = on;
}

void lcd_st7789_init(lcd_st7789_t *lcd)
{
    /* Preload the output latch before enabling the active-low backlight. */
    ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BLK, 1));
    const gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << PIN_LCD_BLK,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&backlight_config));

    const ledc_timer_config_t backlight_timer = {
        .speed_mode = BACKLIGHT_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BACKLIGHT_TIMER,
        .freq_hz = LCD_ST7789_BACKLIGHT_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = PIN_LCD_BLK,
        .speed_mode = BACKLIGHT_MODE,
        .channel = BACKLIGHT_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_TIMER,
        .duty = BACKLIGHT_OFF_DUTY,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    backlight_ready = true;

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
        /* One 40 MHz cycle (25 ns) exceeds the panel's 15 ns CS setup/hold. */
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
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
