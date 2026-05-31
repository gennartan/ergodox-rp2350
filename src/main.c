#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/logging/log.h>

#include "keyboard_layout.h"
#include "split_transport.h"

LOG_MODULE_REGISTER(kbd);

#define LEDS 4
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

static const struct gpio_dt_spec leds[LEDS] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

#define KEY_DEBOUNCE_MS 5

// Individual key GPIOs for the electronic design
static const struct gpio_dt_spec keys[KEYBOARD_KEY_COUNT] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(key1), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key2), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key3), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key4), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key5), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key6), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key7), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key8), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key9), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key10), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key11), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key12), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key13), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key14), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key15), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key16), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key17), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key18), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key19), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key20), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key21), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key22), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key23), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key24), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key25), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key26), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key27), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key28), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key29), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key30), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key31), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key32), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key33), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key34), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key35), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key36), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key37), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(key38), gpios),
};

const uint8_t hid_report_desc[] = HID_KEYBOARD_REPORT_DESC();

static const struct device *hid_dev;
static bool hid_ready;
static uint32_t hid_duration;

USBD_DEVICE_DEFINE(ergodox_usbd,
    DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
    0x2fe3, 0x0007);

USBD_DESC_LANG_DEFINE(ergodox_lang);
USBD_DESC_MANUFACTURER_DEFINE(ergodox_mfr, "Zephyr");
USBD_DESC_PRODUCT_DEFINE(ergodox_product, "Ergodox Custom Keyboard");
USBD_DESC_CONFIG_DEFINE(ergodox_fs_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(ergodox_fs_config, 0, 50, &ergodox_fs_desc);

struct key_irq {
    struct gpio_callback callback;
    struct k_work_delayable work;
    uint8_t index;
};

static struct key_irq key_irqs[KEYBOARD_KEY_COUNT];
static struct keyboard_runtime keyboard;
static uint8_t local_report[KEYBOARD_HID_REPORT_SIZE];
static uint8_t remote_report[KEYBOARD_HID_REPORT_SIZE];
static struct k_spinlock report_lock;
static struct k_work remote_report_work;

enum keyboard_transport_mode {
    KEYBOARD_TRANSPORT_USB,
    KEYBOARD_TRANSPORT_UART,
};

static enum keyboard_transport_mode transport_mode = KEYBOARD_TRANSPORT_UART;

#if IS_ENABLED(CONFIG_ERGODOX_KEYBOARD_HAND_LEFT)
#define ERGODOX_KEYBOARD_HAND KEYBOARD_HAND_LEFT
#define ERGODOX_KEYBOARD_HAND_NAME "left"
#else
#define ERGODOX_KEYBOARD_HAND KEYBOARD_HAND_RIGHT
#define ERGODOX_KEYBOARD_HAND_NAME "right"
#endif

// HID report buffer
UDC_STATIC_BUF_DEFINE(hid_report, KEYBOARD_HID_REPORT_SIZE);


static void kb_iface_ready(const struct device *dev, const bool ready)
{
	LOG_INF("HID device %s interface is %s",
		dev->name, ready ? "ready" : "not ready");
	hid_ready = ready;
}

static int kb_get_report(const struct device *dev,
			 const uint8_t type, const uint8_t id, const uint16_t len,
			 uint8_t *const buf)
{
	LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);

	return 0;
}

static int kb_set_report(const struct device *dev,
			 const uint8_t type, const uint8_t id, const uint16_t len,
			 const uint8_t *const buf)
{
    LOG_WRN("Set Report not implemented, Type %u ID %u", type, id);
	return 0;
}

