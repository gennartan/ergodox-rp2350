#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "keyboard_config.h"

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>
#include "keyboard_input_state.h"
#include <zephyr/kvss/nvs.h>
#include <zephyr/storage/flash_map.h>
#endif

static struct keyboard_config source, decoded;
static uint8_t blob[KEYBOARD_CONFIG_MAX_BLOB_SIZE];

static void setup(void)
{
    memset(&source, 0, sizeof(source));
    strcpy(source.name, "test");
    source.layer_count = 8;
    source.default_layer = 0;
    source.actions[0][0][0] = (struct keyboard_action)KB_KEY(HID_KEY_A);
    source.actions[0][0][1] = (struct keyboard_action)KB_MO(7);
    source.actions[1][7][0] = (struct keyboard_action)KB_MOD(HID_KBD_MODIFIER_LEFT_SHIFT, HID_KEY_B);
    source.actions[1][0][2] = (struct keyboard_action)KB_CONSUMER(HID_CONSUMER_VOLUME_INCREMENT);
    source.actions[1][0][3] = (struct keyboard_action)KB_MOUSE_MOVE(-16, 16);
    source.actions[1][0][4] = (struct keyboard_action)KB_MOUSE_WHEEL(-1);
    source.actions[1][0][5] = (struct keyboard_action)KB_SYSTEM(HID_SYSTEM_SLEEP);
    source.actions[1][0][6] = (struct keyboard_action)KB_MOUSE_BUTTON(HID_MOUSE_BUTTON_LEFT);
    source.actions[1][0][7] = (struct keyboard_action)KB_LIGHTING(KEYBOARD_LIGHTING_TOGGLE);
    source.actions[1][0][8] = (struct keyboard_action)KB_TG(6);
    source.actions[1][0][9] = (struct keyboard_action)KB_DF(5);
    source.actions[1][0][10] = (struct keyboard_action)KB_NONE;
}

static void test_codec(void)
{
    size_t length;
    setup();
    assert(sizeof(struct keyboard_action) == 4);
    assert(keyboard_config_encode(&source, blob, sizeof(blob), &length) == 0);
    assert(length == 2496);
    assert(memcmp(blob, "EKCF\1\0\100\0", 8) == 0);
    assert(blob[64] == KEYBOARD_ACTION_KEY && blob[66] == HID_KEY_A);
    assert(keyboard_config_decode(blob, length, &decoded) == 0);
    assert(memcmp(&source, &decoded, sizeof(source)) == 0);
    for (size_t i = 0; i < length; ++i) {
        blob[i] ^= 0x80;
        assert(keyboard_config_decode(blob, length, &decoded) < 0);
        assert(memcmp(&source, &decoded, sizeof(source)) == 0);
        blob[i] ^= 0x80;
    }
    for (size_t i = 0; i < length; ++i) {
        assert(keyboard_config_decode(blob, i, &decoded) < 0);
    }
    assert(keyboard_config_decode(blob, length + 1, &decoded) < 0);
    assert(keyboard_config_encode(&source, blob, length - 1, &length) == -ENOSPC);
    source.layer_count = 1;
    source.default_layer = 1;
    assert(keyboard_config_validate(&source) == -EINVAL);
    source.default_layer = 0;
    assert(keyboard_config_validate(&source) == -EINVAL); /* layer target 7 */
    source.actions[0][0][1] = (struct keyboard_action)KB_NONE;
    assert(keyboard_config_encode(&source, blob, sizeof(blob), &length) == -EINVAL); /* other targets */
    memset(source.actions, 0, sizeof(source.actions));
    assert(keyboard_config_encode(&source, blob, sizeof(blob), &length) == 0);
    assert(length == 368);
    assert(keyboard_config_decode(blob, length, &decoded) == 0);
    assert(decoded.layer_count == 1);
    source.actions[0][0][0].type = 255;
    assert(keyboard_config_validate(&source) == -EINVAL);
    source.actions[0][0][0] = (struct keyboard_action)KB_MOUSE_MOVE(-128, 0);
    assert(keyboard_config_validate(&source) == -EINVAL);
    setup();
    assert(keyboard_config_encode(&source, blob, sizeof(blob), &length) == 0);
    FILE *out = fopen("/tmp/ergodox-config-test.bin", "wb");
    assert(out != NULL);
    assert(fwrite(blob, 1, length, out) == length);
    fclose(out);
}

