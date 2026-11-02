#ifndef PM_FOCUS_POSITION_H
#define PM_FOCUS_POSITION_H
#include "focus_unit.h"

/* UI task owns this state machine; process and EEPROM writes run without the
 * LVGL lock. Call init once after settings_init and focus_unit_init. */
void focus_position_init(void);
void focus_position_process(uint32_t now_ms);
esp_err_t focus_position_set_target(int32_t raw);
esp_err_t focus_position_home(uint32_t now_ms);
bool focus_position_can_home(uint32_t now_ms);
bool focus_position_is_homing(void);
focus_unit_snapshot_t focus_position_snapshot(uint32_t now_ms);
const char *focus_position_message(void);
#endif
