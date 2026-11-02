#include "focus_position.h"
#include "settings.h"
#include "esp_timer.h"

typedef enum {
    STARTUP, NEED_HOME, READY, PREPARE, SEND, WAIT_ACK,
    RESTORE_VERIFY, MOVING, STOP_SEND, STOP_VERIFY, SAVE, FAILED,
} position_phase_t;

static struct {
    position_phase_t phase;
    focus_unit_command_t command;
    bool saved_valid, basis_valid, pending, homing, storage_error;
    int32_t saved_raw, desired, sent_target, stopped_raw;
    uint32_t control_sequence, status_sequence, last_status_ms, retry_ms;
    unsigned stopped_samples;
} focus;

static bool normal(const focus_unit_snapshot_t *s)
{
    return s->valid && s->initialized && s->work_state == 1 && s->zero_valid && !s->seek_zero;
}

static bool stopped(const focus_unit_snapshot_t *s)
{
    return normal(s) && s->mode == 0 && s->duty == 0;
}

static bool reached(const focus_unit_snapshot_t *s)
{
    const int32_t target = focus.homing ? 0 : focus.sent_target;
    const int64_t error = (int64_t)s->position - target;
    return normal(s) && s->mode == 4 && s->move_state == 2 && s->duty == 0 &&
           s->target_raw == target && error >= -50 && error <= 50;
}

static void fail(void)
{
    /* Also clear a possibly committed SAVE whose final readback failed. */
    focus.storage_error = settings_save_focus(false, 0) != ESP_OK;
    focus.saved_valid = false;
    focus.phase = FAILED;
    focus.basis_valid = false;
    focus.pending = false;
    focus.homing = false;
}

static void send_control(focus_unit_command_t command, int32_t raw,
                         const focus_unit_snapshot_t *s)
{
    /* EEPROM commits may have blocked since process() captured its timestamp. */
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (focus_unit_request(command, raw, now_ms) != ESP_OK) return;
    focus.command = command;
    focus.control_sequence = s->control_sequence;
    focus.status_sequence = s->status_sequence;
    focus.last_status_ms = now_ms;
    if (command == FOCUS_UNIT_MOVE) {
        focus.sent_target = raw;
        focus.pending = false;
    }
    focus.phase = WAIT_ACK;
}

void focus_position_init(void)
{
    focus.storage_error = settings_load_focus(&focus.saved_valid, &focus.saved_raw) != ESP_OK;
    focus.phase = STARTUP;
}

esp_err_t focus_position_set_target(int32_t raw)
{
    if (raw < 0) return ESP_ERR_INVALID_ARG;
    if (!focus.basis_valid || focus.homing || focus.storage_error ||
        focus.phase == FAILED)
        return ESP_ERR_INVALID_STATE;
    if (raw != focus.desired) {
        focus.desired = raw;
        focus.pending = true; /* Only unsent targets are replaced. */
    }
    return ESP_OK;
}

bool focus_position_can_home(uint32_t now_ms)
{
    const focus_unit_snapshot_t s = focus_unit_snapshot(now_ms);
    return (focus.phase == READY || focus.phase == NEED_HOME || focus.phase == FAILED) &&
           s.valid && s.initialized && s.work_state != 2 && s.mode != 3 && !s.seek_zero;
}

bool focus_position_is_homing(void)
{
    return focus.homing;
}

esp_err_t focus_position_home(uint32_t now_ms)
{
    if (!focus_position_can_home(now_ms)) return ESP_ERR_INVALID_STATE;
    focus.homing = true;
    focus.pending = false;
    focus.basis_valid = false;
    focus.command = FOCUS_UNIT_HOME;
    focus.phase = PREPARE;
    return ESP_OK;
}

