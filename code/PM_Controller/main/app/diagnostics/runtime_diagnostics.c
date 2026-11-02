#include "runtime_diagnostics.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <limits.h>
#include <stdlib.h>

#define REPORT_INTERVAL_US (30LL * 1000000)
#define STACK_NOTICE_BYTES 512U
#define INTERNAL_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

static const char *TAG = "runtime_diag";
static struct {
    const char *name;
    TaskHandle_t handle;
    UBaseType_t reported_low;
} tasks[] = {
    {.name = "main"}, {.name = "ui"}, {.name = "taskLVGL"},
    {.name = "adc_measure"}, {.name = "sdm50_rx"},
    {.name = "hc165_scan"}, {.name = "film_reload"},
};
static int64_t next_report_us;

void runtime_diagnostics_init(void)
{
    char sha[65];
    esp_app_get_elf_sha256(sha, sizeof(sha));
    ESP_LOGI(TAG, "Running ELF SHA256: %s", sha);
    for (unsigned i = 0; i < sizeof(tasks) / sizeof(tasks[0]); ++i) {
        tasks[i].handle = xTaskGetHandle(tasks[i].name);
        tasks[i].reported_low = UINT_MAX;
    }
    ESP_LOGI(TAG, "task_handles main=%p ui=%p LVGL=%p ADC=%p SDM50=%p keys=%p film=%p",
             (void *)tasks[0].handle, (void *)tasks[1].handle, (void *)tasks[2].handle,
             (void *)tasks[3].handle, (void *)tasks[4].handle, (void *)tasks[5].handle,
             (void *)tasks[6].handle);
    next_report_us = 0;
    runtime_diagnostics_poll();
}

void runtime_diagnostics_poll(void)
{
    const int64_t now = esp_timer_get_time();
    if (now < next_report_us) return;
    next_report_us = now + REPORT_INTERVAL_US;

    /* Limit inspection to internal byte-addressable heaps used by task control
     * blocks, stacks and DMA state. A full PSRAM walk could stall the system.
     * This checks allocator metadata, not arbitrary writes within allocations. */
    if (!heap_caps_check_integrity(INTERNAL_CAPS, true)) {
        ESP_LOGE(TAG, "Internal heap metadata corrupted; stopping at first diagnostic detection");
        abort();
    }
    long minimum[sizeof(tasks) / sizeof(tasks[0])];
    for (unsigned i = 0; i < sizeof(tasks) / sizeof(tasks[0]); ++i) {
        minimum[i] = -1; /* Task absent after an optional peripheral init failure. */
        if (!tasks[i].handle) continue;
        const UBaseType_t bytes = uxTaskGetStackHighWaterMark(tasks[i].handle);
        minimum[i] = (long)bytes; /* ESP-IDF returns bytes, not upstream words. */
        if (bytes < STACK_NOTICE_BYTES && bytes < tasks[i].reported_low) {
            ESP_LOGW(TAG, "Low stack reserve: task=%s minimum=%lu bytes",
                     tasks[i].name, (unsigned long)bytes);
            tasks[i].reported_low = bytes;
        }
    }
    ESP_LOGI(TAG, "stack_min_bytes main=%ld ui=%ld LVGL=%ld ADC=%ld SDM50=%ld keys=%ld film=%ld",
             minimum[0], minimum[1], minimum[2], minimum[3], minimum[4], minimum[5], minimum[6]);
    ESP_LOGI(TAG, "internal_heap_bytes free=%lu minimum=%lu largest=%lu integrity=OK",
             (unsigned long)heap_caps_get_free_size(INTERNAL_CAPS),
             (unsigned long)heap_caps_get_minimum_free_size(INTERNAL_CAPS),
             (unsigned long)heap_caps_get_largest_free_block(INTERNAL_CAPS));
}
