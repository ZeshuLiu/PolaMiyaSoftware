#include "settings_record.h"

#include <string.h>

typedef struct {
    settings_key_t key;
    size_t offset;
    uint64_t default_value;
    uint64_t maximum;
    bool counter;
} parameter_t;

/* One schema table owns defaults, ranges, encoding and update validation. */
static const parameter_t parameters[] = {
    {SETTINGS_REBOOT_COUNT, offsetof(settings_values_t, reboot_count), 0, UINT64_MAX, true},
    {SETTINGS_SHUTTER_COUNT, offsetof(settings_values_t, shutter_count), 0, UINT64_MAX, true},
    {SETTINGS_MOTOR_RUN_MS, offsetof(settings_values_t, motor_run_ms), 0, UINT64_MAX, true},
    {SETTINGS_BACKLIGHT_PERCENT, offsetof(settings_values_t, backlight_percent), 100, 100, false},
};
#define PARAMETER_COUNT (sizeof(parameters) / sizeof(parameters[0]))
#define ENTRY_BYTES 12U /* ID: u16, length: u16, value: u64; all little-endian. */
#define RECORD_MAGIC 0x53504D50U /* "PMPS" */
#define COMMIT_MAGIC 0x45564153U /* "SAVE" */

_Static_assert(SETTINGS_HEADER_BYTES + PARAMETER_COUNT * ENTRY_BYTES <= SETTINGS_COMMIT_OFFSET,
               "Parameter payload overlaps the commit page");

static uint64_t read_le(const uint8_t *bytes, size_t length)
{
    uint64_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        value |= (uint64_t)bytes[i] << (8 * i);
    }
    return value;
}

static void write_le(uint8_t *bytes, uint64_t value, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        bytes[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint64_t *field(settings_values_t *values, size_t index)
{
    return (uint64_t *)((uint8_t *)values + parameters[index].offset);
}

static int find_parameter(settings_key_t key)
{
    for (size_t i = 0; i < PARAMETER_COUNT; ++i) {
        if (parameters[i].key == key) {
            return (int)i;
        }
    }
    return -1;
}

void settings_values_default(settings_values_t *values)
{
    memset(values, 0, sizeof(*values));
    for (size_t i = 0; i < PARAMETER_COUNT; ++i) {
        *field(values, i) = parameters[i].default_value;
    }
}

esp_err_t settings_values_set(settings_values_t *values, settings_key_t key, uint64_t value)
{
    const int index = find_parameter(key);
    if (index < 0 || value > parameters[index].maximum) {
        return ESP_ERR_INVALID_ARG;
    }
    *field(values, (size_t)index) = value;
    return ESP_OK;
}

esp_err_t settings_values_increment(settings_values_t *values, settings_key_t key, uint64_t amount)
{
    const int index = find_parameter(key);
    if (index < 0 || !parameters[index].counter) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t *value = field(values, (size_t)index);
    if (amount > parameters[index].maximum - *value) {
        return ESP_ERR_INVALID_ARG;
    }
    *value += amount;
    return ESP_OK;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0);
        }
    }
    return crc;
}

size_t settings_record_body_size(const uint8_t record[SETTINGS_RECORD_BYTES])
{
    return SETTINGS_HEADER_BYTES + (size_t)read_le(record + 6, 2);
}

static uint32_t record_crc(const uint8_t *record)
{
    uint32_t crc = crc32_update(UINT32_MAX, record, 12);
    crc = crc32_update(crc, record + SETTINGS_HEADER_BYTES, (size_t)read_le(record + 6, 2));
    return crc ^ UINT32_MAX;
}

esp_err_t settings_record_encode(const settings_values_t *values, uint32_t sequence,
                                 uint8_t record[SETTINGS_RECORD_BYTES])
{
    memset(record, 0xFF, SETTINGS_RECORD_BYTES);
    write_le(record, RECORD_MAGIC, 4);
    write_le(record + 4, SETTINGS_RECORD_VERSION, 2);
    write_le(record + 6, PARAMETER_COUNT * ENTRY_BYTES, 2);
    write_le(record + 8, sequence, 4);
    for (size_t i = 0; i < PARAMETER_COUNT; ++i) {
        uint64_t value;
        memcpy(&value, (const uint8_t *)values + parameters[i].offset, sizeof(value));
        if (value > parameters[i].maximum) {
            return ESP_ERR_INVALID_ARG;
        }
        uint8_t *entry = record + SETTINGS_HEADER_BYTES + i * ENTRY_BYTES;
        write_le(entry, parameters[i].key, 2);
        write_le(entry + 2, 8, 2);
        write_le(entry + 4, value, 8);
    }
    write_le(record + 12, record_crc(record), 4);
    write_le(record + SETTINGS_COMMIT_OFFSET, COMMIT_MAGIC, SETTINGS_COMMIT_BYTES);
    return ESP_OK;
}

bool settings_record_valid(const uint8_t record[SETTINGS_RECORD_BYTES], uint32_t *sequence)
{
    if (read_le(record, 4) != RECORD_MAGIC ||
        settings_record_body_size(record) > SETTINGS_COMMIT_OFFSET ||
        read_le(record + SETTINGS_COMMIT_OFFSET, SETTINGS_COMMIT_BYTES) != COMMIT_MAGIC ||
        read_le(record + 12, 4) != record_crc(record)) {
        return false;
    }
    *sequence = (uint32_t)read_le(record + 8, 4);
    return true;
}

esp_err_t settings_record_decode(const uint8_t record[SETTINGS_RECORD_BYTES], settings_values_t *values)
{
    uint32_t sequence;
    if (!settings_record_valid(record, &sequence)) {
        return ESP_ERR_INVALID_CRC;
    }
    if (read_le(record + 4, 2) != SETTINGS_RECORD_VERSION) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    settings_values_default(values);
    bool seen[PARAMETER_COUNT] = {false};
    bool unknown_parameter = false;
    const size_t end = settings_record_body_size(record);
    for (size_t cursor = SETTINGS_HEADER_BYTES; cursor < end;) {
        if (end - cursor < 4) {
            return ESP_ERR_INVALID_SIZE;
        }
        const uint16_t key = (uint16_t)read_le(record + cursor, 2);
        const size_t length = (size_t)read_le(record + cursor + 2, 2);
        if (length > end - cursor - 4) {
            return ESP_ERR_INVALID_SIZE;
        }
        const int index = find_parameter((settings_key_t)key);
        if (index < 0) {
            unknown_parameter = true;
        } else {
            if (seen[index] || length != 8) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            seen[index] = true;
            const esp_err_t result = settings_values_set(values, (settings_key_t)key,
                                                         read_le(record + cursor + 4, 8));
            if (result != ESP_OK) {
                return result;
            }
        }
        cursor += 4 + length;
    }
    /* Don't allow old firmware to erase parameters introduced by newer firmware. */
    return unknown_parameter ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
}
