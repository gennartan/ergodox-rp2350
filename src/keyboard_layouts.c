#include "keyboard_layout.h"

static const struct keyboard_action simple_layers[KEYBOARD_LAYER_COUNT][KEYBOARD_KEY_COUNT] = {
    [KEYBOARD_LAYER_BASE] = {
        [0] = KB_KEY(HID_KEY_Q),
        [1] = KB_KEY(HID_KEY_W),
        [2] = KB_KEY(HID_KEY_E),
        [3] = KB_KEY(HID_KEY_R),
        [4] = KB_KEY(HID_KEY_T),
        [5] = KB_KEY(HID_KEY_Y),
        [6] = KB_KEY(HID_KEY_A),
        [7] = KB_KEY(HID_KEY_S),
        [8] = KB_KEY(HID_KEY_D),
        [9] = KB_KEY(HID_KEY_F),
        [10] = KB_KEY(HID_KEY_G),
        [11] = KB_KEY(HID_KEY_H),
        [12] = KB_KEY(HID_KEY_Z),
        [13] = KB_KEY(HID_KEY_X),
        [14] = KB_KEY(HID_KEY_C),
        [15] = KB_KEY(HID_KEY_V),
        [16] = KB_KEY(HID_KEY_B),
        [17] = KB_KEY(HID_KEY_N),
        [18] = KB_KEY(HID_KEY_ESC),
        [19] = KB_KEY(HID_KEY_TAB),
        [20] = KB_KEY(HID_KEY_SPACE),
        [21] = KB_KEY(HID_KEY_ENTER),
        [22] = KB_KEY(HID_KEY_BACKSPACE),
        [23] = KB_KEY(HID_KEY_LEFT),
        [24] = KB_KEY(HID_KEY_1),
        [25] = KB_KEY(HID_KEY_2),
        [26] = KB_KEY(HID_KEY_3),
        [27] = KB_KEY(HID_KEY_4),
        [28] = KB_KEY(HID_KEY_5),
        [29] = KB_KEY(HID_KEY_6),
        [30] = KB_MO(KEYBOARD_LAYER_NAV),
        [31] = KB_MO(KEYBOARD_LAYER_SYMBOL),
        [32] = KB_TG(KEYBOARD_LAYER_FN),
    },
    [KEYBOARD_LAYER_NAV] = {
        [0] = KB_KEY(HID_KEY_HOME),
        [1] = KB_KEY(HID_KEY_UP),
        [2] = KB_KEY(HID_KEY_END),
        [6] = KB_KEY(HID_KEY_LEFT),
        [7] = KB_KEY(HID_KEY_DOWN),
        [8] = KB_KEY(HID_KEY_RIGHT),
        [18] = KB_KEY(HID_KEY_ESC),
        [20] = KB_KEY(HID_KEY_SPACE),
        [21] = KB_KEY(HID_KEY_ENTER),
        [22] = KB_KEY(HID_KEY_BACKSPACE),
    },
    [KEYBOARD_LAYER_SYMBOL] = {
        [0] = KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_1),
        [1] = KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_2),
        [2] = KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_3),
        [3] = KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_4),
        [4] = KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_5),
        [5] = KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_6),
        [6] = KB_KEY(HID_KEY_LEFTBRACE),
        [7] = KB_KEY(HID_KEY_RIGHTBRACE),
        [8] = KB_KEY(HID_KEY_BACKSLASH),
        [9] = KB_KEY(HID_KEY_SEMICOLON),
        [10] = KB_KEY(HID_KEY_APOSTROPHE),
        [11] = KB_KEY(HID_KEY_GRAVE),
    },
    [KEYBOARD_LAYER_FN] = {
        [0] = KB_KEY(HID_KEY_F1),
        [1] = KB_KEY(HID_KEY_F2),
        [2] = KB_KEY(HID_KEY_F3),
        [3] = KB_KEY(HID_KEY_F4),
        [4] = KB_KEY(HID_KEY_F5),
        [5] = KB_KEY(HID_KEY_F6),
        [6] = KB_KEY(HID_KEY_F7),
        [7] = KB_KEY(HID_KEY_F8),
        [8] = KB_KEY(HID_KEY_F9),
        [9] = KB_KEY(HID_KEY_F10),
        [10] = KB_KEY(HID_KEY_F11),
        [11] = KB_KEY(HID_KEY_F12),
        [32] = KB_TG(KEYBOARD_LAYER_FN),
    },
};

#pragma GCC diagnostic ignored "-Wunused-const-variable="
static const struct keyboard_layout simple_layout = {
    .name = "simple",
    .layer_count = KEYBOARD_LAYER_COUNT,
    .layers = simple_layers,
};
#pragma GCC diagnostic pop

const struct keyboard_layout *keyboard_layout_get_selected(void)
{
#if defined(CONFIG_ERGODOX_KEYBOARD_LAYOUT_SIMPLE)
    return &simple_layout;
#elif defined(CONFIG_ERGODOX_KEYBOARD_LAYOUT_CUSTOM)
#include "custom_layout.c"
    return &custom_layout;
#else
    return &simple_layout;
#endif
}
