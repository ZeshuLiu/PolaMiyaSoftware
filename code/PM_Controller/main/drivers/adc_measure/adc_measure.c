#include "adc_measure.h"

#include <string.h>
#include "PIN.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

#define FRAME_BYTES 128
#define CHANNEL_COUNT 2
#define SAMPLE_FREQ_HZ SOC_ADC_SAMPLE_FREQ_THRES_LOW
#define SNAPSHOT_HZ 10
#define WINDOW_SAMPLES ((SAMPLE_FREQ_HZ + CHANNEL_COUNT * SNAPSHOT_HZ - 1) / (CHANNEL_COUNT * SNAPSHOT_HZ))
#define STALE_US 300000

enum { BATTERY, MOTOR };
static const char *TAG = "adc_measure";
static adc_continuous_handle_t adc;
static adc_channel_t channels[CHANNEL_COUNT];
static adc_cali_handle_t calibration[CHANNEL_COUNT];
static portMUX_TYPE sample_lock = portMUX_INITIALIZER_UNLOCKED;
static adc_measurement_t latest;
static uint32_t overflows;
static bool initialized;

static bool pool_overflow(adc_continuous_handle_t handle,
                          const adc_continuous_evt_data_t *event, void *context)
{
    (void)handle;
    (void)event;
    (void)context;
    /* No conversion, logging, UI or recovery work in the ADC interrupt. */
    portENTER_CRITICAL_ISR(&sample_lock);
    ++overflows;
    portEXIT_CRITICAL_ISR(&sample_lock);
    return false;
}

static uint32_t overflow_count(void)
{
    portENTER_CRITICAL(&sample_lock);
    const uint32_t count = overflows;
    portEXIT_CRITICAL(&sample_lock);
    return count;
}

static void stream_error(esp_err_t error)
{
    portENTER_CRITICAL(&sample_lock);
    latest.error = error;
    latest.battery.valid = false;
    latest.motor.valid = false;
    portEXIT_CRITICAL(&sample_lock);
}

static adc_channel_sample_t convert_window(unsigned channel, uint32_t sum, unsigned count, bool saturated,
                                           int32_t *filter_q8, bool *filter_ready)
{
    adc_channel_sample_t result = {
        .raw = (uint16_t)((sum + count / 2) / count),
        .valid = true,
        .saturated = saturated,
    };
    if (saturated) {
        *filter_ready = false;
        return result;
    }
    int mv;
    if (calibration[channel] && adc_cali_raw_to_voltage(calibration[channel], result.raw, &mv) == ESP_OK) {
        result.voltage_mv = (uint16_t)mv;
        result.calibrated = true;
        if (!*filter_ready) {
            *filter_q8 = mv * 256;
            *filter_ready = true;
        } else {
            *filter_q8 += (mv * 256 - *filter_q8) / 8;
        }
        result.filtered_mv = (uint16_t)((*filter_q8 + 128) / 256);
    } else {
        *filter_ready = false;
    }
    return result;
}