/* Idle duration is stored but not used to calculate idle reports. */
static void kb_set_idle(const struct device *dev,
			const uint8_t id, const uint32_t duration)
{
	LOG_INF("Set Idle %u to %u", id, duration);
	hid_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id)
{
	LOG_INF("Get Idle %u to %u", id, hid_duration);
	return hid_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto)
{
	LOG_INF("Protocol changed to %s",
		proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len,
			     const uint8_t *const buf)
{
	LOG_HEXDUMP_DBG(buf, len, "o.r.");
	kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

struct hid_device_ops kb_ops = {
	.iface_ready = kb_iface_ready,
	.get_report = kb_get_report,
	.set_report = kb_set_report,
	.set_idle = kb_set_idle,
	.get_idle = kb_get_idle,
	.set_protocol = kb_set_protocol,
	.output_report = kb_output_report,
};

static void usbd_msg_cb(struct usbd_context *const usbd_ctx,
    const struct usbd_msg *const msg)
{
    LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

    if (msg->type == USBD_MSG_CONFIGURATION) {
        LOG_INF("Configuration value %d", msg->status);
    }

    if (usbd_can_detect_vbus(usbd_ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY) {
            if (usbd_enable(usbd_ctx)) {
                LOG_ERR("Failed to enable USB device support");
            }
        }

        if (msg->type == USBD_MSG_VBUS_REMOVED) {
            hid_ready = false;
            if (usbd_disable(usbd_ctx)) {
                LOG_ERR("Failed to disable USB device support");
            }
        }
    }
}

static int init_usb_device(void)
{
    int ret;

    hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
    if (!device_is_ready(hid_dev)) {
        LOG_ERR("HID device is not ready");
        return -ENODEV;
    }

    ret = hid_device_register(hid_dev, hid_report_desc,
        sizeof(hid_report_desc), &kb_ops);
    if (ret != 0) {
        LOG_ERR("Failed to register HID device: %d", ret);
        return ret;
    }

    ret = usbd_add_descriptor(&ergodox_usbd, &ergodox_lang);
    if (ret != 0) {
        LOG_ERR("Failed to add language descriptor: %d", ret);
        return ret;
    }

    ret = usbd_add_descriptor(&ergodox_usbd, &ergodox_mfr);
    if (ret != 0) {
        LOG_ERR("Failed to add manufacturer descriptor: %d", ret);
        return ret;
    }

    ret = usbd_add_descriptor(&ergodox_usbd, &ergodox_product);
    if (ret != 0) {
        LOG_ERR("Failed to add product descriptor: %d", ret);
        return ret;
    }

    ret = usbd_add_configuration(&ergodox_usbd, USBD_SPEED_FS,
        &ergodox_fs_config);
    if (ret != 0) {
        LOG_ERR("Failed to add full-speed configuration: %d", ret);
        return ret;
    }

    ret = usbd_register_all_classes(&ergodox_usbd, USBD_SPEED_FS, 1, NULL);
    if (ret != 0) {
        LOG_ERR("Failed to register USB classes: %d", ret);
        return ret;
    }

    ret = usbd_device_set_code_triple(&ergodox_usbd, USBD_SPEED_FS, 0, 0, 0);
    if (ret != 0) {
        LOG_ERR("Failed to set USB code triple: %d", ret);
        return ret;
    }

    ret = usbd_msg_register_cb(&ergodox_usbd, usbd_msg_cb);
    if (ret != 0) {
        LOG_ERR("Failed to register USB message callback: %d", ret);
        return ret;
    }

    ret = usbd_init(&ergodox_usbd);
    if (ret != 0) {
        LOG_ERR("Failed to initialize USB device support: %d", ret);
        return ret;
    }

    if (!usbd_can_detect_vbus(&ergodox_usbd)) {
        ret = usbd_enable(&ergodox_usbd);
        if (ret != 0) {
            LOG_ERR("Failed to enable USB device support: %d", ret);
            return ret;
        }
    }

    return 0;
}

static void update_leds_for_layers(void)
{
    uint32_t active_layers = keyboard_runtime_active_layers(&keyboard);

    // Map layers to LEDs (BASE layer has no LED indicator)
    // Layer 0 (BASE) = all LEDs off
    // Layer 1 (NAV) = LED0 on
    // Layer 2 (SYMBOL) = LED1 on
    // Layer 3 (FN) = LED2 on
    for (int i = 0; i < LEDS; i++) {
        // Turn on LED if the corresponding layer (i+1) is active
        int led_state = (active_layers & BIT(i + 1)) ? 1 : 0;
        gpio_pin_set_dt(&leds[i], led_state);
        LOG_INF("LED %d %s for layer %d", i, led_state ? "ON" : "OFF", i + 1);
    }
}

static void send_hid_report(void)
{
    int ret;

    if (!hid_ready) {
        LOG_DBG("USB HID interface is not ready");
        return;
    }

    ret = hid_device_submit_report(hid_dev, KEYBOARD_HID_REPORT_SIZE,
        hid_report);
    if (ret != 0) {
        LOG_ERR("Failed to submit HID report: %d", ret);
    }
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

static void merge_report_keys(uint8_t merged[KEYBOARD_HID_REPORT_SIZE],
    uint8_t *report_index, const uint8_t report[KEYBOARD_HID_REPORT_SIZE])
{
    for (uint8_t i = 2; i < KEYBOARD_HID_REPORT_SIZE; i++) {
        append_report_key(merged, report_index, report[i]);
    }
}

static void build_merged_hid_report(void)
{
    uint8_t report_index = 2;

    memset(hid_report, 0, KEYBOARD_HID_REPORT_SIZE);
    hid_report[0] = local_report[0] | remote_report[0];

    merge_report_keys(hid_report, &report_index, local_report);
    merge_report_keys(hid_report, &report_index, remote_report);
}

static void submit_merged_hid_report(void)
{
    k_spinlock_key_t key = k_spin_lock(&report_lock);

    build_merged_hid_report();

    k_spin_unlock(&report_lock, key);
    send_hid_report();
}

static void remote_report_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    submit_merged_hid_report();
}

static void split_report_received(const uint8_t *report, uint8_t len,
    void *user_data)
{
    ARG_UNUSED(user_data);

    if (len != KEYBOARD_HID_REPORT_SIZE) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&report_lock);

    memcpy(remote_report, report, KEYBOARD_HID_REPORT_SIZE);

    k_spin_unlock(&report_lock, key);
    k_work_submit(&remote_report_work);
}

static bool wait_for_usb_ready(void)
{
    int64_t deadline = k_uptime_get() + CONFIG_ERGODOX_USB_READY_TIMEOUT_MS;

    while (k_uptime_get() < deadline) {
        if (hid_ready) {
            return true;
        }

        k_msleep(10);
    }

    return hid_ready;
}

static void update_local_report(void)
{
    k_spinlock_key_t key = k_spin_lock(&report_lock);

    keyboard_runtime_build_hid_report(&keyboard, local_report);

    k_spin_unlock(&report_lock, key);
}

static int publish_local_report(void)
{
    if (transport_mode == KEYBOARD_TRANSPORT_USB) {
        submit_merged_hid_report();
        return 0;
    }

#if IS_ENABLED(CONFIG_ERGODOX_SPLIT_UART)
    return split_transport_send_report(local_report, KEYBOARD_HID_REPORT_SIZE);
#else
    return -ENOTSUP;
#endif
}

static void update_key(uint8_t index, bool pressed)
{
    const struct keyboard_action *action;
    int ret;

    if (!keyboard_runtime_handle_key(&keyboard, index, pressed)) {
        update_leds_for_layers();
        return;
    }

    update_local_report();

    ret = publish_local_report();
    if (ret != 0) {
        LOG_WRN("Failed to publish local report: %d", ret);
    }

    action = keyboard_runtime_pressed_action(&keyboard, index);
    LOG_INF("Key %s: index %d, keycode 0x%02x, layers 0x%08x",
        pressed ? "pressed" : "released", index + 1, action->keycode,
        keyboard_runtime_active_layers(&keyboard));
}

static void key_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct key_irq *key_irq = CONTAINER_OF(dwork, struct key_irq, work);
    bool pressed = gpio_pin_get_dt(&keys[key_irq->index]);

    update_key(key_irq->index, pressed);
}

