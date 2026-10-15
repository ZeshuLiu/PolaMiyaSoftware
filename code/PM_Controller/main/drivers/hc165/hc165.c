#include "hc165.h"

#include "PIN.h"
#include "hc165_keys.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HC165_SCAN_INTERVAL_MS 5
#define HC165_TASK_STACK_BYTES 2048
#define HC165_TASK_PRIORITY 3
#define HC165_SPI_CLOCK_HZ 500000

static spi_device_handle_t spi;
/* Internal, word-aligned DMA storage; only one byte is clocked in.
 * Startup reads finish before the scan task takes ownership. */
static DMA_ATTR uint8_t spi_rx[4];
static TaskHandle_t scan_task;
static hc165_keys_state_t keys;
static hc165_inputs_t inputs;
static hc165_key_events_t pending_events;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;

static void load_parallel_inputs(spi_transaction_t *transaction)
{
    (void)transaction;
    /* The driver has applied Mode 2 before this callback. Load after any
     * initial clock-level change, so the first received bit is still D7.
     * Polling transfers execute this callback in the calling task. */
    gpio_set_level(PIN_MASPI_CS, 0);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_MASPI_CS, 1);
    esp_rom_delay_us(1);
}

static uint8_t read_parallel_inputs(void)
{
    spi_transaction_t transaction = {
        .length = 8,
        .rxlength = 8,
        .rx_buffer = spi_rx,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &transaction));
    return spi_rx[0];
}

static void publish_inputs(uint8_t raw, hc165_key_events_t events)
{
    const uint8_t pressed = hc165_keys_pressed(&keys);
    portENTER_CRITICAL(&state_lock);
    inputs.raw = raw;
    inputs.pressed = pressed;
    inputs.charging = (raw & HC165_INPUT_CHG) == 0;
    inputs.charge_done = (raw & HC165_INPUT_STB) == 0;
    pending_events.short_press |= events.short_press;
    pending_events.long_press |= events.long_press;
    portEXIT_CRITICAL(&state_lock);
}

static void scan_once(void)
{
    const uint8_t raw = read_parallel_inputs();
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    const hc165_key_events_t events = hc165_keys_update(&keys, raw, now_ms);
    publish_inputs(raw, events);
}

static void scan_inputs(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(HC165_SCAN_INTERVAL_MS);

    while (true) {
        scan_once();
        xTaskDelayUntil(&last_wake, interval);
    }
}

void hc165_init(void)
{
    ESP_ERROR_CHECK(scan_task ? ESP_ERR_INVALID_STATE : ESP_OK);

    /* /PL stays high except for the parallel-load pulse. SPI owns CP and Q7. */
    ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CS, 1));
    const gpio_config_t output_config = {
        .pin_bit_mask = 1ULL << PIN_MASPI_CS,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));
    const spi_bus_config_t bus_config = {
        .mosi_io_num = -1,
        .miso_io_num = PIN_MASPI_MISO,
        .sclk_io_num = PIN_MASPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t device_config = {
        .clock_speed_hz = HC165_SPI_CLOCK_HZ,
        .mode = 2, /* Sample on falling edges; HC165 shifts on rising edges. */
        .spics_io_num = -1,
        .queue_size = 1,
        .pre_cb = load_parallel_inputs,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &device_config, &spi));

    const uint8_t raw = read_parallel_inputs();
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    hc165_keys_init(&keys, raw, now_ms);
    publish_inputs(raw, (hc165_key_events_t){0});

    const BaseType_t created = xTaskCreate(scan_inputs, "hc165_scan", HC165_TASK_STACK_BYTES,
                                          NULL, HC165_TASK_PRIORITY, &scan_task);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

hc165_inputs_t hc165_get_inputs(void)
{
    portENTER_CRITICAL(&state_lock);
    const hc165_inputs_t snapshot = inputs;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}

hc165_key_events_t hc165_get_events(void)
{
    portENTER_CRITICAL(&state_lock);
    const hc165_key_events_t snapshot = pending_events;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}

hc165_key_events_t hc165_take_events(void)
{
    portENTER_CRITICAL(&state_lock);
    const hc165_key_events_t snapshot = pending_events;
    pending_events.short_press = 0;
    pending_events.long_press = 0;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}
