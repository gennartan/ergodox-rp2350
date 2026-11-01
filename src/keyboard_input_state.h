#ifndef ERGODOX_KEYBOARD_INPUT_STATE_H
#define ERGODOX_KEYBOARD_INPUT_STATE_H

#include "keyboard_layout.h"
#include "split_transport.h"

#define KEYBOARD_INPUT_BITMAP_SIZE ((KEYBOARD_HAND_COUNT * KEYBOARD_KEY_COUNT + 7) / 8)

/* ISR-safe physical state; no layout interpretation or HID processing. */
void keyboard_input_state_reset(void);
void keyboard_input_state_handle(const struct split_key_event *event);
void keyboard_input_state_snapshot(uint8_t output[KEYBOARD_INPUT_BITMAP_SIZE]);

/* Start local GPIO scanning and slave reception, exclusively for the viewer. */
int keyboard_inputs_start_configurator(void);

#endif