static void key_gpio_callback(const struct device *port,
    struct gpio_callback *callback, gpio_port_pins_t pins)
{
    struct key_irq *key_irq = CONTAINER_OF(callback, struct key_irq, callback);

    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    k_work_reschedule(&key_irq->work, K_MSEC(KEY_DEBOUNCE_MS));
}

static int init_split_transport(bool receive_reports)
{
#if IS_ENABLED(CONFIG_ERGODOX_SPLIT_UART)
    return split_transport_init(receive_reports ? split_report_received : NULL,
        NULL);
#else
    ARG_UNUSED(receive_reports);
    return 0;
#endif
}

static int select_transport_mode(void)
{
    int ret = init_usb_device();

    if (ret == 0 && wait_for_usb_ready()) {
        transport_mode = KEYBOARD_TRANSPORT_USB;
        LOG_INF("Using USB HID to host and UART for split RX");
        return init_split_transport(true);
    }

    if (ret != 0) {
        LOG_WRN("USB initialization failed, falling back to UART: %d", ret);
    } else {
        LOG_WRN("USB HID not ready after %d ms, falling back to UART",
            CONFIG_ERGODOX_USB_READY_TIMEOUT_MS);
        (void)usbd_disable(&ergodox_usbd);
    }

    transport_mode = KEYBOARD_TRANSPORT_UART;
    return init_split_transport(false);
}

