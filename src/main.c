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
static bool local_pressed[KEYBOARD_KEY_COUNT];
static struct k_spinlock runtime_lock;
static struct k_work split_events_work;
K_MSGQ_DEFINE(split_event_msgq, sizeof(struct split_key_event), 32, 4);

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
    k_spinlock_key_t key = k_spin_lock(&runtime_lock);
    uint32_t active_layers = keyboard_runtime_active_layers(&keyboard);

    k_spin_unlock(&runtime_lock, key);

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

static void submit_hid_report_from_runtime(void)
{
    k_spinlock_key_t key = k_spin_lock(&runtime_lock);

    keyboard_runtime_build_hid_report(&keyboard, hid_report);

    k_spin_unlock(&runtime_lock, key);
    send_hid_report();
}

static void process_key_event(const struct split_key_event *event)
{
    struct keyboard_action action;
    uint32_t active_layers;
    bool pressed;
    bool report_changed;

    if (event->hand >= KEYBOARD_HAND_COUNT ||
        event->key_index >= KEYBOARD_KEY_COUNT) {
        return;
    }

    if (event->type == SPLIT_KEY_EVENT_DOUBLE_TAP) {
        struct split_key_event press = *event;
        struct split_key_event release = *event;

        press.type = SPLIT_KEY_EVENT_PRESS;
        release.type = SPLIT_KEY_EVENT_RELEASE;
        process_key_event(&press);
        process_key_event(&release);
        process_key_event(&press);
        process_key_event(&release);
        return;
    }

    switch (event->type) {
    case SPLIT_KEY_EVENT_PRESS:
        pressed = true;
        break;
    case SPLIT_KEY_EVENT_RELEASE:
        pressed = false;
        break;
    default:
        LOG_WRN("Unsupported split key event type %u", event->type);
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&runtime_lock);

    report_changed = keyboard_runtime_handle_key(&keyboard, event->hand,
        event->key_index, pressed);
    action = *keyboard_runtime_pressed_action(&keyboard, event->hand,
        event->key_index);
    active_layers = keyboard_runtime_active_layers(&keyboard);

    k_spin_unlock(&runtime_lock, key);

    update_leds_for_layers();

    if (report_changed) {
        submit_hid_report_from_runtime();
    }

    LOG_INF("Key %s: hand %u, index %d, keycode 0x%02x, layers 0x%08x",
        pressed ? "pressed" : "released", event->hand, event->key_index + 1,
        action.keycode, active_layers);
}

static void split_events_work_handler(struct k_work *work)
{
    struct split_key_event event;

    ARG_UNUSED(work);

    while (k_msgq_get(&split_event_msgq, &event, K_NO_WAIT) == 0) {
        process_key_event(&event);
    }
}

static void split_events_received(const struct split_key_event *events,
    uint8_t count, void *user_data)
{
    ARG_UNUSED(user_data);

    for (uint8_t i = 0; i < count; i++) {
        if (k_msgq_put(&split_event_msgq, &events[i], K_NO_WAIT) != 0) {
            LOG_WRN("Dropped split key event");
        }
    }

    k_work_submit(&split_events_work);
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

static int publish_key_event(uint8_t index, bool pressed)
{
    struct split_key_event event = {
        .hand = ERGODOX_KEYBOARD_HAND,
        .key_index = index,
        .type = pressed ? SPLIT_KEY_EVENT_PRESS : SPLIT_KEY_EVENT_RELEASE,
    };

#if IS_ENABLED(CONFIG_ERGODOX_SPLIT_UART)
    return split_transport_send_event(&event);
#else
    ARG_UNUSED(event);
    return -ENOTSUP;
#endif
}

static void update_key(uint8_t index, bool pressed)
{
    struct split_key_event event = {
        .hand = ERGODOX_KEYBOARD_HAND,
        .key_index = index,
        .type = pressed ? SPLIT_KEY_EVENT_PRESS : SPLIT_KEY_EVENT_RELEASE,
    };
    int ret;

    if (index >= KEYBOARD_KEY_COUNT || pressed == local_pressed[index]) {
        return;
    }

    local_pressed[index] = pressed;

    if (transport_mode == KEYBOARD_TRANSPORT_USB) {
        process_key_event(&event);
        return;
    }

    ret = publish_key_event(index, pressed);
    if (ret != 0) {
        LOG_WRN("Failed to publish local key event: %d", ret);
    }
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

static int init_split_transport(bool receive_events)
{
#if IS_ENABLED(CONFIG_ERGODOX_SPLIT_UART)
    return split_transport_init(receive_events ? split_events_received : NULL,
        NULL);
#else
    ARG_UNUSED(receive_events);
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
    k_work_init(&split_events_work, split_events_work_handler);

    keyboard_runtime_init(&keyboard, keyboard_layout_get_selected());
    LOG_INF("Keyboard layout: %s, hand: %s", keyboard.layout->name,
        ERGODOX_KEYBOARD_HAND_NAME);

    ret = select_transport_mode();
    if (ret != 0) {
        LOG_ERR("Failed to initialize keyboard transport: %d", ret);
        return ret;
    }

    for (int i = 0; i < LEDS; i++) {
        ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure LED %d: %d", i, ret);
            return ret;
        }
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
        if (gpio_pin_get_dt(&keys[i])) {
            update_key(i, true);
        }
    }

    if (transport_mode == KEYBOARD_TRANSPORT_USB) {
        submit_hid_report_from_runtime();
    }

    // Set initial LED states based on active layers
    update_leds_for_layers();

    LOG_INF("Keyboard interrupts initialized");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
