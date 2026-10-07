#ifndef PM_SETTINGS_RECORD_H
#define PM_SETTINGS_RECORD_H

#include <stddef.h>
#include "settings.h"

#define SETTINGS_RECORD_BYTES 256U
#define SETTINGS_HEADER_BYTES 16U
#define SETTINGS_COMMIT_OFFSET 224U
#define SETTINGS_COMMIT_BYTES 4U
#define SETTINGS_RECORD_VERSION 1U

void settings_values_default(settings_values_t *values);
esp_err_t settings_values_set(settings_values_t *values, settings_key_t key, uint64_t value);
esp_err_t settings_values_increment(settings_values_t *values, settings_key_t key, uint64_t amount);
esp_err_t settings_record_encode(const settings_values_t *values, uint32_t sequence,
                                 uint8_t record[SETTINGS_RECORD_BYTES]);
bool settings_record_valid(const uint8_t record[SETTINGS_RECORD_BYTES], uint32_t *sequence);
esp_err_t settings_record_decode(const uint8_t record[SETTINGS_RECORD_BYTES], settings_values_t *values);
size_t settings_record_body_size(const uint8_t record[SETTINGS_RECORD_BYTES]);

#endif
