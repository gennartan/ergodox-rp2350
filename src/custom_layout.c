#include "keyboard_layout.h"

/* Generated layout: custom (right half, 38 keys). */
static const struct keyboard_action custom_layers[KEYBOARD_LAYER_COUNT][KEYBOARD_KEY_COUNT] = {
    [KEYBOARD_LAYER_BASE] = {
        [0] = KB_KEY(HID_KEY_RIGHTBRACE),
        [1] = KB_KEY(HID_KEY_PAGEUP),
        [2] = KB_KEY(HID_KEY_HOME),
        [4] = KB_KEY(HID_KEY_Y),
        [5] = KB_KEY(HID_KEY_H),
        [6] = KB_KEY(HID_KEY_N),
        [8] = KB_KEY(HID_KEY_U),
        [9] = KB_KEY(HID_KEY_H),
        [10] = KB_KEY(HID_KEY_COMMA),
        [11] = KB_KEY(HID_KEY_LEFT),
        [13] = KB_KEY(HID_KEY_I),
        [14] = KB_KEY(HID_KEY_J),
        [15] = KB_KEY(HID_KEY_SEMICOLON),
        [16] = KB_KEY(HID_KEY_DOWN),
        [18] = KB_KEY(HID_KEY_O),
        [19] = KB_KEY(HID_KEY_L),
        [21] = KB_KEY(HID_KEY_UP),
        [23] = KB_KEY(HID_KEY_P),
        [24] = KB_KEY(HID_KEY_M),
        [26] = KB_KEY(HID_KEY_RIGHT),
        [27] = KB_KEY(HID_KEY_BACKSPACE),
        [29] = KB_KEY(HID_KEY_MINUS),
        [35] = KB_TG(KEYBOARD_LAYER_FN),
        [36] = KB_MO(KEYBOARD_LAYER_SYMBOL),
        [37] = KB_KEY(HID_KEY_ENTER),
    },
    [KEYBOARD_LAYER_NAV] = {
    },
    [KEYBOARD_LAYER_SYMBOL] = {
    },
    [KEYBOARD_LAYER_FN] = {
    },
};

static const struct keyboard_layout custom_layout = {
    .name = "custom",
    .layer_count = KEYBOARD_LAYER_COUNT,
    .layers = custom_layers,
};
