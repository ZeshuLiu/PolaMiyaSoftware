#include "usb_power.h"
#include "PIN.h"

static bool ready;

esp_err_t usb_power_init(void)
{
    /* U18 /RESET is pulled to +3.3 V by R56. No internal pull or IRQ needed. */
    const gpio_config_t input = {
        .pin_bit_mask = 1ULL << PIN_USBPWR_DET,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t result = gpio_config(&input);
    ready = result == ESP_OK;
    return result;
}

esp_err_t usb_power_get(bool *present)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    *present = gpio_get_level(PIN_USBPWR_DET) != 0;
    return ESP_OK;
}
