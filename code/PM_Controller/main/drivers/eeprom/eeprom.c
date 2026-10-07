#include "eeprom.h"

#include <string.h>
#include <stdbool.h>
#include "PIN.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define EEPROM_I2C_CLOCK_HZ 100000
#define EEPROM_TRANSFER_TIMEOUT_MS 50
#define EEPROM_WRITE_TIMEOUT_MS 20

static struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    SemaphoreHandle_t mutex;
    bool initialized;
} eeprom;

static void release_resources(void)
{
    if (eeprom.device) {
        i2c_master_bus_rm_device(eeprom.device);
        eeprom.device = NULL;
    }
    if (eeprom.bus) {
        i2c_del_master_bus(eeprom.bus);
        eeprom.bus = NULL;
    }
    if (eeprom.mutex) {
        vSemaphoreDelete(eeprom.mutex);
        eeprom.mutex = NULL;
    }
}

esp_err_t eeprom_init(void)
{
    if (eeprom.initialized) {
        return ESP_OK;
    }
    eeprom.mutex = xSemaphoreCreateMutex();
    if (!eeprom.mutex) {
        return ESP_ERR_NO_MEM;
    }
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .scl_io_num = PIN_ESP_SCL,
        .sda_io_num = PIN_ESP_SDA,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, /* R15/R16 provide 4.7 kOhm pullups. */
    };
    esp_err_t result = i2c_new_master_bus(&bus_config, &eeprom.bus);
    if (result != ESP_OK) {
        goto fail;
    }
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = EEPROM_DEVICE_ADDRESS,
        .scl_speed_hz = EEPROM_I2C_CLOCK_HZ,
    };
    result = i2c_master_bus_add_device(eeprom.bus, &device_config, &eeprom.device);
    if (result != ESP_OK) {
        goto fail;
    }
    result = i2c_master_probe(eeprom.bus, EEPROM_DEVICE_ADDRESS, EEPROM_TRANSFER_TIMEOUT_MS);
    if (result != ESP_OK) {
        goto fail;
    }
    eeprom.initialized = true;
    return ESP_OK;

fail:
    release_resources();
    return result;
}

static esp_err_t check_access(uint32_t address, const void *data, size_t length)
{
    if (!eeprom.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if ((!data && length) || address > EEPROM_CAPACITY_BYTES ||
        length > EEPROM_CAPACITY_BYTES - address) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t eeprom_read(uint32_t address, void *data, size_t length)
{
    esp_err_t result = check_access(address, data, length);
    if (result != ESP_OK || length == 0) {
        return result;
    }
    xSemaphoreTake(eeprom.mutex, portMAX_DELAY);
    uint8_t *destination = data;
    while (length) {
        /* Keep each transfer below the 50 ms timeout, even for an 8 KB read. */
        const size_t chunk = length < 256 ? length : 256;
        const uint8_t word_address[] = {(uint8_t)(address >> 8), (uint8_t)address};
        result = i2c_master_transmit_receive(eeprom.device, word_address, sizeof(word_address),
                                            destination, chunk, EEPROM_TRANSFER_TIMEOUT_MS);
        if (result != ESP_OK) {
            break;
        }
        address += (uint32_t)chunk;
        destination += chunk;
        length -= chunk;
    }
    xSemaphoreGive(eeprom.mutex);
    return result;
}

static esp_err_t wait_until_ready(void)
{
    const int64_t started_us = esp_timer_get_time();
    while (true) {
        const esp_err_t result = i2c_master_probe(eeprom.bus, EEPROM_DEVICE_ADDRESS, 5);
        if (result != ESP_ERR_NOT_FOUND) {
            return result;
        }
        if (esp_timer_get_time() - started_us >= EEPROM_WRITE_TIMEOUT_MS * 1000) {
            return ESP_ERR_TIMEOUT;
        }
        /* At 100 Hz, a 1 ms delay rounds to zero. Wait at least one tick. */
        const TickType_t delay = pdMS_TO_TICKS(1);
        vTaskDelay(delay ? delay : 1);
    }
}

esp_err_t eeprom_write(uint32_t address, const void *data, size_t length)
{
    esp_err_t result = check_access(address, data, length);
    if (result != ESP_OK || length == 0) {
        return result;
    }
    const uint8_t *source = data;
    xSemaphoreTake(eeprom.mutex, portMAX_DELAY);
    while (length) {
        const size_t page_remaining = EEPROM_PAGE_BYTES - address % EEPROM_PAGE_BYTES;
        const size_t chunk = length < page_remaining ? length : page_remaining;
        uint8_t packet[2 + EEPROM_PAGE_BYTES];
        packet[0] = (uint8_t)(address >> 8);
        packet[1] = (uint8_t)address;
        memcpy(packet + 2, source, chunk);
        result = i2c_master_transmit(eeprom.device, packet, chunk + 2, EEPROM_TRANSFER_TIMEOUT_MS);
        if (result != ESP_OK) {
            break;
        }
        result = wait_until_ready();
        if (result != ESP_OK) {
            break;
        }
        address += (uint32_t)chunk;
        source += chunk;
        length -= chunk;
    }
    xSemaphoreGive(eeprom.mutex);
    return result;
}
