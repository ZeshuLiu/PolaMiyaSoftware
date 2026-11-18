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
    bool commit_uncertain;
    bool calibration_migration;
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
        settings_values_t values;
        bool calibration_missing = false;
        result = settings_record_decode(record, &values, &sequence, &calibration_missing);
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
                store.calibration_migration = calibration_missing;
            }
        }
    }
    if (newest_result != ESP_OK) {
        settings_values_default(&store.values);
        return newest_result;
    }
    store.status.loaded_from_eeprom = found;
    store.status.dirty = !found || store.calibration_migration;
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
        store.status.dirty = store.commit_uncertain || store.calibration_migration || !store.status.loaded_from_eeprom ||
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
        store.status.dirty = store.commit_uncertain || store.calibration_migration || !store.status.loaded_from_eeprom ||
            memcmp(&store.values, &store.persisted_values, sizeof(store.values)) != 0;
    }
    xSemaphoreGive(store.mutex);
    return result;
}

/* Callers pass at most one record; reuse this readback for each commit stage. */
static esp_err_t write_verified(uint32_t address, const uint8_t *data, size_t length)
{
    uint8_t verify[SETTINGS_RECORD_BYTES];
    esp_err_t result = eeprom_write(address, data, length);
    if (result != ESP_OK) {
        return result;
    }
    result = eeprom_read(address, verify, length);
    if (result != ESP_OK) {
        return result;
    }
    return memcmp(verify, data, length) == 0 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t write_record(unsigned int index, const uint8_t *record)
{
    const uint32_t address = FIRST_RECORD_ADDRESS + index * SETTINGS_RECORD_BYTES;
    const uint8_t invalid_marker[SETTINGS_COMMIT_BYTES] = {0};
    esp_err_t result = write_verified(address + SETTINGS_COMMIT_OFFSET,
                                      invalid_marker, sizeof(invalid_marker));
    if (result != ESP_OK) {
        return result;
    }
    result = write_verified(address, record, settings_record_body_size(record));
    if (result != ESP_OK) {
        return result;
    }
    /* The last marker uses its own physical page, leaving the previous
     * committed record intact throughout an interrupted save. */
    return write_verified(address + SETTINGS_COMMIT_OFFSET,
                          record + SETTINGS_COMMIT_OFFSET, SETTINGS_COMMIT_BYTES);
}

/* Caller holds the parameter mutex. A selected-field save uses the last persisted
 * values for every other field; never silently save an unconfirmed backlight edit. */
static esp_err_t commit_values_locked(const settings_values_t *values)
{
    esp_err_t result = store.status.writable ? ESP_OK : store.status.storage_error;
    if (result == ESP_OK && (store.commit_uncertain || store.calibration_migration || !store.status.loaded_from_eeprom ||
                            memcmp(values, &store.persisted_values, sizeof(*values)) != 0)) {
        uint8_t record[SETTINGS_RECORD_BYTES];
        const unsigned int next = store.active_record < 0 ? 0
            : ((unsigned int)store.active_record + 1) % RECORD_COUNT;
        const uint32_t sequence = store.status.sequence + 1;
        settings_record_encode(values, sequence, record);
        result = write_record(next, record);
        /* A failed final readback may hide a physically committed record. Even
         * equal values must overwrite and verify that slot before motion. */
        store.commit_uncertain = result != ESP_OK;
        if (result == ESP_OK) {
            store.active_record = (int)next;
            store.status.sequence = sequence;
            store.status.loaded_from_eeprom = true;
            store.persisted_values = *values;
            store.calibration_migration = false;
        }
    }
    store.status.dirty = store.commit_uncertain || store.calibration_migration || !store.status.loaded_from_eeprom ||
        memcmp(&store.values, &store.persisted_values, sizeof(store.values)) != 0;
    store.status.storage_error = result;
    return result;
}

esp_err_t settings_commit(void)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    const esp_err_t result = commit_values_locked(&store.values);
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_commit_shutter(void)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    settings_values_t values = store.persisted_values;
    values.shutter_count = store.values.shutter_count;
    const esp_err_t result = commit_values_locked(&values);
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_save_wifi_enabled(bool enabled)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    store.values.wifi_enabled = enabled ? 1 : 0;
    settings_values_t values = store.persisted_values;
    values.wifi_enabled = store.values.wifi_enabled;
    const esp_err_t result = commit_values_locked(&values);
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_save_ble_remote_enabled(bool enabled)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    store.values.ble_remote_enabled = enabled ? 1 : 0;
    settings_values_t values = store.persisted_values;
    values.ble_remote_enabled = store.values.ble_remote_enabled;
    const esp_err_t result = commit_values_locked(&values);
    xSemaphoreGive(store.mutex);
    return result;
}

