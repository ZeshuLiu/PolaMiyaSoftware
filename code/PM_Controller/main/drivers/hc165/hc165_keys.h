#ifndef PM_HC165_KEYS_H
#define PM_HC165_KEYS_H

#include "hc165.h"

/* Internal key recognition logic, independent of GPIO and the scheduler. */
#define HC165_KEY_COUNT 6
#define HC165_DEBOUNCE_MS 20U
#define HC165_LONG_PRESS_MS 1000U

typedef struct {
    bool candidate_pressed;
    bool pressed;
    bool armed;
    bool long_reported;
    uint32_t candidate_since_ms;
    uint32_t pressed_since_ms;
} hc165_key_state_t;

typedef struct {
    hc165_key_state_t keys[HC165_KEY_COUNT];
} hc165_keys_state_t;

void hc165_keys_init(hc165_keys_state_t *state, uint8_t raw, uint32_t now_ms);
hc165_key_events_t hc165_keys_update(hc165_keys_state_t *state, uint8_t raw, uint32_t now_ms);
uint8_t hc165_keys_pressed(const hc165_keys_state_t *state);

#endif
