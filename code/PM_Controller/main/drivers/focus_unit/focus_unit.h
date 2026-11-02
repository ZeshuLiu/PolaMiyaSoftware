#ifndef PM_FOCUS_UNIT_H
#define PM_FOCUS_UNIT_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool command_rejected;
    bool valid;              /* A successful status response is still fresh. */
    uint8_t work_state, mode, direction;
    uint16_t duty;
    uint8_t initialized, zero_valid, seek_zero, home_attempts;
    int32_t home_error_counts, position, target_raw;
    uint8_t move_state, glide_completed;
    uint8_t command_result;  /* Most recent control result; 0 when cleared. */
    int32_t desired_raw;      /* Host target, independent of device target_raw. */
    uint32_t status_sequence; /* Increments only for a new successful status. */
    uint32_t control_sequence; /* Increments on control response or final timeout. */
    bool initialization_required; /* Application position-basis gate. */
    bool storage_error;
} focus_unit_snapshot_t;

typedef enum {
    FOCUS_UNIT_STOP = 0x10,
    FOCUS_UNIT_HOME = 0x11,   /* Glide B, seek_zero=1. */
    FOCUS_UNIT_MOVE = 0x13,
    FOCUS_UNIT_SET_CURRENT = 0x14,
} focus_unit_command_t;

/* Init at startup; thereafter only the UI task calls these nonblocking APIs. */
esp_err_t focus_unit_init(void);
/* Raw transport only. Application must invalidate persisted position before
 * HOME/MOVE. No new control is queued while another request is in flight. */
esp_err_t focus_unit_request(focus_unit_command_t command, int32_t raw, uint32_t now_ms);
/* Active focus polls at 100 ms; idle focus at 1 s. UI task only. */
void focus_unit_set_active(bool active);
void focus_unit_process(uint32_t now_ms);
focus_unit_snapshot_t focus_unit_snapshot(uint32_t now_ms);
#endif
