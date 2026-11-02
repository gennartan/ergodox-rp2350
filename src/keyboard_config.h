#ifndef ERGODOX_KEYBOARD_CONFIG_H
#define ERGODOX_KEYBOARD_CONFIG_H

#include <stddef.h>
#include "keyboard_layout.h"

#define KEYBOARD_CONFIG_NAME_SIZE 32
#define KEYBOARD_CONFIG_HEADER_SIZE 64
#define KEYBOARD_CONFIG_VERSION 3
#define KEYBOARD_CONFIG_MACRO_SLOT_SIZE (4 + KEYBOARD_MACRO_MAX_STEPS * 4)
#define KEYBOARD_CONFIG_MACRO_BANK_SIZE (KEYBOARD_MACRO_COUNT * KEYBOARD_CONFIG_MACRO_SLOT_SIZE)
#define KEYBOARD_CONFIG_DISPLAY_LAYOUT_SIZE 12
#define KEYBOARD_CONFIG_DEFAULT_DISPLAY_LAYOUT "qwerty"
#define KEYBOARD_CONFIG_MAX_BLOB_SIZE (KEYBOARD_CONFIG_HEADER_SIZE + \
    KEYBOARD_HAND_COUNT * KEYBOARD_MAX_LAYERS * KEYBOARD_KEY_COUNT * 4 + \
    KEYBOARD_CONFIG_MACRO_BANK_SIZE)

/* Owned RAM data; keyboard_layout is a read-only view of these arrays. */
struct keyboard_config {
    char name[KEYBOARD_CONFIG_NAME_SIZE];
    uint8_t layer_count;
    uint8_t default_layer;
    /* UI preference only; never changes HID interpretation. Empty means qwerty. */
    char display_layout[KEYBOARD_CONFIG_DISPLAY_LAYOUT_SIZE];
    struct keyboard_macro macros[KEYBOARD_MACRO_COUNT];
    struct keyboard_action actions[KEYBOARD_HAND_COUNT][KEYBOARD_MAX_LAYERS][KEYBOARD_KEY_COUNT];
};

int keyboard_config_validate(const struct keyboard_config *config);
int keyboard_config_encode(const struct keyboard_config *config,
    uint8_t *blob, size_t capacity, size_t *length);
/* Failure leaves the destination unchanged. Unknown versions are rejected. */
int keyboard_config_decode(const uint8_t *blob, size_t length,
    struct keyboard_config *config);

/* Initialize before accepting key events. Only configuration mode may save. */
int keyboard_config_init(bool configuration_mode);
const struct keyboard_layout *keyboard_config_layout(void);
/* Read-only snapshot of the active RAM configuration, in the versioned format. */
int keyboard_config_export(uint8_t *blob, size_t capacity, size_t *length);
/* Persists a candidate for next boot; never modifies the live runtime table. */
int keyboard_config_save(const struct keyboard_config *config);
/* Validate a complete binary upload and persist it for the next boot. */
int keyboard_config_import(const uint8_t *blob, size_t length);

#endif