static void test_runtime(void)
{
    struct keyboard_runtime runtime;
    uint8_t report[8];
    setup();
    struct keyboard_layout layout = {
        .name = source.name, .layer_count = source.layer_count,
        .default_layer = source.default_layer, .layers = &source.actions,
    };
    keyboard_runtime_init(&runtime, &layout);
    keyboard_runtime_handle_key(&runtime, 0, 1, true); /* master layer key */
    assert(keyboard_runtime_handle_key(&runtime, 1, 0, true)); /* raw slave event */
    keyboard_runtime_build_hid_report(&runtime, report);
    assert(report[0] == HID_KBD_MODIFIER_LEFT_SHIFT && report[2] == HID_KEY_B);
    keyboard_runtime_handle_key(&runtime, 0, 1, false);
    assert(keyboard_runtime_handle_key(&runtime, 1, 0, false)); /* original action released */
    keyboard_runtime_build_hid_report(&runtime, report);
    assert(report[0] == 0 && report[2] == 0);
    assert(keyboard_runtime_handle_key(&runtime, 0, 0, true));
    assert(!keyboard_runtime_handle_key(&runtime, 0, 0, true));
    keyboard_runtime_build_hid_report(&runtime, report);
    assert(report[2] == HID_KEY_A);
    assert(!keyboard_runtime_handle_key(&runtime, 2, 0, true));
    assert(!keyboard_runtime_handle_key(&runtime, 0, 38, true));
    keyboard_runtime_handle_key(&runtime, 1, 2, true);
    assert(keyboard_runtime_consumer_usage(&runtime) == HID_CONSUMER_VOLUME_INCREMENT);
    keyboard_runtime_handle_key(&runtime, 1, 5, true);
    assert(keyboard_runtime_system_usage(&runtime) == HID_SYSTEM_SLEEP);
    keyboard_runtime_handle_key(&runtime, 1, 6, true);
    assert(keyboard_runtime_mouse_buttons(&runtime) == HID_MOUSE_BUTTON_LEFT);
    layout.default_layer = 7;
    keyboard_runtime_init(&runtime, &layout);
    assert(keyboard_runtime_handle_key(&runtime, 1, 0, true));
    keyboard_runtime_build_hid_report(&runtime, report);
    assert(report[2] == HID_KEY_B);
}

#ifdef CONFIG_ZTEST
ZTEST(config, test_codec_roundtrip) { test_codec(); }
ZTEST(config, test_master_runtime) { test_runtime(); }

ZTEST(config, test_nvm_save_reload_and_fallback)
{
    struct nvs_fs fs = {0};
    const struct flash_area *area;
    size_t length;

    assert(flash_area_open(PARTITION_ID(keyboard_config_partition), &area) == 0);
    assert(flash_area_erase(area, 0, area->fa_size) == 0);
    assert(keyboard_config_init(true) == 0);
    assert(strcmp(keyboard_config_layout()->name, "simple") == 0);
    setup();
    assert(keyboard_config_save(&source) == 0);
    assert(strcmp(keyboard_config_layout()->name, "simple") == 0); /* apply only on reboot */
    assert(keyboard_config_save(&source) == 0); /* unchanged save is successful */
    source.actions[0][0][0].type = 255;
    assert(keyboard_config_save(&source) == -EINVAL);
    source.actions[0][0][0] = (struct keyboard_action)KB_KEY(HID_KEY_A);
    /* Repeat writes exercise NVS sector rollover and garbage collection. */
    for (int i = 0; i < 20; ++i) {
        source.default_layer = i % source.layer_count;
        assert(keyboard_config_save(&source) == 0);
    }
    assert(keyboard_config_init(false) == 0);
    assert(strcmp(keyboard_config_layout()->name, "test") == 0);
    assert(keyboard_config_layout()->layer_count == 8);
    assert(keyboard_config_layout()->default_layer == source.default_layer);
    assert(memcmp(*keyboard_config_layout()->layers, source.actions, sizeof(source.actions)) == 0);
    assert(keyboard_config_save(&source) == -EACCES);
    assert(keyboard_config_export(blob, sizeof(blob), &length) == 0);
    assert(keyboard_config_decode(blob, length, &decoded) == 0);
    assert(memcmp(&decoded, &source, sizeof(source)) == 0);

    fs.flash_device = area->fa_dev;
    fs.offset = area->fa_off;
    fs.sector_size = 4096;
    fs.sector_count = 4;
    assert(nvs_mount(&fs) == 0);
    assert(keyboard_config_encode(&source, blob, sizeof(blob), &length) == 0);
    blob[64] ^= 1; /* completed NVS record, invalid application CRC */
    assert(nvs_write(&fs, 1, blob, length) == length);
    assert(keyboard_config_init(false) == 0);
    assert(strcmp(keyboard_config_layout()->name, "simple") == 0);
    flash_area_close(area);
}

