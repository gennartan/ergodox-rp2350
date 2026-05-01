#ifndef KEYBOARD_LAYOUT_H
#define KEYBOARD_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/hid.h>

#define KEYBOARD_KEY_COUNT 38
#define KEYBOARD_MAX_LAYERS 8
#define KEYBOARD_HID_REPORT_SIZE 8
#define KEYBOARD_MAX_REPORT_KEYS 6

enum keyboard_layer {
    KEYBOARD_LAYER_BASE,
    KEYBOARD_LAYER_NAV,
    KEYBOARD_LAYER_SYMBOL,
    KEYBOARD_LAYER_FN,
    KEYBOARD_LAYER_COUNT,
};

enum keyboard_action_type {
    KEYBOARD_ACTION_TRANSPARENT,
    KEYBOARD_ACTION_NONE,
    KEYBOARD_ACTION_KEY,
    KEYBOARD_ACTION_LAYER_MOMENTARY,
    KEYBOARD_ACTION_LAYER_TOGGLE,
    KEYBOARD_ACTION_LAYER_DEFAULT,
};

struct keyboard_action {
    enum keyboard_action_type type;
    uint8_t keycode;
    uint8_t modifier;
    uint8_t layer;
};

struct keyboard_layout {
    const char *name;
    uint8_t layer_count;
    const struct keyboard_action (*layers)[KEYBOARD_KEY_COUNT];
};

struct keyboard_runtime {
    const struct keyboard_layout *layout;
    struct keyboard_action pressed_actions[KEYBOARD_KEY_COUNT];
    bool pressed[KEYBOARD_KEY_COUNT];
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
    uint8_t key_index, bool pressed);
void keyboard_runtime_build_hid_report(const struct keyboard_runtime *runtime,
    uint8_t report[KEYBOARD_HID_REPORT_SIZE]);
uint32_t keyboard_runtime_active_layers(const struct keyboard_runtime *runtime);
const struct keyboard_action *keyboard_runtime_pressed_action(
    const struct keyboard_runtime *runtime, uint8_t key_index);

#endif /* KEYBOARD_LAYOUT_H */