static void sampling_task(void *context)
{
    (void)context;
    uint8_t raw[FRAME_BYTES];
    adc_continuous_data_t parsed[FRAME_BYTES / SOC_ADC_DIGI_RESULT_BYTES];
    uint32_t sums[CHANNEL_COUNT] = {0};
    unsigned counts[CHANNEL_COUNT] = {0};
    bool saturated[CHANNEL_COUNT] = {false};
    int32_t filter_q8[CHANNEL_COUNT] = {0};
    bool filter_ready[CHANNEL_COUNT] = {false};
    uint32_t accepted_overflows = 0;
    while (1) {
        if (overflow_count() != accepted_overflows) {
            stream_error(ESP_ERR_INVALID_STATE);
            /* Dropped DMA data must not enter a seemingly fresh average. */
            esp_err_t error = adc_continuous_stop(adc);
            if (error == ESP_OK) error = adc_continuous_flush_pool(adc);
            accepted_overflows = overflow_count();
            if (error == ESP_OK) error = adc_continuous_start(adc);
            if (error != ESP_OK) {
                stream_error(error);
                ESP_LOGE(TAG, "ADC overflow recovery failed: %s", esp_err_to_name(error));
                vTaskSuspend(NULL); /* Reboot is required after a failed restart. */
            }
            memset(sums, 0, sizeof(sums));
            memset(counts, 0, sizeof(counts));
            memset(saturated, 0, sizeof(saturated));
            memset(filter_ready, 0, sizeof(filter_ready));
            ESP_LOGW(TAG, "ADC pool overflow; discarded window and restarted");
        }
        uint32_t bytes = 0;
        esp_err_t error = adc_continuous_read(adc, raw, sizeof(raw), &bytes, 100);
        uint32_t parsed_count = 0;
        if (error == ESP_OK) {
            error = adc_continuous_parse_data(adc, raw, bytes, parsed, &parsed_count);
        }
        if (error != ESP_OK) {
            stream_error(error);
            memset(sums, 0, sizeof(sums));
            memset(counts, 0, sizeof(counts));
            memset(saturated, 0, sizeof(saturated));
            memset(filter_ready, 0, sizeof(filter_ready));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        for (unsigned i = 0; i < parsed_count; ++i) {
            const adc_continuous_data_t *value = &parsed[i];
            if (!value->valid || value->unit != ADC_UNIT_1) continue;
            int channel = value->channel == channels[BATTERY] ? BATTERY :
                          value->channel == channels[MOTOR] ? MOTOR : -1;
            if (channel < 0) continue;
            sums[channel] += value->raw_data;
            ++counts[channel];
            saturated[channel] |= value->raw_data == 4095;
            if (counts[BATTERY] < WINDOW_SAMPLES || counts[MOTOR] < WINDOW_SAMPLES) continue;

            adc_measurement_t sample = {
                .battery = convert_window(BATTERY, sums[BATTERY], counts[BATTERY], saturated[BATTERY],
                                           &filter_q8[BATTERY], &filter_ready[BATTERY]),
                .motor = convert_window(MOTOR, sums[MOTOR], counts[MOTOR], saturated[MOTOR],
                                         &filter_q8[MOTOR], &filter_ready[MOTOR]),
                .timestamp_us = esp_timer_get_time(),
                .overflows = accepted_overflows,
                .error = ESP_OK,
            };
            portENTER_CRITICAL(&sample_lock);
            if (overflows == accepted_overflows) {
                sample.sequence = latest.sequence + 1;
                latest = sample;
            }
            portEXIT_CRITICAL(&sample_lock);
            memset(sums, 0, sizeof(sums));
            memset(counts, 0, sizeof(counts));
            memset(saturated, 0, sizeof(saturated));
        }
    }
}

esp_err_t adc_measure_init(void)
{
    const int pins[] = {PIN_ADC_BAT, PIN_ADC_MT};
    adc_digi_pattern_config_t patterns[CHANNEL_COUNT] = {0};
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        adc_unit_t unit;
        esp_err_t error = adc_continuous_io_to_channel(pins[i], &unit, &channels[i]);
        if (error != ESP_OK) return error;
        if (unit != ADC_UNIT_1) return ESP_ERR_INVALID_ARG;
        patterns[i] = (adc_digi_pattern_config_t) {
            .atten = ADC_ATTEN_DB_12,
            .channel = channels[i],
            .unit = unit,
            .bit_width = 12,
        };
    }
    const adc_continuous_handle_cfg_t handle_config = {
        .max_store_buf_size = 2048,
        .conv_frame_size = FRAME_BYTES,
    };
    esp_err_t error = adc_continuous_new_handle(&handle_config, &adc);
    if (error != ESP_OK) return error;
    bool started = false;
    const adc_continuous_config_t config = {
        .pattern_num = CHANNEL_COUNT,
        .adc_pattern = patterns,
        .sample_freq_hz = SAMPLE_FREQ_HZ, /* ESP32-S3: 611 total, ~305.5 Hz/channel. */
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    };
    error = adc_continuous_config(adc, &config);
    if (error != ESP_OK) goto fail;
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        const adc_cali_curve_fitting_config_t cal_config = {
            .unit_id = ADC_UNIT_1,
            .chan = channels[i],
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        error = adc_cali_create_scheme_curve_fitting(&cal_config, &calibration[i]);
        if (error == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "GPIO%d calibration unavailable; raw samples only", pins[i]);
        } else if (error != ESP_OK) {
            goto fail;
        }
    }
    const adc_continuous_evt_cbs_t callbacks = {.on_pool_ovf = pool_overflow};
    error = adc_continuous_register_event_callbacks(adc, &callbacks, NULL);
    if (error != ESP_OK) goto fail;
    error = adc_continuous_start(adc);
    if (error != ESP_OK) goto fail;
    started = true;
    if (xTaskCreate(sampling_task, "adc_measure", 4096, NULL, 4, NULL) != pdPASS) {
        error = ESP_ERR_NO_MEM;
        goto fail;
    }
    initialized = true;
    ESP_LOGI(TAG, "ADC1 DMA: GPIO%d/CH%d, GPIO%d/CH%d; total=%u Hz, %u samples/channel/window",
             pins[BATTERY], channels[BATTERY], pins[MOTOR], channels[MOTOR],
             (unsigned)SAMPLE_FREQ_HZ, (unsigned)WINDOW_SAMPLES);
    return ESP_OK;

fail:
    if (started) (void)adc_continuous_stop(adc);
    (void)adc_continuous_deinit(adc);
    adc = NULL;
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        if (calibration[i]) {
            (void)adc_cali_delete_scheme_curve_fitting(calibration[i]);
            calibration[i] = NULL;
        }
    }
    return error;
}

esp_err_t adc_measure_get(adc_measurement_t *sample)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&sample_lock);
    *sample = latest;
    const uint32_t count = overflows;
    portEXIT_CRITICAL(&sample_lock);
    if (sample->overflows != count || !sample->sequence ||
        esp_timer_get_time() - sample->timestamp_us > STALE_US) {
        sample->battery.valid = false;
        sample->motor.valid = false;
        sample->error = sample->overflows != count ? ESP_ERR_INVALID_STATE : ESP_ERR_TIMEOUT;
    }
    sample->overflows = count;
    return ESP_OK;
}
