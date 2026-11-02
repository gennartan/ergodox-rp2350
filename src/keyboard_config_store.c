#include <errno.h>
#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "keyboard_config.h"

LOG_MODULE_REGISTER(keyboard_config);

#define CONFIG_RECORD_ID 1

static struct keyboard_config active;
static struct keyboard_layout active_layout;
static struct nvs_fs storage;
static uint8_t blob[KEYBOARD_CONFIG_MAX_BLOB_SIZE];
static bool writes_allowed;
K_MUTEX_DEFINE(config_store_lock);
K_MUTEX_DEFINE(config_import_lock);

static int mount_storage(void)
{
    const struct flash_area *area;
    struct flash_pages_info page;
    int ret;

    ret = flash_area_open(PARTITION_ID(keyboard_config_partition), &area);
    if (ret != 0) { return ret; }
    storage.flash_device = area->fa_dev;
    storage.offset = area->fa_off;
    if (!device_is_ready(storage.flash_device)) {
        flash_area_close(area);
        return -ENODEV;
    }
    ret = flash_get_page_info_by_offs(storage.flash_device, storage.offset, &page);
    if (ret == 0) {
        if (page.size != 4096 || area->fa_size != 4 * page.size ||
            page.start_offset != storage.offset) {
            ret = -EINVAL;
        } else {
            storage.sector_size = page.size;
            storage.sector_count = 4;
        }
    }
    flash_area_close(area);
    if (ret != 0) { return ret; }
    return nvs_mount(&storage);
}

int keyboard_config_init(bool configuration_mode)
{
    const struct keyboard_layout *factory = keyboard_layout_get_selected();
    int ret;
    ssize_t length;

    writes_allowed = configuration_mode;
    memset(&active, 0, sizeof(active));
    strncpy(active.name, factory->name, sizeof(active.name) - 1);
    active.layer_count = factory->layer_count;
    active.default_layer = factory->default_layer;
    strcpy(active.display_layout, KEYBOARD_CONFIG_DEFAULT_DISPLAY_LAYOUT);
    memcpy(active.actions, *factory->layers, sizeof(active.actions));
    if (factory->macros != NULL) { memcpy(active.macros, factory->macros, sizeof(active.macros)); }
    ret = keyboard_config_validate(&active);
    if (ret != 0) {
        LOG_ERR("Invalid compiled factory configuration");
        return ret;
    }

    ret = mount_storage();
    if (ret == 0) {
        length = nvs_read(&storage, CONFIG_RECORD_ID, blob, sizeof(blob));
        ret = length < 0 ? (int)length : keyboard_config_decode(blob, length, &active);
    }
    if (ret != 0) {
        LOG_WRN("Using factory configuration (NVM unavailable or invalid: %d)", ret);
    } else {
        LOG_INF("Loaded configuration from NVM");
    }
    active_layout = (struct keyboard_layout) {
        .name = active.name,
        .layer_count = active.layer_count,
        .default_layer = active.default_layer,
        .layers = &active.actions,
        .macros = active.macros,
    };
    LOG_INF("Configuration: %s, %u layers, %u-byte actions", active.name,
        active.layer_count, (unsigned int)sizeof(struct keyboard_action));
    return 0;
}

const struct keyboard_layout *keyboard_config_layout(void)
{
    return &active_layout;
}

int keyboard_config_export(uint8_t *output, size_t capacity, size_t *length)
{
    return keyboard_config_encode(&active, output, capacity, length);
}

int keyboard_config_save(const struct keyboard_config *config)
{
    size_t length;
    ssize_t written;
    int ret;

    /* Flash programming is never allowed while the keyboard is processing HID. */
    if (!writes_allowed) { return -EACCES; }
    if (!storage.ready) { return -ENODEV; }
    k_mutex_lock(&config_store_lock, K_FOREVER);
    ret = keyboard_config_encode(config, blob, sizeof(blob), &length);
    if (ret == 0) {
        written = nvs_write(&storage, CONFIG_RECORD_ID, blob, length);
        ret = written < 0 ? (int)written :
            (written == 0 || written == length ? 0 : -EIO);
    }
    k_mutex_unlock(&config_store_lock);
    return ret;
}

int keyboard_config_import(const uint8_t *input, size_t length)
{
    /* Keep the candidate off small stacks and serialize all import callers. */
    static struct keyboard_config candidate;
    int ret;

    if (!writes_allowed) { return -EACCES; }
    k_mutex_lock(&config_import_lock, K_FOREVER);
    ret = keyboard_config_decode(input, length, &candidate);
    if (ret == 0) { ret = keyboard_config_save(&candidate); }
    k_mutex_unlock(&config_import_lock);
    return ret;
}
