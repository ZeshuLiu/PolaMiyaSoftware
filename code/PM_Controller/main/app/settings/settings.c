#include "settings.h"

#include <string.h>
#include "eeprom.h"
#include "settings_record.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define FIRST_RECORD_ADDRESS 0x100U /* Preserve Rev1 bytes and reserve the first 256 bytes. */
#define RECORD_COUNT ((EEPROM_CAPACITY_BYTES - FIRST_RECORD_ADDRESS) / SETTINGS_RECORD_BYTES)

static struct {
    SemaphoreHandle_t mutex;
    settings_values_t values;
    settings_values_t persisted_values;
    settings_status_t status;
    int active_record;
} store;

static bool sequence_is_newer(uint32_t candidate, uint32_t current)
{
    return candidate != current && (uint32_t)(candidate - current) < 0x80000000U;
}

static esp_err_t load_records(void)
{
    bool found = false;
    esp_err_t newest_result = ESP_OK;
    for (unsigned int index = 0; index < RECORD_COUNT; ++index) {
        uint8_t record[SETTINGS_RECORD_BYTES];
        esp_err_t result = eeprom_read(FIRST_RECORD_ADDRESS + index * SETTINGS_RECORD_BYTES,
                                       record, sizeof(record));
        if (result != ESP_OK) {
            /* An unreadable slot may contain the newest record. Never overwrite
             * the EEPROM based on a partial scan. */
            return result;
        }
        uint32_t sequence;
        if (!settings_record_valid(record, &sequence)) {
            continue;
        }
        settings_values_t values;
        result = settings_record_decode(record, &values);
        if (result != ESP_OK && result != ESP_ERR_NOT_SUPPORTED) {
            continue;
        }
        if (!found || sequence_is_newer(sequence, store.status.sequence)) {
            found = true;
            store.active_record = (int)index;
            store.status.sequence = sequence;
            newest_result = result;
            if (result == ESP_OK) {
                store.values = values;
            }
        }
    }
    if (newest_result != ESP_OK) {
        settings_values_default(&store.values);
        return newest_result;
    }
    store.status.loaded_from_eeprom = found;
    store.status.dirty = !found;
    store.persisted_values = store.values;
    return ESP_OK;
}

