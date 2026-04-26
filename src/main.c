#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kbd);

#define KEYS 39

// Individual key GPIOs for the electronic design
static const struct gpio_dt_spec keys[KEYS] = {
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
    GPIO_DT_SPEC_GET(DT_NODELABEL(key39), gpios),
};

// Keymap - HID keycodes for each input position.
// Adjust this array to match your electronic design key assignments.
static const uint8_t keymap[KEYS] = {
    HID_KEY_Q, HID_KEY_W, HID_KEY_E, HID_KEY_R, HID_KEY_T, HID_KEY_Y,
    HID_KEY_A, HID_KEY_S, HID_KEY_D, HID_KEY_F, HID_KEY_G, HID_KEY_H,
    HID_KEY_Z, HID_KEY_X, HID_KEY_C, HID_KEY_V, HID_KEY_B, HID_KEY_N,
    HID_KEY_ESC, HID_KEY_TAB, HID_KEY_SPACE, HID_KEY_ENTER, HID_KEY_BACKSPACE, HID_KEY_LEFT,
    HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_4, HID_KEY_5, HID_KEY_6,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const uint8_t hid_report_desc[] = HID_KEYBOARD_REPORT_DESC();

static const struct device *hid_dev;

// Current key states (pressed/released)
static bool key_states[KEYS] = {0};

// Debounce timer
static struct k_timer debounce_timer;
static bool debounce_active = false;

// HID report buffer
static uint8_t hid_report[8] = {0};
static uint8_t modifier_report = 0;

static void send_hid_report(void)
{
    hid_report[0] = modifier_report;
    hid_int_ep_write(hid_dev, hid_report, sizeof(hid_report), NULL);
}

static void update_key(uint8_t index, bool pressed)
{
    uint8_t keycode = keymap[index];

    if (pressed && !key_states[index]) {
        // Key pressed
        key_states[index] = true;
        hid_report[2] = keycode;
        send_hid_report();
        LOG_INF("Key pressed: index %d, keycode 0x%02x", index + 1, keycode);

        // Immediately send release report to make it a "tap"
        hid_report[2] = 0;
        send_hid_report();
        LOG_INF("Key released: index %d", index + 1);

        // Reset state immediately (key handled as tap, not hold)
        key_states[index] = false;
    }
}

static void scan_keys(void)
{
    if (debounce_active) {
        return;
    }

    for (int i = 0; i < KEYS; i++) {
        bool pressed = gpio_pin_get_dt(&keys[i]);
        update_key(i, pressed);
    }
}

static void debounce_timer_handler(struct k_timer *timer)
{
    debounce_active = false;
}

static void key_scan_thread_callback(void)
{
    while (1) {
        scan_keys();
        k_sleep(K_MSEC(10));
    }
}

K_THREAD_DEFINE(key_scan_thread, 1024, key_scan_thread_callback, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    int ret;

    LOG_INF("Keyboard scan start");

    hid_dev = device_get_binding("HID_0");
    if (!hid_dev) {
        LOG_ERR("Cannot get HID device");
        return -1;
    }

    usb_hid_register_device(hid_dev,
        hid_report_desc,
        sizeof(hid_report_desc),
        NULL);

    usb_hid_init(hid_dev);
    usb_enable(NULL);

    for (int i = 0; i < KEYS; i++) {
        ret = gpio_pin_configure_dt(&keys[i], GPIO_INPUT | GPIO_PULL_UP);
        if (ret < 0) {
            LOG_ERR("Failed to configure key %d: %d", i + 1, ret);
            return ret;
        }
    }

    k_timer_init(&debounce_timer, debounce_timer_handler, NULL);

    LOG_INF("Keyboard scan initialized");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
