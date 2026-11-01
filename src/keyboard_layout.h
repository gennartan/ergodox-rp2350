#ifndef KEYBOARD_LAYOUT_H
#define KEYBOARD_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util_macro.h>

#include "keyboard_hid.h"

#define KEYBOARD_KEY_COUNT 38
#define KEYBOARD_MAX_LAYERS 8
#define KEYBOARD_HID_REPORT_SIZE 8
#define KEYBOARD_MAX_REPORT_KEYS 6
#define KEYBOARD_MOUSE_MOVE_STEP 16

enum keyboard_hand {
    KEYBOARD_HAND_LEFT,
    KEYBOARD_HAND_RIGHT,
    KEYBOARD_HAND_COUNT,
};

enum keyboard_layer {
    KEYBOARD_LAYER_BASE,
    KEYBOARD_LAYER_NAV,
    KEYBOARD_LAYER_SYMBOL,
    KEYBOARD_LAYER_FN,
    KEYBOARD_LAYER_COUNT,
};

enum keyboard_action_type {
    KEYBOARD_ACTION_TRANSPARENT = 0,
    KEYBOARD_ACTION_NONE = 1,
    KEYBOARD_ACTION_KEY = 2,
    KEYBOARD_ACTION_CONSUMER = 3,
    KEYBOARD_ACTION_SYSTEM = 4,
    KEYBOARD_ACTION_MOUSE_BUTTON = 5,
    KEYBOARD_ACTION_MOUSE_MOVE = 6,
    KEYBOARD_ACTION_MOUSE_WHEEL = 7,
    KEYBOARD_ACTION_LIGHTING = 8,
    KEYBOARD_ACTION_LAYER_MOMENTARY = 9,
    KEYBOARD_ACTION_LAYER_TOGGLE = 10,
    KEYBOARD_ACTION_LAYER_DEFAULT = 11,
};

/* Four bytes, naturally aligned: tag, modifiers, and a two-byte payload. */
struct keyboard_action {
    uint8_t type;
    uint8_t modifier;
    union {
        uint16_t usage;
        struct { uint8_t keycode; uint8_t layer; };
        struct { int8_t x; int8_t y; };
    };
};

_Static_assert(sizeof(struct keyboard_action) == 4, "Actions must occupy four bytes");

enum keyboard_lighting_action {
    KEYBOARD_LIGHTING_TOGGLE,
    KEYBOARD_LIGHTING_ON,
    KEYBOARD_LIGHTING_OFF,
    KEYBOARD_LIGHTING_BRIGHTNESS_UP,
    KEYBOARD_LIGHTING_BRIGHTNESS_DOWN,
};

struct keyboard_layout {
    const char *name;
    uint8_t layer_count;
    uint8_t default_layer;
    const struct keyboard_action (*layers)[KEYBOARD_HAND_COUNT][KEYBOARD_MAX_LAYERS][KEYBOARD_KEY_COUNT];
};

struct keyboard_runtime {
    const struct keyboard_layout *layout;
    struct keyboard_action pressed_actions[KEYBOARD_HAND_COUNT][KEYBOARD_KEY_COUNT];
    bool pressed[KEYBOARD_HAND_COUNT][KEYBOARD_KEY_COUNT];
    uint32_t default_layer;
    uint32_t active_layers;
};

#define KB_TRNS \
    { .type = KEYBOARD_ACTION_TRANSPARENT }
#define KB_NONE \
    { .type = KEYBOARD_ACTION_NONE }
#define KB_KEY(key) \
    { .type = KEYBOARD_ACTION_KEY, .keycode = (key), .modifier = HID_KBD_MODIFIER_NONE }
#define KB_MOD(mod, key) \
    { .type = KEYBOARD_ACTION_KEY, .keycode = (key), .modifier = (mod) }
#define KB_CONSUMER(consumer_usage) \
    { .type = KEYBOARD_ACTION_CONSUMER, .usage = (consumer_usage) }
#define KB_SYSTEM(system_usage) \
    { .type = KEYBOARD_ACTION_SYSTEM, .usage = (system_usage) }
#define KB_MOUSE_BUTTON(button_mask) \
    { .type = KEYBOARD_ACTION_MOUSE_BUTTON, .usage = (button_mask) }
#define KB_MOUSE_MOVE(delta_x, delta_y) \
    { .type = KEYBOARD_ACTION_MOUSE_MOVE, .x = (delta_x), .y = (delta_y) }
#define KB_MOUSE_WHEEL(delta) \
    { .type = KEYBOARD_ACTION_MOUSE_WHEEL, .y = (delta) }
#define KB_LIGHTING(command) \
    { .type = KEYBOARD_ACTION_LIGHTING, .usage = (command) }
#define KB_MO(target_layer) \
    { .type = KEYBOARD_ACTION_LAYER_MOMENTARY, .layer = (target_layer) }
#define KB_TG(target_layer) \
    { .type = KEYBOARD_ACTION_LAYER_TOGGLE, .layer = (target_layer) }
#define KB_DF(target_layer) \
    { .type = KEYBOARD_ACTION_LAYER_DEFAULT, .layer = (target_layer) }

const struct keyboard_layout *keyboard_layout_get_selected(void);

void keyboard_runtime_init(struct keyboard_runtime *runtime,
    const struct keyboard_layout *layout);
bool keyboard_runtime_handle_key(struct keyboard_runtime *runtime,
    uint8_t hand, uint8_t key_index, bool pressed);
void keyboard_runtime_build_hid_report(const struct keyboard_runtime *runtime,
    uint8_t report[KEYBOARD_HID_REPORT_SIZE]);
uint16_t keyboard_runtime_consumer_usage(const struct keyboard_runtime *runtime);
uint8_t keyboard_runtime_system_usage(const struct keyboard_runtime *runtime);
uint8_t keyboard_runtime_mouse_buttons(const struct keyboard_runtime *runtime);
uint32_t keyboard_runtime_active_layers(const struct keyboard_runtime *runtime);
const struct keyboard_action *keyboard_runtime_pressed_action(
    const struct keyboard_runtime *runtime, uint8_t hand, uint8_t key_index);

#endif /* KEYBOARD_LAYOUT_H */
