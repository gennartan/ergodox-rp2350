#include <errno.h>
#include <string.h>

#include "keyboard_config.h"

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t value)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

/* IEEE CRC-32 over header and payload with bytes 60..63 treated as zero.
 * Used only during load/save, never in the key-event path. */
static uint32_t blob_crc(const uint8_t *blob, size_t length)
{
    uint32_t crc = UINT32_MAX;

    for (size_t i = 0; i < length; ++i) {
        crc ^= (i >= 60 && i < 64) ? 0 : blob[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static bool valid_action(const struct keyboard_action *a, uint8_t layers, const struct keyboard_macro *macros)
{
    struct keyboard_action canonical = { .type = a->type };

    switch (a->type) {
    case KEYBOARD_ACTION_TRANSPARENT:
    case KEYBOARD_ACTION_NONE:
        break;
    case KEYBOARD_ACTION_KEY:
        if (a->keycode > 101) { return false; }
        canonical.keycode = a->keycode;
        canonical.modifier = a->modifier;
        break;
    case KEYBOARD_ACTION_MACRO:
        if (a->usage >= KEYBOARD_MACRO_COUNT || macros[a->usage].count == 0) { return false; }
        canonical.usage = a->usage;
        break;
    case KEYBOARD_ACTION_CONSUMER:
        if (a->usage > 0x3ff) { return false; }
        canonical.usage = a->usage;
        break;
    case KEYBOARD_ACTION_SYSTEM:
        if (a->usage != 0 && (a->usage < HID_SYSTEM_POWER_DOWN ||
            a->usage > HID_SYSTEM_WAKE_UP)) { return false; }
        canonical.usage = a->usage;
        break;
    case KEYBOARD_ACTION_MOUSE_BUTTON:
        if (a->usage & ~0x1fU) { return false; }
        canonical.usage = a->usage;
        break;
    case KEYBOARD_ACTION_MOUSE_MOVE:
        if (a->x == INT8_MIN || a->y == INT8_MIN) { return false; }
        canonical.x = a->x;
        canonical.y = a->y;
        break;
    case KEYBOARD_ACTION_MOUSE_WHEEL:
        if (a->y == INT8_MIN) { return false; }
        canonical.y = a->y;
        break;
    case KEYBOARD_ACTION_LIGHTING:
        if (a->usage > KEYBOARD_LIGHTING_BRIGHTNESS_DOWN) { return false; }
        canonical.usage = a->usage;
        break;
    case KEYBOARD_ACTION_LAYER_MOMENTARY:
    case KEYBOARD_ACTION_LAYER_TOGGLE:
    case KEYBOARD_ACTION_LAYER_DEFAULT:
        if (a->layer >= layers) { return false; }
        canonical.layer = a->layer;
        break;
    default:
        return false;
    }
    return memcmp(a, &canonical, sizeof(*a)) == 0;
}

static bool valid_display_layout(const uint8_t *name, size_t capacity, bool allow_empty)
{
    for (size_t i = 0; i < capacity; ++i) {
        uint8_t c = name[i];
        if (c == 0) { return i != 0 || allow_empty; }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            (i != 0 && (c == '-' || c == '_')))) { return false; }
    }
    return false;
}

int keyboard_config_validate(const struct keyboard_config *config)
{
    if (config == NULL || config->layer_count == 0 ||
        config->layer_count > KEYBOARD_MAX_LAYERS ||
        config->default_layer >= config->layer_count ||
        memchr(config->name, '\0', sizeof(config->name)) == NULL ||
        !valid_display_layout((const uint8_t *)config->display_layout, sizeof(config->display_layout), true)) {
        return -EINVAL;
    }
    for (int i = 0; i < KEYBOARD_MACRO_COUNT; ++i) {
        const struct keyboard_macro *macro = &config->macros[i];
        if (macro->count > KEYBOARD_MACRO_MAX_STEPS) { return -EINVAL; }
        for (int j = 0; j < KEYBOARD_MACRO_MAX_STEPS; ++j) {
            const struct keyboard_macro_step *step = &macro->steps[j];
            if (step->keycode > 101 || step->delay_ms > KEYBOARD_MACRO_MAX_DELAY_MS ||
                (j >= macro->count && (step->modifier || step->keycode || step->delay_ms))) {
                return -EINVAL;
            }
        }
    }
    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; ++hand) {
        for (int layer = 0; layer < config->layer_count; ++layer) {
            for (int key = 0; key < KEYBOARD_KEY_COUNT; ++key) {
                if (!valid_action(&config->actions[hand][layer][key],
                    config->layer_count, config->macros)) {
                    return -EINVAL;
                }
            }
        }
    }
    return 0;
}