ZTEST(config, test_binary_import_persists_without_live_apply)
{
    const struct flash_area *area;
    size_t length;

    assert(flash_area_open(PARTITION_ID(keyboard_config_partition), &area) == 0);
    assert(flash_area_erase(area, 0, area->fa_size) == 0);
    assert(keyboard_config_init(true) == 0);
    setup();
    assert(keyboard_config_encode(&source, blob, sizeof(blob), &length) == 0);
    assert(keyboard_config_import(blob, length) == 0);
    assert(strcmp(keyboard_config_layout()->name, "simple") == 0);
    blob[64] ^= 1;
    assert(keyboard_config_import(blob, length) == -EBADMSG);
    assert(keyboard_config_import(blob, length - 1) == -EINVAL);
    blob[64] ^= 1;
    assert(keyboard_config_import(blob, length) == 0); /* idempotent retry */
    assert(keyboard_config_init(false) == 0);
    assert(strcmp(keyboard_config_layout()->name, "test") == 0);
    assert(memcmp(*keyboard_config_layout()->layers, source.actions, sizeof(source.actions)) == 0);
    assert(keyboard_config_import(blob, length) == -EACCES);
    flash_area_close(area);
}

ZTEST(config, test_physical_key_bitmap)
{
    uint8_t snapshot[KEYBOARD_INPUT_BITMAP_SIZE];
    uint8_t retained[KEYBOARD_INPUT_BITMAP_SIZE];
    struct split_key_event event = { .hand = 0, .key_index = 37, .type = SPLIT_KEY_EVENT_PRESS };

    assert(sizeof(snapshot) == 10);
    keyboard_input_state_reset();
    keyboard_input_state_handle(&event);
    event.hand = 1; event.key_index = 0;
    keyboard_input_state_handle(&event);
    event.key_index = 37;
    keyboard_input_state_handle(&event);
    keyboard_input_state_snapshot(snapshot);
    assert(snapshot[4] == 0x60); /* left key 37 and right key 0 cross a byte */
    assert(snapshot[9] == 0x08); /* final key, high four bits remain zero */
    memcpy(retained, snapshot, sizeof(retained));
    event.type = SPLIT_KEY_EVENT_RELEASE;
    keyboard_input_state_handle(&event);
    keyboard_input_state_snapshot(snapshot);
    assert(snapshot[9] == 0 && retained[9] == 8);
    event.hand = 2; event.key_index = 0; event.type = SPLIT_KEY_EVENT_PRESS;
    keyboard_input_state_handle(&event);
    event.hand = 0; event.key_index = 38;
    keyboard_input_state_handle(&event);
    event.key_index = 0; event.type = 255;
    keyboard_input_state_handle(&event);
    keyboard_input_state_handle(NULL);
    keyboard_input_state_snapshot(snapshot);
    assert(snapshot[0] == 0 && snapshot[4] == 0x60);
    event.hand = 1; event.key_index = 0; event.type = SPLIT_KEY_EVENT_DOUBLE_TAP;
    keyboard_input_state_handle(&event);
    keyboard_input_state_snapshot(snapshot);
    assert(snapshot[4] == 0x20);
    keyboard_input_state_reset();
    keyboard_input_state_snapshot(snapshot);
    for (unsigned int i = 0; i < sizeof(snapshot); ++i) assert(snapshot[i] == 0);
}

ZTEST_SUITE(config, NULL, NULL, NULL, NULL, NULL);
#else
int main(void)
{
    test_codec();
    test_runtime();
    puts("Configuration codec, corrupt/truncated inputs, and master layer/HID tests passed");
    return 0;
}
#endif