int main(void)
{
    int ret;

    LOG_INF("Keyboard interrupt setup start");
    k_work_init(&remote_report_work, remote_report_work_handler);

    keyboard_runtime_init(&keyboard, keyboard_layout_get_selected(),
        ERGODOX_KEYBOARD_HAND);
    LOG_INF("Keyboard layout: %s, hand: %s", keyboard.layout->name,
        ERGODOX_KEYBOARD_HAND_NAME);

    ret = select_transport_mode();
    if (ret != 0) {
        LOG_ERR("Failed to initialize keyboard transport: %d", ret);
        return ret;
    }

    for (int i = 0; i < KEYBOARD_KEY_COUNT; i++) {
        ret = gpio_pin_configure_dt(&keys[i], GPIO_INPUT | GPIO_PULL_UP);
        if (ret < 0) {
            LOG_ERR("Failed to configure key %d: %d", i + 1, ret);
            return ret;
        }

        key_irqs[i].index = i;
        k_work_init_delayable(&key_irqs[i].work, key_work_handler);
        gpio_init_callback(&key_irqs[i].callback, key_gpio_callback,
            BIT(keys[i].pin));

        ret = gpio_add_callback(keys[i].port, &key_irqs[i].callback);
        if (ret < 0) {
            LOG_ERR("Failed to add key %d callback: %d", i + 1, ret);
            return ret;
        }

        ret = gpio_pin_interrupt_configure_dt(&keys[i], GPIO_INT_EDGE_BOTH);
        if (ret < 0) {
            LOG_ERR("Failed to enable key %d interrupt: %d", i + 1, ret);
            return ret;
        }
        keyboard_runtime_handle_key(&keyboard, i, gpio_pin_get_dt(&keys[i]));
    }

    update_local_report();
    ret = publish_local_report();
    if (ret != 0) {
        LOG_WRN("Failed to publish initial report: %d", ret);
    }

    for (int i = 0; i < LEDS; i++) {
        ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure LED %d: %d", i, ret);
            return ret;
        }
    }

    // Set initial LED states based on active layers
    update_leds_for_layers();

    LOG_INF("Keyboard interrupts initialized");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