static struct keyboard_action decode_action(const uint8_t *p)
{
    struct keyboard_action a = { .type = p[0], .modifier = p[1] };

    /* Explicit wire byte order; do not serialize the native union. */
    if (a.type == KEYBOARD_ACTION_MOUSE_MOVE || a.type == KEYBOARD_ACTION_MOUSE_WHEEL) {
        a.x = p[2] < 128 ? p[2] : (int)p[2] - 256;
        a.y = p[3] < 128 ? p[3] : (int)p[3] - 256;
    } else if (a.type == KEYBOARD_ACTION_KEY ||
        (a.type >= KEYBOARD_ACTION_LAYER_MOMENTARY && a.type <= KEYBOARD_ACTION_LAYER_DEFAULT)) {
        a.keycode = p[2];
        a.layer = p[3];
    } else {
        a.usage = get16(p + 2);
    }
    return a;
}

int keyboard_config_encode(const struct keyboard_config *config,
    uint8_t *blob, size_t capacity, size_t *length)
{
    size_t offset = KEYBOARD_CONFIG_HEADER_SIZE;
    size_t size;
    int ret = keyboard_config_validate(config);

    if (ret != 0 || blob == NULL || length == NULL) {
        return -EINVAL;
    }
    size = KEYBOARD_CONFIG_HEADER_SIZE +
        KEYBOARD_HAND_COUNT * config->layer_count * KEYBOARD_KEY_COUNT * 4;
    size += KEYBOARD_CONFIG_MACRO_BANK_SIZE + KEYBOARD_CONFIG_DISPLAY_LAYOUT_SIZE;
    if (capacity < size) { return -ENOSPC; }
    memset(blob, 0, size);
    memcpy(blob, "EKCF", 4);
    blob[4] = KEYBOARD_CONFIG_VERSION;
    blob[6] = KEYBOARD_CONFIG_HEADER_SIZE;
    put32(blob + 8, size);
    blob[12] = config->layer_count;
    blob[13] = config->default_layer;
    blob[14] = KEYBOARD_HAND_COUNT;
    blob[15] = KEYBOARD_KEY_COUNT;
    memcpy(blob + 16, config->name, strlen(config->name));
    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; ++hand) {
        for (int layer = 0; layer < config->layer_count; ++layer) {
            for (int key = 0; key < KEYBOARD_KEY_COUNT; ++key) {
                const struct keyboard_action *a = &config->actions[hand][layer][key];
                blob[offset] = a->type;
                blob[offset + 1] = a->modifier;
                if (a->type == KEYBOARD_ACTION_MOUSE_MOVE || a->type == KEYBOARD_ACTION_MOUSE_WHEEL) {
                    blob[offset + 2] = (uint8_t)a->x;
                    blob[offset + 3] = (uint8_t)a->y;
                } else if (a->type == KEYBOARD_ACTION_KEY ||
                    (a->type >= KEYBOARD_ACTION_LAYER_MOMENTARY && a->type <= KEYBOARD_ACTION_LAYER_DEFAULT)) {
                    blob[offset + 2] = a->keycode;
                    blob[offset + 3] = a->layer;
                } else {
                    blob[offset + 2] = a->usage & 0xff;
                    blob[offset + 3] = a->usage >> 8;
                }
                offset += 4;
            }
        }
    }
    for (int i = 0; i < KEYBOARD_MACRO_COUNT; ++i) {
        const struct keyboard_macro *macro = &config->macros[i];
        blob[offset] = macro->count;
        offset += 4;
        for (int j = 0; j < KEYBOARD_MACRO_MAX_STEPS; ++j) {
            const struct keyboard_macro_step *step = &macro->steps[j];
            blob[offset] = step->modifier;
            blob[offset + 1] = step->keycode;
            blob[offset + 2] = step->delay_ms & 0xff;
            blob[offset + 3] = step->delay_ms >> 8;
            offset += 4;
        }
    }
    const char *locale = config->display_layout[0] ? config->display_layout : KEYBOARD_CONFIG_DEFAULT_DISPLAY_LAYOUT;
    memcpy(blob + offset, locale, strlen(locale));
    put32(blob + 60, blob_crc(blob, size));
    *length = size;
    return 0;
}

int keyboard_config_decode(const uint8_t *blob, size_t length,
    struct keyboard_config *config)
{
    size_t expected, offset = KEYBOARD_CONFIG_HEADER_SIZE;
    uint8_t layers;

