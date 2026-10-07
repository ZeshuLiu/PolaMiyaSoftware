#include "hc165_keys.h"

void hc165_keys_init(hc165_keys_state_t *state, uint8_t raw, uint32_t now_ms)
{
    for (unsigned int i = 0; i < HC165_KEY_COUNT; ++i) {
        hc165_key_state_t *key = &state->keys[i];
        const bool pressed = (raw & (1U << i)) == 0;
        key->candidate_pressed = pressed;
        key->pressed = pressed;
        /* A key held during startup must be released before producing events. */
        key->armed = !pressed;
        key->long_reported = false;
        key->candidate_since_ms = now_ms;
        key->pressed_since_ms = now_ms;
    }
}

hc165_key_events_t hc165_keys_update(hc165_keys_state_t *state, uint8_t raw, uint32_t now_ms)
{
    hc165_key_events_t events = {0};

    for (unsigned int i = 0; i < HC165_KEY_COUNT; ++i) {
        hc165_key_state_t *key = &state->keys[i];
        const uint8_t mask = (uint8_t)(1U << i);
        const bool sampled_pressed = (raw & mask) == 0;

        if (sampled_pressed != key->candidate_pressed) {
            key->candidate_pressed = sampled_pressed;
            key->candidate_since_ms = now_ms;
        }

        if (key->candidate_pressed != key->pressed &&
            (uint32_t)(now_ms - key->candidate_since_ms) >= HC165_DEBOUNCE_MS) {
            key->pressed = key->candidate_pressed;
            if (key->pressed) {
                /* Count from the first sample of the now-confirmed edge. */
                key->pressed_since_ms = key->candidate_since_ms;
                key->long_reported = false;
            } else {
                const uint32_t held_ms = key->candidate_since_ms - key->pressed_since_ms;
                if (key->armed && !key->long_reported) {
                    if (held_ms >= HC165_LONG_PRESS_MS) {
                        events.long_press |= mask;
                    } else {
                        events.short_press |= mask;
                    }
                }
                key->armed = true;
                key->long_reported = false;
            }
        }

        /* Don't turn a release just before 1 s into a long press while
         * waiting for release debounce. Long presses fire only once. */
        if (key->pressed && key->candidate_pressed && key->armed && !key->long_reported &&
            (uint32_t)(now_ms - key->pressed_since_ms) >= HC165_LONG_PRESS_MS) {
            events.long_press |= mask;
            key->long_reported = true;
        }
    }
    return events;
}

uint8_t hc165_keys_pressed(const hc165_keys_state_t *state)
{
    uint8_t pressed = 0;
    for (unsigned int i = 0; i < HC165_KEY_COUNT; ++i) {
        if (state->keys[i].pressed) {
            pressed |= (uint8_t)(1U << i);
        }
    }
    return pressed;
}
