#include "flash_trigger.h"

#include "PIN.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"

#define FLASH_RMT_RESOLUTION_HZ 1000000U /* One tick is one microsecond. */

/* RMT keeps the payload pointer until transmission finishes. Keep both the
 * payload and callback state in internal RAM, not on the caller's stack. */
static DRAM_ATTR struct {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    rmt_symbol_word_t pulse;
    bool initialized;
    bool busy;
    bool submitting;
} flash;
static DRAM_ATTR portMUX_TYPE flash_lock = portMUX_INITIALIZER_UNLOCKED;

static bool IRAM_ATTR transmit_done(rmt_channel_handle_t channel,
                                    const rmt_tx_done_event_data_t *event, void *context)
{
    (void)channel;
    (void)event;
    (void)context;
    portENTER_CRITICAL_ISR(&flash_lock);
    flash.busy = false;
    portEXIT_CRITICAL_ISR(&flash_lock);
    return false;
}

static void release_resources(void)
{
    if (flash.encoder) {
        rmt_del_encoder(flash.encoder);
        flash.encoder = NULL;
    }
    if (flash.channel) {
        rmt_del_channel(flash.channel);
        flash.channel = NULL;
    }
    /* On initialization failure, release the output and keep it pulled low.
     * R24 also provides a board-level 10 kOhm pull-down. */
    gpio_set_direction(PIN_FLASH_TRG, GPIO_MODE_INPUT);
    gpio_pullup_dis(PIN_FLASH_TRG);
    gpio_pulldown_en(PIN_FLASH_TRG);
}

esp_err_t flash_trigger_init(void)
{
    if (flash.initialized) {
        return ESP_OK;
    }
    esp_err_t result = gpio_set_level(PIN_FLASH_TRG, 0);
    if (result != ESP_OK) {
        return result;
    }
    const rmt_tx_channel_config_t channel_config = {
        .gpio_num = PIN_FLASH_TRG,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = FLASH_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 48,
        .trans_queue_depth = 1,
        .flags.init_level = 0,
    };
    result = rmt_new_tx_channel(&channel_config, &flash.channel);
    if (result != ESP_OK) {
        goto fail;
    }
    const rmt_copy_encoder_config_t encoder_config = {};
    result = rmt_new_copy_encoder(&encoder_config, &flash.encoder);
    if (result != ESP_OK) {
        goto fail;
    }
    const rmt_tx_event_callbacks_t callbacks = {.on_trans_done = transmit_done};
    result = rmt_tx_register_event_callbacks(flash.channel, &callbacks, &flash);
    if (result != ESP_OK) {
        goto fail;
    }
    result = rmt_enable(flash.channel);
    if (result != ESP_OK) {
        goto fail;
    }
    flash.initialized = true;
    return ESP_OK;

fail:
    release_resources();
    return result;
}

esp_err_t flash_trigger_fire_us(uint32_t pulse_us)
{
    if (pulse_us < FLASH_TRIGGER_MIN_US || pulse_us > FLASH_TRIGGER_MAX_US) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&flash_lock);
    if (!flash.initialized || flash.busy || flash.submitting) {
        portEXIT_CRITICAL(&flash_lock);
        return ESP_ERR_INVALID_STATE;
    }
    flash.pulse = (rmt_symbol_word_t){
        .duration0 = pulse_us,
        .level0 = 1,
        .duration1 = FLASH_TRIGGER_RECOVERY_US,
        .level1 = 0,
    };
    flash.busy = true;
    flash.submitting = true;
    portEXIT_CRITICAL(&flash_lock);

    const rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
        .flags.eot_level = 0,
        .flags.queue_nonblocking = true,
    };
    const esp_err_t result = rmt_transmit(flash.channel, flash.encoder, &flash.pulse,
                                         sizeof(flash.pulse), &transmit_config);
    /* A completion ISR can arrive before rmt_transmit() returns. Keep the
     * submission gate closed until the driver call has finished as well. */
    portENTER_CRITICAL(&flash_lock);
    if (result != ESP_OK) {
        flash.busy = false;
    }
    flash.submitting = false;
    portEXIT_CRITICAL(&flash_lock);
    return result;
}

esp_err_t flash_trigger_fire(void)
{
    return flash_trigger_fire_us(FLASH_TRIGGER_DEFAULT_US);
}

bool flash_trigger_is_busy(void)
{
    portENTER_CRITICAL(&flash_lock);
    const bool busy = flash.busy || flash.submitting;
    portEXIT_CRITICAL(&flash_lock);
    return busy;
}
