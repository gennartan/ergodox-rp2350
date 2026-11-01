#include <string.h>

#include <zephyr/kernel.h>

#include "keyboard_input_state.h"

static struct k_spinlock state_lock;
static uint8_t pressed_keys[KEYBOARD_INPUT_BITMAP_SIZE];

void keyboard_input_state_reset(void)
{
    k_spinlock_key_t key = k_spin_lock(&state_lock);

    memset(pressed_keys, 0, sizeof(pressed_keys));
    k_spin_unlock(&state_lock, key);
}

void keyboard_input_state_handle(const struct split_key_event *event)
{
    unsigned int bit;
    k_spinlock_key_t key;

    if (event == NULL || event->hand >= KEYBOARD_HAND_COUNT ||
        event->key_index >= KEYBOARD_KEY_COUNT ||
        event->type > SPLIT_KEY_EVENT_DOUBLE_TAP) {
        return;
    }
    bit = event->hand * KEYBOARD_KEY_COUNT + event->key_index;
    key = k_spin_lock(&state_lock);
    if (event->type == SPLIT_KEY_EVENT_PRESS) {
        pressed_keys[bit / 8] |= BIT(bit % 8);
    } else {
        /* A synthetic double tap is a completed gesture, not a held key. */
        pressed_keys[bit / 8] &= ~BIT(bit % 8);
    }
    k_spin_unlock(&state_lock, key);
}

void keyboard_input_state_snapshot(uint8_t output[KEYBOARD_INPUT_BITMAP_SIZE])
{
    k_spinlock_key_t key = k_spin_lock(&state_lock);

    memcpy(output, pressed_keys, sizeof(pressed_keys));
    k_spin_unlock(&state_lock, key);
}