void focus_position_process(uint32_t now_ms)
{
    focus_unit_set_active(focus.pending || (focus.phase != READY &&
                          focus.phase != NEED_HOME && focus.phase != FAILED));
    focus_unit_process(now_ms);
    const focus_unit_snapshot_t s = focus_unit_snapshot(now_ms);
    const bool fresh = s.valid && s.status_sequence != focus.status_sequence;
    if (fresh) focus.last_status_ms = now_ms;

    if (focus.phase == STARTUP) {
        if (!fresh) return;
        if (!s.initialized && s.work_state != 2) {
            focus.status_sequence = s.status_sequence;
            return; /* A slower FocusUnit boot must not discard a valid record. */
        }
        if (focus.saved_valid && s.initialized && s.mode == 0 && !s.seek_zero && !s.duty &&
            ((s.work_state == 0 && !s.zero_valid) ||
             (normal(&s) && s.position == focus.saved_raw))) {
            /* This declares the saved physical position; it never moves there. */
            send_control(FOCUS_UNIT_SET_CURRENT, focus.saved_raw, &s);
        } else {
            /* Never adopt an unexpected running device as an established basis. */
            focus.storage_error = settings_save_focus(false, 0) != ESP_OK;
            focus.saved_valid = false;
            focus.phase = NEED_HOME;
        }
        return;
    }

    if (focus.phase == WAIT_ACK) {
        if (s.control_sequence == focus.control_sequence) return;
        if (s.command_result != 0) { fail(); return; }
        focus.status_sequence = s.status_sequence;
        focus.last_status_ms = now_ms;
        if (focus.command == FOCUS_UNIT_SET_CURRENT) focus.phase = RESTORE_VERIFY;
        else if (focus.command == FOCUS_UNIT_STOP) {
            focus.phase = STOP_VERIFY;
            focus.stopped_samples = 0;
        } else focus.phase = MOVING;
        return;
    }

    if (focus.phase == READY && focus.pending) {
        if (stopped(&s) && s.position == focus.saved_raw) {
            focus.command = FOCUS_UNIT_MOVE;
            focus.phase = PREPARE;
        } else {
            if (s.valid) fail();
            return;
        }
    }
    if (focus.phase == PREPARE) {
        /* The committed invalid record must exist BEFORE any motion byte is
         * sent. If this write is interrupted, motion has not started. */
        focus.storage_error = settings_save_focus(false, 0) != ESP_OK;
        if (focus.storage_error) { fail(); return; }
        focus.saved_valid = false;
        focus.phase = SEND;
    }
    if (focus.phase == SEND) {
        if (!s.valid) return;
        if (focus.command == FOCUS_UNIT_MOVE && !normal(&s)) { fail(); return; }
        send_control(focus.command, focus.desired, &s);
        return;
    }

    if (focus.phase == RESTORE_VERIFY || focus.phase == MOVING ||
        focus.phase == STOP_SEND || focus.phase == STOP_VERIFY || focus.phase == SAVE) {
        if ((uint32_t)(now_ms - focus.last_status_ms) >= 3000U) { fail(); return; }
    }
    if (focus.pending && !focus.homing && !focus.storage_error &&
        (focus.phase == MOVING || focus.phase == STOP_SEND) && normal(&s) &&
        s.mode != 3 && s.move_state != 3) {
        /* The session is already persisted invalid. A new target need not
         * wait for REACHED, another query, or an intermediate EEPROM save. */
        send_control(FOCUS_UNIT_MOVE, focus.desired, &s);
        return;
    }
    if (!fresh) return;
    focus.status_sequence = s.status_sequence;

    if (focus.phase == RESTORE_VERIFY) {
        if (!stopped(&s) || s.position != focus.saved_raw) { fail(); return; }
        focus.desired = s.position < 0 ? 0 : s.position;
        focus.basis_valid = true;
        focus.phase = READY;
    } else if (focus.phase == MOVING) {
        if (s.work_state == 2 || s.mode == 3 || s.move_state == 3 ||
            (!focus.homing && !normal(&s))) { fail(); return; }
        if (reached(&s)) {
            /* REACHED retains automatic correction in the device. Cancel it
             * before saving, so a later correction cannot use a valid record. */
            focus.phase = STOP_SEND;
            send_control(FOCUS_UNIT_STOP, 0, &s);
        }
    } else if (focus.phase == STOP_SEND) {
        if (!normal(&s) || s.move_state == 3) { fail(); return; }
        if (reached(&s)) send_control(FOCUS_UNIT_STOP, 0, &s);
        else focus.phase = MOVING;
    } else if (focus.phase == STOP_VERIFY || focus.phase == SAVE) {
        if (!stopped(&s)) { fail(); return; }
        if (focus.pending && !focus.homing && !focus.storage_error) {
            /* Continue the same invalid session instead of saving a position
             * that is about to be invalidated again for the queued target. */
            send_control(FOCUS_UNIT_MOVE, focus.desired, &s);
            return;
        }
        if (!focus.stopped_samples || focus.stopped_raw != s.position) {
            focus.stopped_raw = s.position;
            focus.stopped_samples = 1;
            focus.phase = STOP_VERIFY;
        } else {
            focus.stopped_samples = 2;
            focus.phase = SAVE;
            if (focus.storage_error && (uint32_t)(now_ms - focus.retry_ms) < 1000U) return;
            focus.retry_ms = now_ms;
            focus.storage_error = settings_save_focus(true, s.position) != ESP_OK;
            if (!focus.storage_error) {
                focus.saved_valid = true;
                focus.saved_raw = s.position;
                focus.basis_valid = true;
                if (focus.homing) focus.desired = 0;
                focus.homing = false;
                focus.phase = READY;
            }
        }
    } else if (focus.phase == READY && (!stopped(&s) || s.position != focus.saved_raw)) {
        /* A reset or externally initiated motion invalidates this session. */
        fail();
    }
}

focus_unit_snapshot_t focus_position_snapshot(uint32_t now_ms)
{
    focus_unit_snapshot_t s = focus_unit_snapshot(now_ms);
    s.desired_raw = focus.desired;
    s.initialization_required = !focus.basis_valid;
    s.storage_error = focus.storage_error;
    return s;
}

const char *focus_position_message(void)
{
    if (focus.storage_error) return "SAVE ERROR";
    if (focus.phase == FAILED) return "HOME REQUIRED";
    if (focus.phase == STARTUP || focus.phase == RESTORE_VERIFY ||
        (focus.phase == WAIT_ACK && focus.command == FOCUS_UNIT_SET_CURRENT)) return "RESTORING...";
    if (focus.homing) return "HOMING...";
    if (!focus.basis_valid) return "INIT NEEDED";
    if (focus.phase != READY) return "FOCUS BUSY";
    return "FOCUS READY";
}
