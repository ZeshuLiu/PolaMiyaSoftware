#include "settings_record.h"

#include <string.h>
#include <float.h>

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
    {SETTINGS_FILM_REMAINING, offsetof(settings_values_t, film_remaining), 6, 8, false},
    {SETTINGS_FOCUS_RAW, offsetof(settings_values_t, focus_raw), 0, UINT32_MAX, false},
    {SETTINGS_FOCUS_VALID, offsetof(settings_values_t, focus_valid), 0, 1, false},
    {SETTINGS_CAL_OFFSET, offsetof(settings_values_t, cal_offset), 0, UINT64_MAX, false},
    {SETTINGS_CAL_A, offsetof(settings_values_t, cal_a), 0, UINT64_MAX, false},
    {SETTINGS_CAL_B, offsetof(settings_values_t, cal_b), 0, UINT64_MAX, false},
    {SETTINGS_CAL_MIN_MM, offsetof(settings_values_t, cal_min_mm), FOCUS_CALIBRATION_MIN_DISTANCE_MM, UINT32_MAX, false},
    {SETTINGS_CAL_INF_TOL, offsetof(settings_values_t, cal_inf_tol), FOCUS_CALIBRATION_INFINITY_TOLERANCE_RAW, UINT32_MAX, false},
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

static void calibration_defaults(settings_values_t *values)
{
    const double offset = FOCUS_CALIBRATION_OFFSET_RAW, a = FOCUS_CALIBRATION_A_RAW_M, b = FOCUS_CALIBRATION_B_M;
    _Static_assert(sizeof(double) == sizeof(uint64_t) && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
                   "Calibration requires IEEE-754 binary64");
    memcpy(&values->cal_offset, &offset, sizeof(offset));
    memcpy(&values->cal_a, &a, sizeof(a));
    memcpy(&values->cal_b, &b, sizeof(b));
    values->cal_min_mm = FOCUS_CALIBRATION_MIN_DISTANCE_MM;
    values->cal_inf_tol = FOCUS_CALIBRATION_INFINITY_TOLERANCE_RAW;
}

void settings_values_default(settings_values_t *values)
{
    memset(values, 0, sizeof(*values));
    for (size_t i = 0; i < PARAMETER_COUNT; ++i) {
        *field(values, i) = parameters[i].default_value;
    }
    calibration_defaults(values);
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

void settings_record_encode(const settings_values_t *values, uint32_t sequence,
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
        uint8_t *entry = record + SETTINGS_HEADER_BYTES + i * ENTRY_BYTES;
        write_le(entry, parameters[i].key, 2);
        write_le(entry + 2, 8, 2);
        write_le(entry + 4, value, 8);
    }
    write_le(record + 12, record_crc(record), 4);
    write_le(record + SETTINGS_COMMIT_OFFSET, COMMIT_MAGIC, SETTINGS_COMMIT_BYTES);
}

static bool settings_record_valid(const uint8_t record[SETTINGS_RECORD_BYTES], uint32_t *sequence)
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

esp_err_t settings_record_decode(const uint8_t record[SETTINGS_RECORD_BYTES],
                                 settings_values_t *values, uint32_t *sequence, bool *calibration_missing)
{
    if (!settings_record_valid(record, sequence)) {
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
    /* A partial focus pair must never establish a valid position. Old records
     * with neither field remain compatible and require homing. */
    if (!seen[find_parameter(SETTINGS_FOCUS_RAW)] ||
        !seen[find_parameter(SETTINGS_FOCUS_VALID)]) values->focus_valid = 0;
    bool complete = true;
    for (int key = SETTINGS_CAL_OFFSET; key <= SETTINGS_CAL_INF_TOL; ++key)
        complete = complete && seen[find_parameter((settings_key_t)key)];
    focus_calibration_t cal;
    memcpy(&cal.offset_raw, &values->cal_offset, sizeof(double));
    memcpy(&cal.a_raw_m, &values->cal_a, sizeof(double));
    memcpy(&cal.b_m, &values->cal_b, sizeof(double));
    cal.min_distance_mm = (uint32_t)values->cal_min_mm;
    cal.infinity_tolerance_raw = (uint32_t)values->cal_inf_tol;
    if (!complete || !focus_calibration_valid(&cal)) calibration_defaults(values);
    if (calibration_missing) *calibration_missing = !complete || !focus_calibration_valid(&cal);
    /* Don't allow old firmware to erase parameters introduced by newer firmware. */
    return unknown_parameter ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
}
