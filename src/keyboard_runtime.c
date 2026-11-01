#include "keyboard_layout.h"

#include <string.h>

static const struct keyboard_action no_action = KB_NONE;

static uint32_t layer_bit(uint8_t layer)
{
    if (layer >= KEYBOARD_MAX_LAYERS) {
        return 0;
    }

    return BIT(layer);
}

static const struct keyboard_action *resolve_action(
    const struct keyboard_runtime *runtime, uint8_t hand, uint8_t key_index)
{
    uint32_t layers = runtime->default_layer | runtime->active_layers;

    if (hand >= KEYBOARD_HAND_COUNT || key_index >= KEYBOARD_KEY_COUNT ||
        runtime->layout == NULL) {
        return &no_action;
    }

    for (int layer = KEYBOARD_LAYER_COUNT - 1; layer >= 0; layer--) {
        const struct keyboard_action *action;

        if ((layers & BIT(layer)) == 0) {
            continue;
        }

        action = &(*runtime->layout->layers)[hand][layer][key_index];
        if (action->type != KEYBOARD_ACTION_TRANSPARENT) {
            return action;
        }
    }

    return &no_action;
}

static void append_report_key(uint8_t report[KEYBOARD_HID_REPORT_SIZE],
    uint8_t *report_index, uint8_t keycode)
{
    if (keycode == 0 || *report_index >= KEYBOARD_HID_REPORT_SIZE) {
        return;
    }

    for (uint8_t i = 2; i < *report_index; i++) {
        if (report[i] == keycode) {
            return;
        }
    }

    report[(*report_index)++] = keycode;
}

void keyboard_runtime_init(struct keyboard_runtime *runtime,
    const struct keyboard_layout *layout)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->layout = layout;
    runtime->default_layer = BIT(KEYBOARD_LAYER_BASE);
}

bool keyboard_runtime_handle_key(struct keyboard_runtime *runtime,
    uint8_t hand, uint8_t key_index, bool pressed)
{
    const struct keyboard_action *action;
    bool report_changed = false;

    if (hand >= KEYBOARD_HAND_COUNT || key_index >= KEYBOARD_KEY_COUNT ||
        pressed == runtime->pressed[hand][key_index]) {
        return false;
    }

    runtime->pressed[hand][key_index] = pressed;

    if (pressed) {
        action = resolve_action(runtime, hand, key_index);
        runtime->pressed_actions[hand][key_index] = *action;
    } else {
        action = &runtime->pressed_actions[hand][key_index];
    }

    switch (action->type) {
    case KEYBOARD_ACTION_KEY:
    case KEYBOARD_ACTION_CONSUMER:
    case KEYBOARD_ACTION_SYSTEM:
    case KEYBOARD_ACTION_MOUSE_BUTTON:
        report_changed = true;
        break;
    case KEYBOARD_ACTION_MOUSE_MOVE:
    case KEYBOARD_ACTION_MOUSE_WHEEL:
        report_changed = pressed;
        break;
    case KEYBOARD_ACTION_LAYER_MOMENTARY:
        if (pressed) {
            runtime->active_layers |= layer_bit(action->layer);
        } else {
            runtime->active_layers &= ~layer_bit(action->layer);
            runtime->pressed_actions[hand][key_index] = no_action;
        }
        break;
    case KEYBOARD_ACTION_LAYER_TOGGLE:
        if (pressed) {
            runtime->active_layers ^= layer_bit(action->layer);
        }
        break;
    case KEYBOARD_ACTION_LAYER_DEFAULT:
        if (pressed && layer_bit(action->layer) != 0) {
            runtime->default_layer = layer_bit(action->layer);
        }
        break;
    case KEYBOARD_ACTION_NONE:
    case KEYBOARD_ACTION_TRANSPARENT:
    case KEYBOARD_ACTION_LIGHTING:
    default:
        break;
    }

    return report_changed;
}

uint16_t keyboard_runtime_consumer_usage(const struct keyboard_runtime *runtime)
{
    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; hand++) {
        for (int i = 0; i < KEYBOARD_KEY_COUNT; i++) {
            const struct keyboard_action *action =
                &runtime->pressed_actions[hand][i];

            if (runtime->pressed[hand][i] &&
                action->type == KEYBOARD_ACTION_CONSUMER) {
                return action->usage;
            }
        }
    }

    return 0;
}

uint8_t keyboard_runtime_system_usage(const struct keyboard_runtime *runtime)
{
    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; hand++) {
        for (int i = 0; i < KEYBOARD_KEY_COUNT; i++) {
            const struct keyboard_action *action =
                &runtime->pressed_actions[hand][i];

            if (runtime->pressed[hand][i] &&
                action->type == KEYBOARD_ACTION_SYSTEM) {
                return (uint8_t)action->usage;
            }
        }
    }

    return 0;
}

uint8_t keyboard_runtime_mouse_buttons(const struct keyboard_runtime *runtime)
{
    uint8_t buttons = 0;

    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; hand++) {
        for (int i = 0; i < KEYBOARD_KEY_COUNT; i++) {
            const struct keyboard_action *action =
                &runtime->pressed_actions[hand][i];

            if (runtime->pressed[hand][i] &&
                action->type == KEYBOARD_ACTION_MOUSE_BUTTON) {
                buttons |= (uint8_t)action->usage;
            }
        }
    }

    return buttons;
}

void keyboard_runtime_build_hid_report(const struct keyboard_runtime *runtime,
    uint8_t report[KEYBOARD_HID_REPORT_SIZE])
{
    uint8_t report_index = 2;

    memset(report, 0, KEYBOARD_HID_REPORT_SIZE);

    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; hand++) {
        for (int i = 0; i < KEYBOARD_KEY_COUNT; i++) {
            const struct keyboard_action *action =
                &runtime->pressed_actions[hand][i];

            if (!runtime->pressed[hand][i] ||
                action->type != KEYBOARD_ACTION_KEY) {
                continue;
            }

            report[0] |= action->modifier;

            append_report_key(report, &report_index, action->keycode);
        }
    }
}

uint32_t keyboard_runtime_active_layers(const struct keyboard_runtime *runtime)
{
    return runtime->default_layer | runtime->active_layers;
}

const struct keyboard_action *keyboard_runtime_pressed_action(
    const struct keyboard_runtime *runtime, uint8_t hand, uint8_t key_index)
{
    if (hand >= KEYBOARD_HAND_COUNT || key_index >= KEYBOARD_KEY_COUNT) {
        return &no_action;
    }

    return &runtime->pressed_actions[hand][key_index];
}
