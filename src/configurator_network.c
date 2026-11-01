#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/net/net_if.h>
#include <zephyr/usb/usbd.h>

#include "configurator_network.h"
#include "keyboard_config.h"
#include "keyboard_input_state.h"

LOG_MODULE_REGISTER(configurator_network);

#define CONFIGURATOR_ADDRESS "192.168.7.1"

/* A separate identity avoids the host caching the normal keyboard interfaces. */
USBD_DEVICE_DEFINE(configurator_usbd,
    DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 0x2fe3, 0x0008);
USBD_DESC_LANG_DEFINE(configurator_lang);
USBD_DESC_MANUFACTURER_DEFINE(configurator_mfr, "Zephyr");
USBD_DESC_PRODUCT_DEFINE(configurator_product, "Ergodox Configurator");
USBD_DESC_CONFIG_DEFINE(configurator_fs_desc, "Configurator Network");
USBD_CONFIGURATION_DEFINE(configurator_fs_config, 0, 50, &configurator_fs_desc);

static const uint8_t index_html[] = {
#include "configurator_index.html.inc"
};

static struct http_resource_detail_static index_resource = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_STATIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET),
        .content_type = "text/html; charset=utf-8",
        .content_encoding = "gzip",
    },
    .static_data = index_html,
    .static_data_len = sizeof(index_html),
};

static uint8_t layout_blob[KEYBOARD_CONFIG_MAX_BLOB_SIZE];

/* HTTP polls an eventfd, a listener, and one descriptor per accepted client. */
BUILD_ASSERT(CONFIG_ZVFS_POLL_MAX >= CONFIG_HTTP_SERVER_MAX_CLIENTS + 2,
    "Socket poll capacity must cover the complete HTTP descriptor set");
BUILD_ASSERT(CONFIG_NET_MAX_CONTEXTS >= CONFIG_HTTP_SERVER_MAX_CLIENTS + 2,
    "Network contexts must cover HTTP clients, listener, and DHCP");
BUILD_ASSERT(CONFIG_NET_MAX_CONN >= CONFIG_HTTP_SERVER_MAX_CLIENTS + 2,
    "Network connection capacity must cover HTTP and DHCP");

static struct http_resource_detail_static layout_resource = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_STATIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET),
        .content_type = "application/octet-stream",
    },
    .static_data = layout_blob,
};

static int keys_handler(struct http_client_ctx *client,
    enum http_transaction_status status,
    const struct http_request_ctx *request,
    struct http_response_ctx *response, void *user_data)
{
    /* The dynamic resource holder serializes transactions until completion. */
    static uint8_t snapshot[KEYBOARD_INPUT_BITMAP_SIZE];
    static const struct http_header headers[] = {
        { .name = "Cache-Control", .value = "no-store" },
    };

    ARG_UNUSED(client);
    ARG_UNUSED(request);
    ARG_UNUSED(user_data);
    if (status == HTTP_SERVER_REQUEST_DATA_FINAL) {
        keyboard_input_state_snapshot(snapshot);
        response->headers = headers;
        response->header_count = ARRAY_SIZE(headers);
        response->body = snapshot;
        response->body_len = sizeof(snapshot);
        response->final_chunk = true;
    }
    return 0;
}

static struct http_resource_detail_dynamic keys_resource = {
    .common = {
        .type = HTTP_RESOURCE_TYPE_DYNAMIC,
        .bitmask_of_supported_http_methods = BIT(HTTP_GET),
        .content_type = "application/octet-stream",
    },
    .cb = keys_handler,
};

static uint16_t http_port = 80;
HTTP_SERVICE_DEFINE(configurator_http, CONFIGURATOR_ADDRESS, &http_port,
    CONFIG_HTTP_SERVER_MAX_CLIENTS, CONFIG_HTTP_SERVER_MAX_CLIENTS, NULL, NULL, NULL);