esp_err_t settings_init(void)
{
    if (store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    store.mutex = xSemaphoreCreateMutex();
    if (!store.mutex) {
        return ESP_ERR_NO_MEM;
    }
    settings_values_default(&store.values);
    store.active_record = -1;
    store.status.dirty = true;
    esp_err_t result = eeprom_init();
    if (result == ESP_OK) {
        result = load_records();
    }
    if (result != ESP_OK) {
        settings_values_default(&store.values);
        if (result != ESP_ERR_NOT_SUPPORTED) {
            store.active_record = -1;
            store.status.sequence = 0;
        }
    }
    store.status.storage_error = result;
    store.status.writable = result == ESP_OK;
    return result;
}

esp_err_t settings_get(settings_values_t *values)
{
    if (!values) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    *values = store.values;
    xSemaphoreGive(store.mutex);
    return ESP_OK;
}

esp_err_t settings_get_status(settings_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    *status = store.status;
    xSemaphoreGive(store.mutex);
    return ESP_OK;
}

esp_err_t settings_set(settings_key_t key, uint64_t value)
{
    if (!store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    const esp_err_t result = settings_values_set(&store.values, key, value);
    if (result == ESP_OK) {
        store.status.dirty = !store.status.loaded_from_eeprom ||
            memcmp(&store.values, &store.persisted_values, sizeof(store.values)) != 0;
    }
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_increment(settings_key_t key, uint64_t amount)
{
    if (!store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    const esp_err_t result = settings_values_increment(&store.values, key, amount);
    if (result == ESP_OK) {
        store.status.dirty = !store.status.loaded_from_eeprom ||
            memcmp(&store.values, &store.persisted_values, sizeof(store.values)) != 0;
    }
    xSemaphoreGive(store.mutex);
    return result;
}

static esp_err_t write_record(unsigned int index, const uint8_t *record)
{
    const uint32_t address = FIRST_RECORD_ADDRESS + index * SETTINGS_RECORD_BYTES;
    const uint8_t invalid_marker[SETTINGS_COMMIT_BYTES] = {0};
    uint8_t verify[SETTINGS_RECORD_BYTES];
    esp_err_t result = eeprom_write(address + SETTINGS_COMMIT_OFFSET,
                                    invalid_marker, sizeof(invalid_marker));
    if (result != ESP_OK) {
        return result;
    }
    result = eeprom_read(address + SETTINGS_COMMIT_OFFSET, verify, sizeof(invalid_marker));
    if (result != ESP_OK) {
        return result;
    }
    if (memcmp(verify, invalid_marker, sizeof(invalid_marker)) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    const size_t body_size = settings_record_body_size(record);
    result = eeprom_write(address, record, body_size);
    if (result != ESP_OK) {
        return result;
    }
    result = eeprom_read(address, verify, body_size);
    if (result != ESP_OK) {
        return result;
    }
    if (memcmp(verify, record, body_size) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    /* The last marker uses its own physical page, leaving the previous
     * committed record intact throughout an interrupted save. */
    result = eeprom_write(address + SETTINGS_COMMIT_OFFSET,
                          record + SETTINGS_COMMIT_OFFSET, SETTINGS_COMMIT_BYTES);
    if (result != ESP_OK) {
        return result;
    }
    result = eeprom_read(address + SETTINGS_COMMIT_OFFSET, verify, SETTINGS_COMMIT_BYTES);
    if (result != ESP_OK) {
        return result;
    }
    return memcmp(verify, record + SETTINGS_COMMIT_OFFSET, SETTINGS_COMMIT_BYTES) == 0
        ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t settings_commit(void)
{
    if (!store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    esp_err_t result = store.status.writable ? ESP_OK : store.status.storage_error;
    if (result == ESP_OK && store.status.dirty) {
        uint8_t record[SETTINGS_RECORD_BYTES];
        const unsigned int next = store.active_record < 0 ? 0
            : ((unsigned int)store.active_record + 1) % RECORD_COUNT;
        const uint32_t sequence = store.status.sequence + 1;
        result = settings_record_encode(&store.values, sequence, record);
        if (result == ESP_OK) {
            result = write_record(next, record);
        }
        if (result == ESP_OK) {
            store.active_record = (int)next;
            store.status.sequence = sequence;
            store.status.loaded_from_eeprom = true;
            store.status.dirty = false;
            store.persisted_values = store.values;
        }
    }
    store.status.storage_error = result;
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_import_rev1(void)
{
    if (!store.mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    esp_err_t result = store.status.writable ? ESP_OK : store.status.storage_error;
    if (result == ESP_OK && store.active_record >= 0) {
        result = ESP_ERR_INVALID_STATE;
    }
    uint8_t legacy[13];
    if (result == ESP_OK) {
        result = eeprom_read(0x20, legacy, sizeof(legacy));
    }
    if (result == ESP_OK && legacy[0] != 2) {
        result = ESP_ERR_NOT_SUPPORTED;
    }
    if (result == ESP_OK) {
        settings_values_default(&store.values);
        const settings_key_t keys[] = {SETTINGS_REBOOT_COUNT, SETTINGS_SHUTTER_COUNT, SETTINGS_MOTOR_RUN_MS};
        for (unsigned int i = 0; i < 3; ++i) {
            uint32_t value = 0;
            for (unsigned int byte = 0; byte < 4; ++byte) {
                value |= (uint32_t)legacy[1 + 4 * i + byte] << (8 * byte);
            }
            settings_values_set(&store.values, keys[i], value);
        }
        store.status.dirty = true;
    }
    xSemaphoreGive(store.mutex);
    return result;
}
