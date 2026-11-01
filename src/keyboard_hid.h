#ifndef KEYBOARD_HID_H
#define KEYBOARD_HID_H

#include <zephyr/usb/class/hid.h>

/*
 * Zephyr 4.4's keyboard report descriptor accepts usage IDs through 0x65,
 * but its hid_kbd_code enum stops at 0x62. Keep the standard usages needed
 * by this keyboard local to the application.
 */
enum ergodox_hid_keycode {
    ERGODOX_HID_KEY_KP_DOT = 0x63,
    ERGODOX_HID_KEY_NON_US_BACKSLASH = 0x64,
};

/* HID Consumer Page (0x0c) usages used by multimedia keys. */
enum ergodox_hid_consumer_usage {
    HID_CONSUMER_BRIGHTNESS_UP = 0x006f,
    HID_CONSUMER_BRIGHTNESS_DOWN = 0x0070,
    HID_CONSUMER_RECORD = 0x00b2,
    HID_CONSUMER_FAST_FORWARD = 0x00b3,
    HID_CONSUMER_REWIND = 0x00b4,
    HID_CONSUMER_SCAN_NEXT_TRACK = 0x00b5,
    HID_CONSUMER_SCAN_PREVIOUS_TRACK = 0x00b6,
    HID_CONSUMER_STOP = 0x00b7,
    HID_CONSUMER_EJECT = 0x00b8,
    HID_CONSUMER_PLAY_PAUSE = 0x00cd,
    HID_CONSUMER_MUTE = 0x00e2,
    HID_CONSUMER_VOLUME_INCREMENT = 0x00e9,
    HID_CONSUMER_VOLUME_DECREMENT = 0x00ea,
    HID_CONSUMER_AL_EMAIL_READER = 0x018a,
    HID_CONSUMER_AL_CALCULATOR = 0x0192,
    HID_CONSUMER_AC_SEARCH = 0x0221,
    HID_CONSUMER_AC_HOME = 0x0223,
    HID_CONSUMER_AC_BACK = 0x0224,
    HID_CONSUMER_AC_FORWARD = 0x0225,
    HID_CONSUMER_AC_STOP = 0x0226,
    HID_CONSUMER_AC_REFRESH = 0x0227,
    HID_CONSUMER_AC_BOOKMARKS = 0x022a,
};

/* Generic Desktop System Control usages. */
enum ergodox_hid_system_usage {
    HID_SYSTEM_POWER_DOWN = 0x81,
    HID_SYSTEM_SLEEP = 0x82,
    HID_SYSTEM_WAKE_UP = 0x83,
};

enum ergodox_hid_mouse_button {
    HID_MOUSE_BUTTON_LEFT = BIT(0),
    HID_MOUSE_BUTTON_RIGHT = BIT(1),
    HID_MOUSE_BUTTON_MIDDLE = BIT(2),
    HID_MOUSE_BUTTON_BACK = BIT(3),
    HID_MOUSE_BUTTON_FORWARD = BIT(4),
};

#define ERGODOX_HID_REPORT_ID_KEYBOARD 1
#define ERGODOX_HID_REPORT_ID_CONSUMER 2
#define ERGODOX_HID_REPORT_ID_SYSTEM 3
#define ERGODOX_HID_REPORT_ID_MOUSE 4

#define ERGODOX_HID_CONSUMER_REPORT_SIZE 3
#define ERGODOX_HID_SYSTEM_REPORT_SIZE 2
#define ERGODOX_HID_MOUSE_REPORT_SIZE 5

/* One interface uses report IDs; boot protocol sends only the keyboard payload. */
#define ERGODOX_HID_REPORT_DESC() {                                     \
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                               \
    HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),                           \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                         \
        HID_REPORT_ID(ERGODOX_HID_REPORT_ID_KEYBOARD),                   \
        HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),                    \
        HID_USAGE_MIN8(0xe0), HID_USAGE_MAX8(0xe7),                      \
        HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(1),                        \
        HID_REPORT_SIZE(1), HID_REPORT_COUNT(8), HID_INPUT(0x02),        \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(1), HID_INPUT(0x03),        \
        HID_USAGE_PAGE(HID_USAGE_GEN_LEDS),                              \
        HID_USAGE_MIN8(1), HID_USAGE_MAX8(5),                            \
        HID_REPORT_SIZE(1), HID_REPORT_COUNT(5), HID_OUTPUT(0x02),       \
        HID_REPORT_SIZE(3), HID_REPORT_COUNT(1), HID_OUTPUT(0x03),       \
        HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),                    \
        HID_USAGE_MIN8(0), HID_USAGE_MAX8(101),                          \
        HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(101),                      \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(6), HID_INPUT(0x00),        \
    HID_END_COLLECTION,                                                 \
    HID_USAGE_PAGE(0x0c),                                                 \
    HID_USAGE(0x01),                                                      \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                          \
        HID_REPORT_ID(ERGODOX_HID_REPORT_ID_CONSUMER),                    \
        HID_USAGE_MIN16(0x00, 0x00), HID_USAGE_MAX16(0xff, 0x03),         \
        HID_LOGICAL_MIN16(0x00, 0x00), HID_LOGICAL_MAX16(0xff, 0x03),     \
        HID_REPORT_SIZE(16), HID_REPORT_COUNT(1), HID_INPUT(0x00),        \
    HID_END_COLLECTION,                                                   \
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                               \
    HID_USAGE(0x80),                                                      \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                          \
        HID_REPORT_ID(ERGODOX_HID_REPORT_ID_SYSTEM),                      \
        HID_USAGE_MIN8(HID_SYSTEM_POWER_DOWN),                            \
        HID_USAGE_MAX8(HID_SYSTEM_WAKE_UP),                               \
        HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(HID_SYSTEM_WAKE_UP),        \
        HID_REPORT_SIZE(8), HID_REPORT_COUNT(1), HID_INPUT(0x00),         \
    HID_END_COLLECTION,                                                   \
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                               \
    HID_USAGE(HID_USAGE_GEN_DESKTOP_MOUSE),                              \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                          \
        HID_REPORT_ID(ERGODOX_HID_REPORT_ID_MOUSE),                       \
        HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),                         \
        HID_COLLECTION(HID_COLLECTION_PHYSICAL),                         \
            HID_USAGE_PAGE(HID_USAGE_GEN_BUTTON),                         \
            HID_USAGE_MIN8(1), HID_USAGE_MAX8(5),                         \
            HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(1),                     \
            HID_REPORT_SIZE(1), HID_REPORT_COUNT(5), HID_INPUT(0x02),     \
            HID_REPORT_SIZE(3), HID_REPORT_COUNT(1), HID_INPUT(0x01),     \
            HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),                        \
            HID_USAGE(HID_USAGE_GEN_DESKTOP_X),                           \
            HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),                           \
            HID_USAGE(HID_USAGE_GEN_DESKTOP_WHEEL),                       \
            HID_LOGICAL_MIN8(-127), HID_LOGICAL_MAX8(127),                \
            HID_REPORT_SIZE(8), HID_REPORT_COUNT(3), HID_INPUT(0x06),     \
        HID_END_COLLECTION,                                               \
    HID_END_COLLECTION,                                                   \
}

#endif /* KEYBOARD_HID_H */