HTTP_RESOURCE_DEFINE(configurator_index, configurator_http, "/", &index_resource);
HTTP_RESOURCE_DEFINE(configurator_layout, configurator_http, "/api/layout", &layout_resource);
HTTP_RESOURCE_DEFINE(configurator_keys, configurator_http, "/api/keys", &keys_resource);

static void configurator_usb_message(struct usbd_context *const ctx,
    const struct usbd_msg *const msg)
{
    int ret = 0;

    if (!usbd_can_detect_vbus(ctx)) {
        return;
    }
    if (msg->type == USBD_MSG_VBUS_READY) {
        ret = usbd_enable(ctx);
    } else if (msg->type == USBD_MSG_VBUS_REMOVED) {
        ret = usbd_disable(ctx);
    }
    if (ret != 0) {
        LOG_ERR("Configurator USB state change failed: %d", ret);
    }
}

static int configure_ipv4(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(configurator_eth));
    struct net_if *iface;
    struct net_in_addr address, netmask, pool;
    int ret;

    if (!device_is_ready(dev)) {
        return -ENODEV;
    }
    iface = net_if_lookup_by_dev(dev);
    if (iface == NULL) {
        return -ENODEV;
    }
    net_addr_pton(NET_AF_INET, CONFIGURATOR_ADDRESS, &address);
    net_addr_pton(NET_AF_INET, "255.255.255.0", &netmask);
    net_addr_pton(NET_AF_INET, "192.168.7.2", &pool);
    if (net_if_ipv4_addr_add(iface, &address, NET_ADDR_MANUAL, 0) == NULL) {
        return -ENOMEM;
    }
    if (!net_if_ipv4_set_netmask_by_addr(iface, &address, &netmask)) {
        return -EINVAL;
    }
    ret = net_if_up(iface);
    if (ret < 0 && ret != -EALREADY) {
        return ret;
    }
    return net_dhcpv4_server_start(iface, &pool);
}

int configurator_network_start(void)
{
    int ret;

    /* Configuration is immutable until reboot; all clients share this snapshot. */
    ret = keyboard_config_export(layout_blob, sizeof(layout_blob),
        &layout_resource.static_data_len);
    if (ret != 0) {
        LOG_ERR("Failed to export current layout: %d", ret);
        return ret;
    }
    ret = configure_ipv4();
    if (ret != 0) {
        LOG_ERR("Configurator IPv4/DHCP initialization failed: %d", ret);
        return ret;
    }
    ret = http_server_start();
    if (ret != 0) {
        LOG_ERR("Configurator HTTP startup failed: %d", ret);
        return ret;
    }
    ret = usbd_add_descriptor(&configurator_usbd, &configurator_lang);
    if (ret != 0) {
        return ret;
    }
    ret = usbd_add_descriptor(&configurator_usbd, &configurator_mfr);
    if (ret != 0) {
        return ret;
    }
    ret = usbd_add_descriptor(&configurator_usbd, &configurator_product);
    if (ret != 0) {
        return ret;
    }
    ret = usbd_add_configuration(&configurator_usbd, USBD_SPEED_FS,
        &configurator_fs_config);
    if (ret != 0) {
        return ret;
    }
    /* Register only networking: no HID or serial interfaces in this mode. */
    ret = usbd_register_class(&configurator_usbd, "cdc_ncm_0", USBD_SPEED_FS, 1);
    if (ret != 0) {
        return ret;
    }
    ret = usbd_device_set_code_triple(&configurator_usbd, USBD_SPEED_FS,
        USB_BCC_MISCELLANEOUS, 0x02, 0x01);
    if (ret != 0) {
        return ret;
    }
    ret = usbd_msg_register_cb(&configurator_usbd, configurator_usb_message);
    if (ret != 0) {
        return ret;
    }
    ret = usbd_init(&configurator_usbd);
    if (ret != 0) {
        return ret;
    }
    if (!usbd_can_detect_vbus(&configurator_usbd)) {
        ret = usbd_enable(&configurator_usbd);
        if (ret != 0) {
            return ret;
        }
    }
    LOG_INF("Configurator ready at http://" CONFIGURATOR_ADDRESS);
    return 0;
}
