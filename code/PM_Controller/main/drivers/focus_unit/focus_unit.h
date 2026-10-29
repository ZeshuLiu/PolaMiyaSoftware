#ifndef PM_FOCUS_UNIT_H
#define PM_FOCUS_UNIT_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool command_rejected;   /* Visible retained MoveRaw rejection; not completion. */
    bool valid;              /* A successful status response is still fresh. */
    uint8_t work_state, mode, direction;
    uint16_t duty;
    uint8_t initialized, zero_valid, seek_zero, home_attempts;
    int32_t home_error_counts, position, target_raw;
    uint8_t move_state, glide_completed;
    uint8_t command_result;  /* Most recent control result; 0 when cleared. */
    int32_t desired_raw;      /* Host target, independent of device target_raw. */
} focus_unit_snapshot_t;

/* Init at startup; thereafter only the UI task calls these nonblocking APIs. */
esp_err_t focus_unit_init(void);
void focus_unit_set_target(int32_t target_raw);
void focus_unit_process(uint32_t now_ms);
focus_unit_snapshot_t focus_unit_snapshot(uint32_t now_ms);
#endif