static esp_err_t commit_film_locked(void)
{
    settings_values_t values = store.persisted_values;
    values.film_remaining = store.values.film_remaining;
    return commit_values_locked(&values);
}

esp_err_t settings_commit_film(void)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    const esp_err_t result = commit_film_locked();
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_consume_film(void)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    if (store.values.film_remaining > 0) --store.values.film_remaining;
    const esp_err_t result = commit_film_locked();
    xSemaphoreGive(store.mutex);
    return result;
}

static void calibration_from_values(const settings_values_t *values, focus_calibration_t *cal)
{
    memcpy(&cal->offset_raw, &values->cal_offset, sizeof(double));
    memcpy(&cal->a_raw_m, &values->cal_a, sizeof(double));
    memcpy(&cal->b_m, &values->cal_b, sizeof(double));
    cal->min_distance_mm = (uint32_t)values->cal_min_mm;
    cal->infinity_tolerance_raw = (uint32_t)values->cal_inf_tol;
}

esp_err_t settings_get_calibration(focus_calibration_t *cal)
{
    if (!cal) return ESP_ERR_INVALID_ARG;
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    calibration_from_values(&store.values, cal);
    xSemaphoreGive(store.mutex);
    return ESP_OK;
}

esp_err_t settings_save_calibration(const focus_calibration_t *cal)
{
    if (!cal || !focus_calibration_valid(cal)) return ESP_ERR_INVALID_ARG;
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    settings_values_t values = store.persisted_values;
    memcpy(&values.cal_offset, &cal->offset_raw, sizeof(double));
    memcpy(&values.cal_a, &cal->a_raw_m, sizeof(double));
    memcpy(&values.cal_b, &cal->b_m, sizeof(double));
    values.cal_min_mm = cal->min_distance_mm;
    values.cal_inf_tol = cal->infinity_tolerance_raw;
    const esp_err_t result = commit_values_locked(&values);
    if (result == ESP_OK) {
        store.values.cal_offset = values.cal_offset;
        store.values.cal_a = values.cal_a;
        store.values.cal_b = values.cal_b;
        store.values.cal_min_mm = values.cal_min_mm;
        store.values.cal_inf_tol = values.cal_inf_tol;
        store.status.dirty = memcmp(&store.values, &store.persisted_values, sizeof(store.values)) != 0;
    }
    xSemaphoreGive(store.mutex);
    return result;
}

esp_err_t settings_load_focus(bool *valid, int32_t *raw)
{
    if (!valid || !raw) return ESP_ERR_INVALID_ARG;
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    const uint32_t bits = (uint32_t)store.persisted_values.focus_raw;
    *raw = bits <= INT32_MAX ? (int32_t)bits : (int32_t)((int64_t)bits - 0x100000000LL);
    *valid = store.status.writable && store.status.loaded_from_eeprom &&
             store.persisted_values.focus_valid == 1;
    xSemaphoreGive(store.mutex);
    return ESP_OK;
}

esp_err_t settings_save_focus(bool valid, int32_t raw)
{
    if (!store.mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(store.mutex, portMAX_DELAY);
    store.values.focus_raw = (uint32_t)raw;
    store.values.focus_valid = valid ? 1 : 0;
    settings_values_t values = store.persisted_values;
    values.focus_raw = store.values.focus_raw;
    values.focus_valid = store.values.focus_valid;
    const esp_err_t result = commit_values_locked(&values);
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