    if (blob == NULL || config == NULL || length < KEYBOARD_CONFIG_HEADER_SIZE ||
        length > KEYBOARD_CONFIG_MAX_BLOB_SIZE) { return -EINVAL; }
    uint16_t version = get16(blob + 4);
    if (memcmp(blob, "EKCF", 4) != 0 || (version != 1 && version != 2 && version != 3 && version != 4 && version != KEYBOARD_CONFIG_VERSION) ||
        get16(blob + 6) != KEYBOARD_CONFIG_HEADER_SIZE) { return -ENOTSUP; }
    layers = blob[12];
    expected = KEYBOARD_CONFIG_HEADER_SIZE + KEYBOARD_HAND_COUNT * layers * KEYBOARD_KEY_COUNT * 4;
    if (version >= 3) { expected += KEYBOARD_CONFIG_MACRO_BANK_SIZE; }
    if (version >= 5) { expected += KEYBOARD_CONFIG_DISPLAY_LAYOUT_SIZE; }
    if (layers == 0 || layers > KEYBOARD_MAX_LAYERS || blob[13] >= layers ||
        blob[14] != KEYBOARD_HAND_COUNT || blob[15] != KEYBOARD_KEY_COUNT ||
        length != expected || get32(blob + 8) != length ||
        memchr(blob + 16, '\0', KEYBOARD_CONFIG_NAME_SIZE) == NULL) { return -EINVAL; }
    if (version == 1 || version >= 4) {
        for (int i = 48; i < 60; ++i) {
            if (blob[i] != 0) { return -EINVAL; }
        }
    }
    const uint8_t *locale = (const uint8_t *)KEYBOARD_CONFIG_DEFAULT_DISPLAY_LAYOUT;
    if (version >= 5) {
        locale = blob + length - KEYBOARD_CONFIG_DISPLAY_LAYOUT_SIZE;
        if (!valid_display_layout(locale, KEYBOARD_CONFIG_DISPLAY_LAYOUT_SIZE, false)) { return -EINVAL; }
    } else if (version == 2 || version == 3) {
        locale = blob + 48;
        if (!valid_display_layout(locale, 12, false)) { return -EINVAL; }
    }
    if (get32(blob + 60) != blob_crc(blob, length)) { return -EBADMSG; }
    /* Decode/validate the small macro bank before touching the destination. */
    struct keyboard_macro macros[KEYBOARD_MACRO_COUNT] = {0};
    size_t actions_end = KEYBOARD_CONFIG_HEADER_SIZE + KEYBOARD_HAND_COUNT * layers * KEYBOARD_KEY_COUNT * 4;
    if (version >= 3) {
        size_t macro_offset = actions_end;
        for (int i = 0; i < KEYBOARD_MACRO_COUNT; ++i) {
            struct keyboard_macro *macro = &macros[i];
            macro->count = blob[macro_offset];
            if (macro->count > KEYBOARD_MACRO_MAX_STEPS || blob[macro_offset + 1] ||
                blob[macro_offset + 2] || blob[macro_offset + 3]) { return -EINVAL; }
            macro_offset += 4;
            for (int j = 0; j < KEYBOARD_MACRO_MAX_STEPS; ++j) {
                struct keyboard_macro_step *step = &macro->steps[j];
                step->modifier = blob[macro_offset];
                step->keycode = blob[macro_offset + 1];
                step->delay_ms = get16(blob + macro_offset + 2);
                if (step->keycode > 101 || step->delay_ms > KEYBOARD_MACRO_MAX_DELAY_MS ||
                    (j >= macro->count && (step->modifier || step->keycode || step->delay_ms))) { return -EINVAL; }
                macro_offset += 4;
            }
        }
    }
    /* Validate the entire candidate before modifying any destination data. */
    for (size_t i = KEYBOARD_CONFIG_HEADER_SIZE; i < actions_end; i += 4) {
        struct keyboard_action a = decode_action(blob + i);
        if (!valid_action(&a, layers, macros)) { return -EINVAL; }
    }
    memset(config, 0, sizeof(*config));
    memcpy(config->name, blob + 16, sizeof(config->name));
    memcpy(config->macros, macros, sizeof(macros));
    config->layer_count = layers;
    config->default_layer = blob[13];
    strcpy(config->display_layout, (const char *)locale);
    for (int hand = 0; hand < KEYBOARD_HAND_COUNT; ++hand) {
        for (int layer = 0; layer < layers; ++layer) {
            for (int key = 0; key < KEYBOARD_KEY_COUNT; ++key) {
                config->actions[hand][layer][key] = decode_action(blob + offset);
                offset += 4;
            }
        }
    }
    return 0;
}
