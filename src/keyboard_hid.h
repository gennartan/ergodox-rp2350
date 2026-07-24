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

#endif /* KEYBOARD_HID_H */
