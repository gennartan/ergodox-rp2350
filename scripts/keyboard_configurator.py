#!/usr/bin/env python3
"""
Qt keyboard layout editor for the Ergodox custom firmware.

The editor stores layouts as JSON and can export a C table compatible with
src/keyboard_layout.h. It intentionally keeps the generated C separate from the
hand-written firmware so multiple layouts can live side by side.
"""

from __future__ import annotations

import argparse
import ctypes.util
import json
import math
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    from PySide6 import QtCore, QtGui, QtWidgets
except ImportError:
    try:
        from PyQt6 import QtCore, QtGui, QtWidgets
    except ImportError as exc:
        raise SystemExit(
            "Install PySide6 or PyQt6 first, for example: pip install PySide6"
        ) from exc

Signal = QtCore.Signal if hasattr(QtCore, "Signal") else QtCore.pyqtSignal


KEY_COUNT = 38
HANDS = ["right", "left"]
LAYERS = ["BASE", "NAV", "SYMBOL", "FN"]
ACTION_TYPES = [
    "transparent",
    "none",
    "key",
    "consumer",
    "system",
    "mouse_button",
    "mouse_move",
    "mouse_wheel",
    "lighting",
    "momentary_layer",
    "toggle_layer",
    "default_layer",
]

HID_KEYS = [
	"HID_KEY_A",
	"HID_KEY_B",
	"HID_KEY_C",
	"HID_KEY_D",
	"HID_KEY_E",
	"HID_KEY_F",
	"HID_KEY_G",
	"HID_KEY_H",
	"HID_KEY_I",
	"HID_KEY_J",
	"HID_KEY_K",
	"HID_KEY_L",
	"HID_KEY_M",
	"HID_KEY_N",
	"HID_KEY_O",
	"HID_KEY_P",
	"HID_KEY_Q",
	"HID_KEY_R",
	"HID_KEY_S",
	"HID_KEY_T",
	"HID_KEY_U",
	"HID_KEY_V",
	"HID_KEY_W",
	"HID_KEY_X",
	"HID_KEY_Y",
	"HID_KEY_Z",

	"HID_KEY_1",
	"HID_KEY_2",
	"HID_KEY_3",
	"HID_KEY_4",
	"HID_KEY_5",
	"HID_KEY_6",
	"HID_KEY_7",
	"HID_KEY_8",
	"HID_KEY_9",
	"HID_KEY_0",

	"HID_KEY_ENTER",
	"HID_KEY_ESC",
	"HID_KEY_BACKSPACE",
	"HID_KEY_TAB",
	"HID_KEY_SPACE",

    "HID_KEY_MINUS",
	"HID_KEY_EQUAL",
	"HID_KEY_LEFTBRACE",
	"HID_KEY_RIGHTBRACE",
	"HID_KEY_BACKSLASH",
	"HID_KEY_HASH",
	"HID_KEY_SEMICOLON",
	"HID_KEY_APOSTROPHE",
	"HID_KEY_GRAVE",
	"HID_KEY_COMMA",
	"HID_KEY_DOT",
	"HID_KEY_SLASH",

	"HID_KEY_CAPSLOCK",
	"HID_KEY_F1",
	"HID_KEY_F2",
	"HID_KEY_F3",
	"HID_KEY_F4",
	"HID_KEY_F5",
	"HID_KEY_F6",
	"HID_KEY_F7",
	"HID_KEY_F8",
	"HID_KEY_F9",
	"HID_KEY_F10",
	"HID_KEY_F11",
	"HID_KEY_F12",

	"HID_KEY_SYSRQ",
	"HID_KEY_SCROLLLOCK",
	"HID_KEY_PAUSE",

	"HID_KEY_INSERT",
	"HID_KEY_HOME",
	"HID_KEY_PAGEUP",
	"HID_KEY_DELETE",
	"HID_KEY_END",
	"HID_KEY_PAGEDOWN",
	"HID_KEY_RIGHT",
	"HID_KEY_LEFT",
	"HID_KEY_DOWN",
	"HID_KEY_UP",

	"HID_KEY_NUMLOCK",
	"HID_KEY_KPSLASH",
	"HID_KEY_KPASTERISK",
	"HID_KEY_KPMINUS",
	"HID_KEY_KPPLUS",
	"HID_KEY_KPENTER",
	"HID_KEY_KP_1",
	"HID_KEY_KP_2",
	"HID_KEY_KP_3",
	"HID_KEY_KP_4",
	"HID_KEY_KP_5",
	"HID_KEY_KP_6",
	"HID_KEY_KP_7",
	"HID_KEY_KP_8",
	"HID_KEY_KP_9",
	"HID_KEY_KP_0",
	"ERGODOX_HID_KEY_KP_DOT",
	"ERGODOX_HID_KEY_NON_US_BACKSLASH",
]

SPECIAL_ACTIONS = {
    "consumer": [
        "HID_CONSUMER_VOLUME_INCREMENT",
        "HID_CONSUMER_VOLUME_DECREMENT",
        "HID_CONSUMER_MUTE",
        "HID_CONSUMER_PLAY_PAUSE",
        "HID_CONSUMER_STOP",
        "HID_CONSUMER_SCAN_NEXT_TRACK",
        "HID_CONSUMER_SCAN_PREVIOUS_TRACK",
        "HID_CONSUMER_FAST_FORWARD",
        "HID_CONSUMER_REWIND",
        "HID_CONSUMER_RECORD",
        "HID_CONSUMER_EJECT",
        "HID_CONSUMER_BRIGHTNESS_UP",
        "HID_CONSUMER_BRIGHTNESS_DOWN",
        "HID_CONSUMER_AL_EMAIL_READER",
        "HID_CONSUMER_AL_CALCULATOR",
        "HID_CONSUMER_AC_SEARCH",
        "HID_CONSUMER_AC_HOME",
        "HID_CONSUMER_AC_BACK",
        "HID_CONSUMER_AC_FORWARD",
        "HID_CONSUMER_AC_STOP",
        "HID_CONSUMER_AC_REFRESH",
        "HID_CONSUMER_AC_BOOKMARKS",
    ],
    "system": [
        "HID_SYSTEM_POWER_DOWN",
        "HID_SYSTEM_SLEEP",
        "HID_SYSTEM_WAKE_UP",
    ],
    "mouse_button": [
        "HID_MOUSE_BUTTON_LEFT",
        "HID_MOUSE_BUTTON_RIGHT",
        "HID_MOUSE_BUTTON_MIDDLE",
        "HID_MOUSE_BUTTON_BACK",
        "HID_MOUSE_BUTTON_FORWARD",
    ],
    "mouse_move": [
        "MOUSE_MOVE_UP",
        "MOUSE_MOVE_DOWN",
        "MOUSE_MOVE_LEFT",
        "MOUSE_MOVE_RIGHT",
    ],
    "mouse_wheel": [
        "MOUSE_WHEEL_UP",
        "MOUSE_WHEEL_DOWN",
    ],
    "lighting": [
        "KEYBOARD_LIGHTING_TOGGLE",
        "KEYBOARD_LIGHTING_ON",
        "KEYBOARD_LIGHTING_OFF",
        "KEYBOARD_LIGHTING_BRIGHTNESS_UP",
        "KEYBOARD_LIGHTING_BRIGHTNESS_DOWN",
    ],
}

MOUSE_ACTION_C = {
    "MOUSE_MOVE_UP": "KB_MOUSE_MOVE(0, -KEYBOARD_MOUSE_MOVE_STEP)",
    "MOUSE_MOVE_DOWN": "KB_MOUSE_MOVE(0, KEYBOARD_MOUSE_MOVE_STEP)",
    "MOUSE_MOVE_LEFT": "KB_MOUSE_MOVE(-KEYBOARD_MOUSE_MOVE_STEP, 0)",
    "MOUSE_MOVE_RIGHT": "KB_MOUSE_MOVE(KEYBOARD_MOUSE_MOVE_STEP, 0)",
    "MOUSE_WHEEL_UP": "KB_MOUSE_WHEEL(1)",
    "MOUSE_WHEEL_DOWN": "KB_MOUSE_WHEEL(-1)",
}

MODIFIERS = [
    ("Left Ctrl", "HID_KBD_MODIFIER_LEFT_CTRL"),
    ("Left Shift", "HID_KBD_MODIFIER_LEFT_SHIFT"),
    ("Left Alt", "HID_KBD_MODIFIER_LEFT_ALT"),
    ("Left GUI", "HID_KBD_MODIFIER_LEFT_UI"),
    ("Right Ctrl", "HID_KBD_MODIFIER_RIGHT_CTRL"),
    ("Right Shift", "HID_KBD_MODIFIER_RIGHT_SHIFT"),
    ("Right Alt", "HID_KBD_MODIFIER_RIGHT_ALT"),
    ("Right GUI", "HID_KBD_MODIFIER_RIGHT_UI"),
]

def qt_key_value(key: Any) -> int:
    return int(key.value) if hasattr(key, "value") else int(key)


def qt_modifier_value(modifier: Any) -> int:
    return int(modifier.value) if hasattr(modifier, "value") else int(modifier)


QT_KEY_TO_HID = {
    qt_key_value(QtCore.Qt.Key.Key_A): "HID_KEY_A",
    qt_key_value(QtCore.Qt.Key.Key_B): "HID_KEY_B",
    qt_key_value(QtCore.Qt.Key.Key_C): "HID_KEY_C",
    qt_key_value(QtCore.Qt.Key.Key_D): "HID_KEY_D",
    qt_key_value(QtCore.Qt.Key.Key_E): "HID_KEY_E",
    qt_key_value(QtCore.Qt.Key.Key_F): "HID_KEY_F",
    qt_key_value(QtCore.Qt.Key.Key_G): "HID_KEY_G",
    qt_key_value(QtCore.Qt.Key.Key_H): "HID_KEY_H",
    qt_key_value(QtCore.Qt.Key.Key_I): "HID_KEY_I",
    qt_key_value(QtCore.Qt.Key.Key_J): "HID_KEY_J",
    qt_key_value(QtCore.Qt.Key.Key_K): "HID_KEY_K",
    qt_key_value(QtCore.Qt.Key.Key_L): "HID_KEY_L",
    qt_key_value(QtCore.Qt.Key.Key_M): "HID_KEY_M",
    qt_key_value(QtCore.Qt.Key.Key_N): "HID_KEY_N",
    qt_key_value(QtCore.Qt.Key.Key_O): "HID_KEY_O",
    qt_key_value(QtCore.Qt.Key.Key_P): "HID_KEY_P",
    qt_key_value(QtCore.Qt.Key.Key_Q): "HID_KEY_Q",
    qt_key_value(QtCore.Qt.Key.Key_R): "HID_KEY_R",
    qt_key_value(QtCore.Qt.Key.Key_S): "HID_KEY_S",
    qt_key_value(QtCore.Qt.Key.Key_T): "HID_KEY_T",
    qt_key_value(QtCore.Qt.Key.Key_U): "HID_KEY_U",
    qt_key_value(QtCore.Qt.Key.Key_V): "HID_KEY_V",
    qt_key_value(QtCore.Qt.Key.Key_W): "HID_KEY_W",
    qt_key_value(QtCore.Qt.Key.Key_X): "HID_KEY_X",
    qt_key_value(QtCore.Qt.Key.Key_Y): "HID_KEY_Y",
    qt_key_value(QtCore.Qt.Key.Key_Z): "HID_KEY_Z",
    qt_key_value(QtCore.Qt.Key.Key_1): "HID_KEY_1",
    qt_key_value(QtCore.Qt.Key.Key_2): "HID_KEY_2",
    qt_key_value(QtCore.Qt.Key.Key_3): "HID_KEY_3",
    qt_key_value(QtCore.Qt.Key.Key_4): "HID_KEY_4",
    qt_key_value(QtCore.Qt.Key.Key_5): "HID_KEY_5",
    qt_key_value(QtCore.Qt.Key.Key_6): "HID_KEY_6",
    qt_key_value(QtCore.Qt.Key.Key_7): "HID_KEY_7",
    qt_key_value(QtCore.Qt.Key.Key_8): "HID_KEY_8",
    qt_key_value(QtCore.Qt.Key.Key_9): "HID_KEY_9",
    qt_key_value(QtCore.Qt.Key.Key_0): "HID_KEY_0",
    qt_key_value(QtCore.Qt.Key.Key_Return): "HID_KEY_ENTER",
    qt_key_value(QtCore.Qt.Key.Key_Enter): "HID_KEY_ENTER",
    qt_key_value(QtCore.Qt.Key.Key_Escape): "HID_KEY_ESC",
    qt_key_value(QtCore.Qt.Key.Key_Backspace): "HID_KEY_BACKSPACE",
    qt_key_value(QtCore.Qt.Key.Key_Tab): "HID_KEY_TAB",
    qt_key_value(QtCore.Qt.Key.Key_Space): "HID_KEY_SPACE",
    qt_key_value(QtCore.Qt.Key.Key_Minus): "HID_KEY_MINUS",
    qt_key_value(QtCore.Qt.Key.Key_Equal): "HID_KEY_EQUAL",
    qt_key_value(QtCore.Qt.Key.Key_BracketLeft): "HID_KEY_LEFTBRACE",
    qt_key_value(QtCore.Qt.Key.Key_BracketRight): "HID_KEY_RIGHTBRACE",
    qt_key_value(QtCore.Qt.Key.Key_Backslash): "HID_KEY_BACKSLASH",
    qt_key_value(QtCore.Qt.Key.Key_Semicolon): "HID_KEY_SEMICOLON",
    qt_key_value(QtCore.Qt.Key.Key_Apostrophe): "HID_KEY_APOSTROPHE",
    qt_key_value(QtCore.Qt.Key.Key_QuoteLeft): "HID_KEY_GRAVE",
    qt_key_value(QtCore.Qt.Key.Key_Comma): "HID_KEY_COMMA",
    qt_key_value(QtCore.Qt.Key.Key_Period): "HID_KEY_DOT",
    qt_key_value(QtCore.Qt.Key.Key_Slash): "HID_KEY_SLASH",
    qt_key_value(QtCore.Qt.Key.Key_CapsLock): "HID_KEY_CAPSLOCK",
    qt_key_value(QtCore.Qt.Key.Key_F1): "HID_KEY_F1",
    qt_key_value(QtCore.Qt.Key.Key_F2): "HID_KEY_F2",
    qt_key_value(QtCore.Qt.Key.Key_F3): "HID_KEY_F3",
    qt_key_value(QtCore.Qt.Key.Key_F4): "HID_KEY_F4",
    qt_key_value(QtCore.Qt.Key.Key_F5): "HID_KEY_F5",
    qt_key_value(QtCore.Qt.Key.Key_F6): "HID_KEY_F6",
    qt_key_value(QtCore.Qt.Key.Key_F7): "HID_KEY_F7",
    qt_key_value(QtCore.Qt.Key.Key_F8): "HID_KEY_F8",
    qt_key_value(QtCore.Qt.Key.Key_F9): "HID_KEY_F9",
    qt_key_value(QtCore.Qt.Key.Key_F10): "HID_KEY_F10",
    qt_key_value(QtCore.Qt.Key.Key_F11): "HID_KEY_F11",
    qt_key_value(QtCore.Qt.Key.Key_F12): "HID_KEY_F12",
    qt_key_value(QtCore.Qt.Key.Key_Insert): "HID_KEY_INSERT",
    qt_key_value(QtCore.Qt.Key.Key_Home): "HID_KEY_HOME",
    qt_key_value(QtCore.Qt.Key.Key_PageUp): "HID_KEY_PAGEUP",
    qt_key_value(QtCore.Qt.Key.Key_Delete): "HID_KEY_DELETE",
    qt_key_value(QtCore.Qt.Key.Key_End): "HID_KEY_END",
    qt_key_value(QtCore.Qt.Key.Key_PageDown): "HID_KEY_PAGEDOWN",
    qt_key_value(QtCore.Qt.Key.Key_Right): "HID_KEY_RIGHT",
    qt_key_value(QtCore.Qt.Key.Key_Left): "HID_KEY_LEFT",
    qt_key_value(QtCore.Qt.Key.Key_Down): "HID_KEY_DOWN",
    qt_key_value(QtCore.Qt.Key.Key_Up): "HID_KEY_UP",
}

QT_SPECIAL_KEY_TO_ACTION: dict[int, dict[str, Any]] = {}
for qt_name, action_type, code in [
    ("Key_VolumeUp", "consumer", "HID_CONSUMER_VOLUME_INCREMENT"),
    ("Key_VolumeDown", "consumer", "HID_CONSUMER_VOLUME_DECREMENT"),
    ("Key_VolumeMute", "consumer", "HID_CONSUMER_MUTE"),
    ("Key_MediaPlay", "consumer", "HID_CONSUMER_PLAY_PAUSE"),
    ("Key_MediaPause", "consumer", "HID_CONSUMER_PLAY_PAUSE"),
    ("Key_MediaTogglePlayPause", "consumer", "HID_CONSUMER_PLAY_PAUSE"),
    ("Key_MediaStop", "consumer", "HID_CONSUMER_STOP"),
    ("Key_MediaNext", "consumer", "HID_CONSUMER_SCAN_NEXT_TRACK"),
    ("Key_MediaPrevious", "consumer", "HID_CONSUMER_SCAN_PREVIOUS_TRACK"),
    ("Key_MonBrightnessUp", "consumer", "HID_CONSUMER_BRIGHTNESS_UP"),
    ("Key_MonBrightnessDown", "consumer", "HID_CONSUMER_BRIGHTNESS_DOWN"),
    ("Key_LaunchMail", "consumer", "HID_CONSUMER_AL_EMAIL_READER"),
    ("Key_Calculator", "consumer", "HID_CONSUMER_AL_CALCULATOR"),
    ("Key_Search", "consumer", "HID_CONSUMER_AC_SEARCH"),
    ("Key_HomePage", "consumer", "HID_CONSUMER_AC_HOME"),
    ("Key_Back", "consumer", "HID_CONSUMER_AC_BACK"),
    ("Key_Forward", "consumer", "HID_CONSUMER_AC_FORWARD"),
    ("Key_Stop", "consumer", "HID_CONSUMER_AC_STOP"),
    ("Key_Refresh", "consumer", "HID_CONSUMER_AC_REFRESH"),
    ("Key_Favorites", "consumer", "HID_CONSUMER_AC_BOOKMARKS"),
    ("Key_PowerOff", "system", "HID_SYSTEM_POWER_DOWN"),
    ("Key_Sleep", "system", "HID_SYSTEM_SLEEP"),
    ("Key_WakeUp", "system", "HID_SYSTEM_WAKE_UP"),
]:
    qt_key = getattr(QtCore.Qt.Key, qt_name, None)
    if qt_key is not None:
        QT_SPECIAL_KEY_TO_ACTION[qt_key_value(qt_key)] = {
            "type": action_type,
            "code": code,
        }

QT_MODIFIER_KEY_TO_HID_MODIFIER = {
    qt_key_value(QtCore.Qt.Key.Key_Control): "HID_KBD_MODIFIER_LEFT_CTRL",
    qt_key_value(QtCore.Qt.Key.Key_Shift): "HID_KBD_MODIFIER_LEFT_SHIFT",
    qt_key_value(QtCore.Qt.Key.Key_Alt): "HID_KBD_MODIFIER_LEFT_ALT",
    qt_key_value(QtCore.Qt.Key.Key_Meta): "HID_KBD_MODIFIER_LEFT_UI",
}

QT_MODIFIERS_TO_HID_MODIFIERS = [
    (qt_modifier_value(QtCore.Qt.KeyboardModifier.ControlModifier), "HID_KBD_MODIFIER_LEFT_CTRL"),
    (qt_modifier_value(QtCore.Qt.KeyboardModifier.ShiftModifier), "HID_KBD_MODIFIER_LEFT_SHIFT"),
    (qt_modifier_value(QtCore.Qt.KeyboardModifier.AltModifier), "HID_KBD_MODIFIER_LEFT_ALT"),
    (qt_modifier_value(QtCore.Qt.KeyboardModifier.MetaModifier), "HID_KBD_MODIFIER_LEFT_UI"),
]

KEY_UNIT = 58
KEY_GAP = 8
BOARD_WIDTH = 620
BOARD_HEIGHT = 480


@dataclass(frozen=True)
class KeyPlacement:
    key_index: int
    x: float
    y: float
    w: float = 1.0
    h: float = 1.0
    rotation: float = 0.0


ColumnKey = tuple[int, float, float]
ColumnSpec = tuple[float, float, list[ColumnKey]]

def rotate_key(key: KeyPlacement, pivot_x, pivot_y, angle_degrees):
    angle_rad = math.radians(angle_degrees)

    # Original coordinates
    x, y = key.x, key.y

    # Step 1: translate to origin (pivot becomes 0,0)
    tx = x - pivot_x
    ty = y - pivot_y

    # Step 2: rotate
    rx = tx * math.cos(angle_rad) - ty * math.sin(angle_rad)
    ry = tx * math.sin(angle_rad) + ty * math.cos(angle_rad)

    # Step 3: translate back
    new_x = rx + pivot_x
    new_y = ry + pivot_y

    # Return a new KeyPlacement (keeping other fields the same)
    return KeyPlacement(
        key.key_index,
        new_x,
        new_y,
        key.w,
        key.h,
        key.rotation + angle_degrees # or update if needed
    )

RIGHT_COLUMN_SPECS: list[ColumnSpec] = [
    # x, y, [(key index, width units, height units), ...]
    # Change the key index to move a GPIO-backed key to another physical spot.
    (124, 30, [
        (0, 1.0, 1.0),
        (1, 1.0, 1.5),
        (2, 1.0, 1.5),
    ]),
    (198, 30, [
        (3, 1.0, 1.0),
        (4, 1.0, 1.0),
        (5, 1.0, 1.0),
        (6, 1.0, 1.0),
    ]),
    (272, 25, [
        (7, 1.0, 1.0),
        (8, 1.0, 1.0),
        (9, 1.0, 1.0),
        (10, 1.0, 1.0),
        (11, 1.0, 1.0),
    ]),
    (346, 20, [
        (12, 1.0, 1.0),
        (13, 1.0, 1.0),
        (14, 1.0, 1.0),
        (15, 1.0, 1.0),
        (16, 1.0, 1.0),
    ]),
    (420, 25, [
        (17, 1.0, 1.0),
        (18, 1.0, 1.0),
        (19, 1.0, 1.0),
        (20, 1.0, 1.0),
        (21, 1.0, 1.0),
    ]),
    (494, 30, [
        (22, 1.0, 1.0),
        (23, 1.0, 1.0),
        (24, 1.0, 1.0),
        (25, 1.0, 1.0),
        (26, 1.0, 1.0),
    ]),
    (568, 30, [
        (31, 1.5, 1.0),
        (30, 1.5, 1.0),
        (29, 1.5, 1.0),
        (28, 1.5, 1.0),
        (27, 1.0, 1.0),
    ]),
]

RIGHT_THUMB_SPECS = [rotate_key(key, 0, 300, 0) for key in [
    KeyPlacement(32, 66, 300, 1.0, 1.0, 0),
    KeyPlacement(33, 0, 300, 1.0, 1.0, 0),
    KeyPlacement(34, 0, 366, 1.0, 1.0, 0),
    KeyPlacement(35, 0, 432, 1.0, 1.0, 0),
    KeyPlacement(36, 66, 374, 1.0, 2.0, 0),
    KeyPlacement(37, 132, 374, 1.0, 2.0, 0),
]]

LEFT_COLUMN_SPECS: list[ColumnSpec] = [
    # x, y, [(key index, width units, height units), ...]
    # Change the key index to move a GPIO-backed key to another physical spot.
    (0, 30, [
        (26, 1.5, 1.0),
        (25, 1.5, 1.0),
        (24, 1.5, 1.0),
        (23, 1.5, 1.0),
        (22, 1.0, 1.0),
    ]),
    (98, 30, [
        (31, 1.0, 1.0),        
        (30, 1.0, 1.0),
        (29, 1.0, 1.0),
        (28, 1.0, 1.0),
        (27, 1.0, 1.0),
    ]),
    (172, 30, [
        (4, 1.0, 1.0),
        (3, 1.0, 1.0),
        (2, 1.0, 1.0),
        (1, 1.0, 1.0),
        (0, 1.0, 1.0),
    ]),
    (246, 25, [
        (9, 1.0, 1.0),
        (8, 1.0, 1.0),
        (7, 1.0, 1.0),
        (6, 1.0, 1.0),
        (5, 1.0, 1.0),
    ]),
    (320, 20, [
        (14, 1.0, 1.0),
        (13, 1.0, 1.0),
        (12, 1.0, 1.0),
        (11, 1.0, 1.0),
        (10, 1.0, 1.0),
    ]),
    (394, 25, [
        (18, 1.0, 1.0),
        (17, 1.0, 1.0),
        (16, 1.0, 1.0),
        (15, 1.0, 1.0),
    ]),
    (468, 30, [
        (21, 1.0, 1.5),
        (20, 1.0, 1.5),
        (19, 1.0, 1.5),

    ]),

]

DX = 500
LEFT_THUMB_SPECS = [rotate_key(key, 0, 300, 0) for key in [
    KeyPlacement(37, 66 + DX, 300, 1.0, 1.0, 0),
    KeyPlacement(36, 132 + DX, 300, 1.0, 1.0, 0),
    KeyPlacement(35, 132 + DX, 366, 1.0, 1.0, 0),
    KeyPlacement(34, 132 + DX, 432, 1.0, 1.0, 0),
    KeyPlacement(33, 66 + DX, 374, 1.0, 2.0, 0),
    KeyPlacement(32, 0 + DX, 374, 1.0, 2.0, 0),
]]

def make_placements(COLUMN_SPECS, THUMB_SPECS):
    placements: list[KeyPlacement] = []

    for x, y, keys in COLUMN_SPECS:
        current_y = y
        for key_index, width, height in keys:
            placements.append(KeyPlacement(key_index, x, current_y, width, height))
            current_y += height * KEY_UNIT + KEY_GAP

    placements.extend(THUMB_SPECS)

    keys = sorted(placement.key_index for placement in placements)
    expected = list(range(KEY_COUNT))
    if keys != expected:
        raise ValueError(
            f"Placement must contain each key index 0..{KEY_COUNT - 1} once; got {keys}"
        )

    return placements

KEY_PLACEMENTS_RIGHT = make_placements(RIGHT_COLUMN_SPECS, RIGHT_THUMB_SPECS)
KEY_PLACEMENTS_LEFT = make_placements(LEFT_COLUMN_SPECS, LEFT_THUMB_SPECS)


def configure_qt_platform(platform: str) -> None:
    if platform == "auto" and os.environ.get("QT_QPA_PLATFORM"):
        platform = os.environ["QT_QPA_PLATFORM"]
    elif platform == "auto":
        if os.environ.get("WAYLAND_DISPLAY"):
            platform = "wayland"
        elif os.environ.get("DISPLAY"):
            platform = "xcb"
        else:
            platform = "minimal"

    if platform == "xcb" and ctypes.util.find_library("xcb-cursor") is None:
        raise SystemExit(
            "Qt is using Xorg/XCB, but libxcb-cursor is missing.\n"
            "Install it with: sudo apt install libxcb-cursor0\n"
            "Or run with --platform wayland if you are in a Wayland session."
        )

    os.environ["QT_QPA_PLATFORM"] = platform


@dataclass
class KeyInfo:
    index: int
    label: str
    gpio: str


def empty_action(layer: str) -> dict[str, Any]:
    return {"type": "none" if layer == "BASE" else "transparent"}


def default_layout() -> dict[str, Any]:
    return {
        "name": "custom",
        "right": {
            "layers": {
                layer: [empty_action(layer) for _ in range(KEY_COUNT)]
                for layer in LAYERS
            },
        },
        "left": {
            "layers": {
                layer: [empty_action(layer) for _ in range(KEY_COUNT)]
                for layer in LAYERS
            },
        }
    }


def parse_key_infos(repo_root: Path) -> list[KeyInfo]:
    overlay = repo_root / "app.overlay"
    text = overlay.read_text(encoding="utf-8") if overlay.exists() else ""
    infos: list[KeyInfo] = []

    for index in range(KEY_COUNT):
        key_name = f"key{index + 1}"
        match = re.search(
            rf"{key_name}\s*:\s*{key_name}\s*\{{\s*gpios\s*=\s*<([^>]+)>;",
            text,
        )
        gpio = match.group(1).strip() if match else "unknown"
        gpio = gpio.replace("&", "")
        infos.append(KeyInfo(index=index, label=key_name, gpio=gpio))

    return infos


def load_layout(path: Path | None) -> dict[str, Any]:
    if path is None or not path.exists():
        return default_layout()

    data = json.loads(path.read_text(encoding="utf-8"))
    layout = default_layout()
    layout["name"] = data.get("name", layout["name"])

    for hand in HANDS:
        hand_data = data.get(hand, {})
        for layer in LAYERS:
            source = hand_data.get("layers", {}).get(layer, [])
            for index, action in enumerate(source[:KEY_COUNT]):
                layout[hand]["layers"][layer][index] = normalize_action(action, layer)

    return layout


def normalize_action(action: dict[str, Any], layer: str) -> dict[str, Any]:
    if not isinstance(action, dict):
        return empty_action(layer)

    action_type = action.get("type", empty_action(layer)["type"])
    if action_type not in ACTION_TYPES:
        action_type = empty_action(layer)["type"]

    normalized = {"type": action_type}
    if action_type == "key":
        normalized["keycode"] = action.get("keycode", "HID_KEY_A")
        normalized["modifiers"] = list(action.get("modifiers", []))
    elif action_type in SPECIAL_ACTIONS:
        valid_codes = SPECIAL_ACTIONS[action_type]
        code = action.get("code", valid_codes[0])
        normalized["code"] = code if code in valid_codes else valid_codes[0]
    elif action_type in {"momentary_layer", "toggle_layer", "default_layer"}:
        normalized["layer"] = action.get("layer", "NAV")

    return normalized


def event_to_key_action(event: QtGui.QKeyEvent) -> dict[str, Any] | None:
    key = event.key()
    modifiers = []
    event_modifiers = qt_modifier_value(event.modifiers())

    if key in QT_SPECIAL_KEY_TO_ACTION:
        return dict(QT_SPECIAL_KEY_TO_ACTION[key])

    for qt_modifier, hid_modifier in QT_MODIFIERS_TO_HID_MODIFIERS:
        if event_modifiers & qt_modifier:
            modifiers.append(hid_modifier)

    if key in QT_MODIFIER_KEY_TO_HID_MODIFIER:
        modifier = QT_MODIFIER_KEY_TO_HID_MODIFIER[key]
        if modifier not in modifiers:
            modifiers.append(modifier)
        return {"type": "key", "keycode": "0", "modifiers": modifiers}

    keycode = QT_KEY_TO_HID.get(key)
    if keycode is None:
        return None

    return {"type": "key", "keycode": keycode, "modifiers": modifiers}


def action_label(action: dict[str, Any]) -> str:
    action_type = action.get("type")
    if action_type == "transparent":
        return "TRNS"
    if action_type == "none":
        return "NONE"
    if action_type == "key":
        key = action.get("keycode", "")
        mods = action.get("modifiers", [])
        if key == "0":
            return "+".join(short_modifier(mod) for mod in mods) if mods else "NONE"
        if mods:
            return "+".join([short_modifier(mod) for mod in mods] + [short_key(key)])
        return short_key(key)
    if action_type in SPECIAL_ACTIONS:
        return short_special(action.get("code", ""))
    if action_type == "momentary_layer":
        return f"MO({action.get('layer', '')})"
    if action_type == "toggle_layer":
        return f"TG({action.get('layer', '')})"
    if action_type == "default_layer":
        return f"DF({action.get('layer', '')})"
    return ""


def short_key(key: str) -> str:
    return key.removeprefix("HID_KEY_")

def short_special(code: str) -> str:
    prefixes = (
        "HID_CONSUMER_",
        "HID_SYSTEM_",
        "HID_MOUSE_BUTTON_",
        "KEYBOARD_LIGHTING_",
        "MOUSE_",
    )
    for prefix in prefixes:
        if code.startswith(prefix):
            return code.removeprefix(prefix)
    return code


def short_modifier(modifier: str) -> str:
    return (
        modifier.removeprefix("HID_KBD_MODIFIER_")
        .replace("LEFT_", "L")
        .replace("RIGHT_", "R")
    )


def short_gpio(gpio: str) -> str:
    parts = gpio.split()
    if len(parts) >= 2:
        return f"{parts[0].upper()} {parts[1]}"
    return gpio


def layer_enum(layer: str) -> str:
    return f"KEYBOARD_LAYER_{layer}"

def keyboard_hand_enum(hand: str) -> str:
    return f"KEYBOARD_HAND_{hand.upper()}"

def key_placements(hand: str) -> list[KeyPlacement]:
    if hand == "left":
        return KEY_PLACEMENTS_LEFT

    return KEY_PLACEMENTS_RIGHT

def action_to_c(action: dict[str, Any]) -> str:
    action_type = action.get("type")
    if action_type == "transparent":
        return "KB_TRNS"
    if action_type == "none":
        return "KB_NONE"
    if action_type == "momentary_layer":
        return f"KB_MO({layer_enum(action.get('layer', 'NAV'))})"
    if action_type == "toggle_layer":
        return f"KB_TG({layer_enum(action.get('layer', 'NAV'))})"
    if action_type == "default_layer":
        return f"KB_DF({layer_enum(action.get('layer', 'BASE'))})"
    if action_type == "key":
        keycode = action.get("keycode", "HID_KEY_A")
        modifiers = action.get("modifiers", [])
        if not modifiers:
            return f"KB_KEY({keycode})"
        return f"KB_MOD({' | '.join(modifiers)}, {keycode})"
    if action_type == "consumer":
        return f"KB_CONSUMER({action.get('code', SPECIAL_ACTIONS['consumer'][0])})"
    if action_type == "system":
        return f"KB_SYSTEM({action.get('code', SPECIAL_ACTIONS['system'][0])})"
    if action_type == "mouse_button":
        return f"KB_MOUSE_BUTTON({action.get('code', SPECIAL_ACTIONS['mouse_button'][0])})"
    if action_type in {"mouse_move", "mouse_wheel"}:
        code = action.get("code", SPECIAL_ACTIONS[action_type][0])
        return MOUSE_ACTION_C.get(code, "KB_NONE")
    if action_type == "lighting":
        return f"KB_LIGHTING({action.get('code', SPECIAL_ACTIONS['lighting'][0])})"
    return "KB_NONE"


def export_c(layout: dict[str, Any]) -> str:
    c_name = re.sub(r"[^a-zA-Z0-9_]", "_", layout["name"]).lower()
    lines = [
        "#include \"keyboard_layout.h\"",
        "",
        f"/* Generated layout: {layout['name']} - {KEY_COUNT} keys per half). */",
    ]

    lines.append(f"static const struct keyboard_action {c_name}_layers[KEYBOARD_HAND_COUNT][KEYBOARD_MAX_LAYERS][KEYBOARD_KEY_COUNT] = {{")
    for hand in HANDS:
        lines.append(f"    [{keyboard_hand_enum(hand)}] = {{")
        for layer in LAYERS:
            lines.append(f"        [{layer_enum(layer)}] = {{")
            for index, action in enumerate(layout[hand]["layers"][layer]):
                if action.get("type") == "transparent" and layer != "BASE":
                    continue
                if action.get("type") == "none" and layer == "BASE":
                    continue
                lines.append(f"            [{index}] = {action_to_c(action)},")
            lines.append("        },")
        lines.append("    },")

    lines.extend([
        "};",
        "",
        f"static const struct keyboard_layout {c_name}_layout = {{",
        f"    .name = \"{layout['name']}\",",
        "    .layer_count = KEYBOARD_LAYER_COUNT,",
        "    .default_layer = KEYBOARD_LAYER_BASE,",
        f"    .layers = &{c_name}_layers,",
        "};",
        "",
    ])
    return "\n".join(lines)


class KeyButton(QtWidgets.QPushButton):
    selected = Signal(int)

    def __init__(self, info: KeyInfo) -> None:
        super().__init__()
        self.info = info
        self.setCheckable(True)
        self.setMinimumSize(1, 1)
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Fixed, QtWidgets.QSizePolicy.Policy.Fixed)
        self.clicked.connect(lambda: self.selected.emit(self.info.index))


class Configurator(QtWidgets.QMainWindow):
    def __init__(self, repo_root: Path, layout_path: Path | None) -> None:
        super().__init__()
        self.repo_root = repo_root
        self.layout_path = layout_path
        self.layout = load_layout(layout_path)
        self.current_hand = "right"
        self.key_infos = parse_key_infos(repo_root)
        self.current_layer = LAYERS[0]
        self.current_key = 0
        self.capture_next_key = False
        self.buttons: list[KeyButton] = []

        self.setWindowTitle("Ergodox Custom Keyboard Configurator")
        self.resize(1180, 760)
        self.setFocusPolicy(QtCore.Qt.FocusPolicy.StrongFocus)
        self.build_ui()
        self.select_key(0)

    def build_ui(self) -> None:
        root = QtWidgets.QWidget()
        outer = QtWidgets.QHBoxLayout(root)

        left = QtWidgets.QVBoxLayout()

        selector_row = QtWidgets.QHBoxLayout()
        selector_row.addWidget(QtWidgets.QLabel("Half"))
        self.hand_selector = QtWidgets.QComboBox()
        self.hand_selector.addItems([hand.title() for hand in HANDS])
        self.hand_selector.setCurrentIndex(HANDS.index("right"))
        self.hand_selector.currentIndexChanged.connect(self.on_hand_changed)
        selector_row.addWidget(self.hand_selector)
        selector_row.addStretch(1)
        left.addLayout(selector_row)

        self.layer_tabs = QtWidgets.QTabBar()
        for layer in LAYERS:
            self.layer_tabs.addTab(layer.title())
        self.layer_tabs.currentChanged.connect(self.on_layer_changed)
        left.addWidget(self.layer_tabs)

        self.board_box = QtWidgets.QGroupBox()
        board_box_layout = QtWidgets.QVBoxLayout(self.board_box)
        self.board_scene = QtWidgets.QGraphicsScene(self)
        self.board_view = QtWidgets.QGraphicsView(self.board_scene)
        self.board_view.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)
        self.board_view.setMinimumSize(BOARD_WIDTH + 40, BOARD_HEIGHT + 40)
        self.board_view.setSceneRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT)
        self.board_view.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.board_view.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        board_box_layout.addWidget(self.board_view)
        self.render_keyboard()
        left.addWidget(self.board_box)
        left.addStretch(1)

        right = self.build_editor()
        outer.addLayout(left, 3)
        outer.addWidget(right, 1)
        self.setCentralWidget(root)

        toolbar = self.addToolBar("File")
        toolbar.addAction("New", self.new_layout)
        toolbar.addAction("Open JSON", self.open_json)
        toolbar.addAction("Save JSON", self.save_json)
        toolbar.addAction("Save JSON As", self.save_json_as)
        toolbar.addAction("Export C", self.export_c_file)
        capture_shortcut = QtGui.QShortcut(QtGui.QKeySequence("Ctrl+K"), self)
        capture_shortcut.activated.connect(self.start_key_capture)

        self.refresh_buttons()

    def build_editor(self) -> QtWidgets.QWidget:
        panel = QtWidgets.QWidget()
        form = QtWidgets.QFormLayout(panel)

        self.layout_name = QtWidgets.QLineEdit(self.layout["name"])
        self.layout_name.textChanged.connect(self.on_layout_name_changed)
        form.addRow("Layout", self.layout_name)

        self.selected_label = QtWidgets.QLabel()
        form.addRow("Selected", self.selected_label)

        self.capture_button = QtWidgets.QPushButton("Capture Next Key")
        self.capture_button.setCheckable(True)
        self.capture_button.clicked.connect(self.start_key_capture)
        form.addRow(self.capture_button)

        self.capture_status = QtWidgets.QLabel("Select a visual key, then capture from your keyboard.")
        self.capture_status.setWordWrap(True)
        self.capture_status.setObjectName("captureStatus")
        form.addRow("Capture", self.capture_status)

        self.action_type = QtWidgets.QComboBox()
        self.action_type.addItems(ACTION_TYPES)
        self.action_type.currentTextChanged.connect(self.on_action_changed)
        form.addRow("Action", self.action_type)

        self.keycode = QtWidgets.QComboBox()
        self.keycode.setEditable(True)
        self.keycode.addItems(HID_KEYS)
        self.keycode.currentTextChanged.connect(self.on_action_changed)
        form.addRow("Key", self.keycode)

        self.special_code = QtWidgets.QComboBox()
        self.special_code.currentTextChanged.connect(self.on_action_changed)
        form.addRow("Special", self.special_code)

        self.modifier_checks: list[QtWidgets.QCheckBox] = []
        modifiers = QtWidgets.QWidget()
        modifier_layout = QtWidgets.QGridLayout(modifiers)
        for index, (label, value) in enumerate(MODIFIERS):
            checkbox = QtWidgets.QCheckBox(label)
            checkbox.setProperty("modifier_value", value)
            checkbox.stateChanged.connect(self.on_action_changed)
            self.modifier_checks.append(checkbox)
            modifier_layout.addWidget(checkbox, index // 2, index % 2)
        form.addRow("Modifiers", modifiers)

        self.target_layer = QtWidgets.QComboBox()
        self.target_layer.addItems(LAYERS)
        self.target_layer.currentTextChanged.connect(self.on_action_changed)
        form.addRow("Layer", self.target_layer)

        self.copy_layer_button = QtWidgets.QPushButton("Copy Base To This Layer")
        self.copy_layer_button.clicked.connect(self.copy_base_to_current_layer)
        form.addRow(self.copy_layer_button)

        self.clear_key_button = QtWidgets.QPushButton("Clear Key")
        self.clear_key_button.clicked.connect(self.clear_key)
        form.addRow(self.clear_key_button)

        self.c_preview = QtWidgets.QPlainTextEdit()
        self.c_preview.setReadOnly(True)
        self.c_preview.setMinimumHeight(240)
        form.addRow("C preview", self.c_preview)

        return panel

    def render_keyboard(self) -> None:
        self.board_scene.clear()
        self.buttons = []
        self.board_box.setTitle(f"{self.current_hand.title()} half, {KEY_COUNT} keys")

        for placement in key_placements(self.current_hand):
            info = self.key_infos[placement.key_index]
            button = KeyButton(info)
            button.setFixedSize(
                int(placement.w * KEY_UNIT),
                int(placement.h * KEY_UNIT),
            )
            button.selected.connect(self.select_key)
            self.buttons.append(button)

            proxy = self.board_scene.addWidget(button)
            proxy.setPos(placement.x, placement.y)
            proxy.setTransformOriginPoint(button.width() / 2, button.height() / 2)
            proxy.setRotation(placement.rotation)

        self.refresh_buttons()

    def start_key_capture(self) -> None:
        self.capture_next_key = True
        self.capture_button.setChecked(True)
        self.capture_status.setText(
            f"Press a key for {self.key_infos[self.current_key].label}. Esc cancels."
        )
        self.setFocus()

    def stop_key_capture(self, message: str) -> None:
        self.capture_next_key = False
        self.capture_button.setChecked(False)
        self.capture_status.setText(message)

    def keyPressEvent(self, event: QtGui.QKeyEvent) -> None:
        if not self.capture_next_key:
            super().keyPressEvent(event)
            return

        if event.key() == qt_key_value(QtCore.Qt.Key.Key_Escape):
            self.stop_key_capture("Capture cancelled.")
            event.accept()
            return

        action = event_to_key_action(event)
        if action is None:
            self.stop_key_capture(f"Unsupported key: {event.text() or event.key()}")
            event.accept()
            return

        self.set_current_action(action)
        self.load_action_to_editor(self.current_action())
        self.stop_key_capture(
            f"Captured {action_label(action)} for {self.key_infos[self.current_key].label}."
        )
        event.accept()

    def current_action(self) -> dict[str, Any]:
        return self.layout[self.current_hand]["layers"][self.current_layer][self.current_key]

    def set_current_action(self, action: dict[str, Any]) -> None:
        self.layout[self.current_hand]["layers"][self.current_layer][self.current_key] = normalize_action(
            action, self.current_layer
        )
        self.refresh_buttons()
        self.refresh_preview()

    def select_key(self, index: int) -> None:
        self.current_key = index
        for button in self.buttons:
            button.setChecked(button.info.index == index)
        info = self.key_infos[index]
        self.selected_label.setText(f"{info.label} / {info.gpio}")
        if not self.capture_next_key:
            self.capture_status.setText(
                f"Selected {info.label}. Click Capture Next Key, then press a physical key."
            )
        self.load_action_to_editor(self.current_action())

    def on_layer_changed(self, index: int) -> None:
        self.current_layer = LAYERS[index]
        self.refresh_buttons()
        self.select_key(self.current_key)

    def on_hand_changed(self, index: int) -> None:
        self.current_hand = HANDS[index]
        self.render_keyboard()
        self.select_key(min(self.current_key, KEY_COUNT - 1))
        self.refresh_preview()

    def load_action_to_editor(self, action: dict[str, Any]) -> None:
        self.block_editor_signals(True)
        action_type = action.get("type", empty_action(self.current_layer)["type"])
        self.action_type.setCurrentText(action_type)
        self.keycode.setCurrentText(action.get("keycode", "HID_KEY_A"))
        self.populate_special_codes(action_type, action.get("code"))
        self.target_layer.setCurrentText(action.get("layer", "NAV"))
        modifiers = set(action.get("modifiers", []))
        for checkbox in self.modifier_checks:
            checkbox.setChecked(checkbox.property("modifier_value") in modifiers)
        self.block_editor_signals(False)
        self.update_editor_visibility()
        self.refresh_preview()

    def block_editor_signals(self, block: bool) -> None:
        widgets = [
            self.action_type,
            self.keycode,
            self.special_code,
            self.target_layer,
            *self.modifier_checks,
        ]
        for widget in widgets:
            widget.blockSignals(block)

    def populate_special_codes(self, action_type: str, selected: str | None = None) -> None:
        codes = SPECIAL_ACTIONS.get(action_type, [])
        previous = self.special_code.blockSignals(True)
        self.special_code.clear()
        self.special_code.addItems(codes)
        if selected in codes:
            self.special_code.setCurrentText(selected)
        self.special_code.blockSignals(previous)

    def on_action_changed(self, *_args: Any) -> None:
        action_type = self.action_type.currentText()
        if action_type in SPECIAL_ACTIONS:
            current_codes = [
                self.special_code.itemText(index)
                for index in range(self.special_code.count())
            ]
            if current_codes != SPECIAL_ACTIONS[action_type]:
                self.populate_special_codes(action_type)

        action: dict[str, Any] = {"type": action_type}
        if action_type == "key":
            action["keycode"] = self.keycode.currentText()
            action["modifiers"] = [
                checkbox.property("modifier_value")
                for checkbox in self.modifier_checks
                if checkbox.isChecked()
            ]
        elif action_type in SPECIAL_ACTIONS:
            action["code"] = self.special_code.currentText()
        elif action_type in {"momentary_layer", "toggle_layer", "default_layer"}:
            action["layer"] = self.target_layer.currentText()
        self.set_current_action(action)
        self.update_editor_visibility()

    def update_editor_visibility(self) -> None:
        action_type = self.action_type.currentText()
        key_visible = action_type == "key"
        special_visible = action_type in SPECIAL_ACTIONS
        layer_visible = action_type in {"momentary_layer", "toggle_layer", "default_layer"}
        self.keycode.setEnabled(key_visible)
        self.special_code.setEnabled(special_visible)
        for checkbox in self.modifier_checks:
            checkbox.setEnabled(key_visible)
        self.target_layer.setEnabled(layer_visible)

    def refresh_buttons(self) -> None:
        for button in self.buttons:
            action = self.layout[self.current_hand]["layers"][self.current_layer][button.info.index]
            button.setText(
                f"{button.info.label}\n{action_label(action)}\n{short_gpio(button.info.gpio)}"
            )

    def refresh_preview(self) -> None:
        self.c_preview.setPlainText(export_c(self.layout))

    def on_layout_name_changed(self, text: str) -> None:
        self.layout["name"] = text.strip() or "custom"
        self.refresh_preview()


        self.clear_key_button = QtWidgets.QPushButton("Clear Key")
        self.clear_key_button.clicked.connect(self.clear_key)
        form.addRow(self.clear_key_button)

    def copy_base_to_current_layer(self) -> None:
        if self.current_layer == "BASE":
            return
        self.layout[self.current_hand]["layers"][self.current_layer] = [
            dict(action) for action in self.layout[self.current_hand]["layers"]["BASE"]
        ]
        self.refresh_buttons()
        self.select_key(self.current_key)

    def clear_key(self) -> None:
        self.set_current_action(empty_action(self.current_layer))
        self.select_key(self.current_key)

    def new_layout(self) -> None:
        self.layout = default_layout()
        self.layout_path = None
        self.layout_name.setText(self.layout["name"])
        self.hand_selector.setCurrentIndex(HANDS.index(self.current_hand))
        self.render_keyboard()
        self.select_key(0)

    def open_json(self) -> None:
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Open layout JSON", str(self.repo_root), "JSON (*.json)"
        )
        if not path:
            return
        self.layout_path = Path(path)
        self.layout = load_layout(self.layout_path)
        self.layout_name.setText(self.layout["name"])
        self.hand_selector.setCurrentIndex(HANDS.index(self.current_hand))
        self.render_keyboard()
        self.select_key(self.current_key)

    def save_json(self) -> None:
        if self.layout_path is None:
            self.save_json_as()
            return
        self.layout_path.write_text(json.dumps(self.layout, indent=2) + "\n", encoding="utf-8")

    def save_json_as(self) -> None:
        path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Save layout JSON", str(self.repo_root / "layouts" / "custom.json"), "JSON (*.json)"
        )
        if not path:
            return
        self.layout_path = Path(path)
        self.layout_path.parent.mkdir(parents=True, exist_ok=True)
        self.save_json()

    def export_c_file(self) -> None:
        default_name = f"{self.layout['name'].lower().replace(' ', '_')}_layout.c"
        path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Export C layout", str(self.repo_root / "src" / default_name), "C source (*.c)"
        )
        if not path:
            return
        Path(path).write_text(export_c(self.layout), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Configure Ergodox custom keyboard layouts.")
    parser.add_argument(
        "--platform",
        choices=("auto", "xcb", "wayland", "offscreen", "minimal"),
        default="auto",
        help="Qt display backend. Default: auto-detect Wayland, Xorg/XCB, or minimal.",
    )
    parser.add_argument("layout", nargs="?", type=Path, help="Optional layout JSON to open")
    args = parser.parse_args()

    configure_qt_platform(args.platform)

    repo_root = Path(__file__).resolve().parents[1]
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("Ergodox Custom Keyboard Configurator")
    app.setStyleSheet("""
        QMainWindow {
            background: #f6f7f9;
        }
        QWidget {
            font-size: 10px;
            color: #1f2937;
        }
        QLabel, QCheckBox, QRadioButton {
            color: #1f2937;
        }
        QToolBar {
            background: #f6f7f9;
            color: #1f2937;
            border-bottom: 1px solid #c8ced8;
            spacing: 4px;
        }
        QToolButton {
            background: transparent;
            color: #1f2937;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px 6px;
        }
        QToolButton:hover {
            background: #e8eef7;
            border-color: #c8ced8;
        }
        QMenu {
            background: #ffffff;
            color: #111827;
            border: 1px solid #aab3c1;
            padding: 4px;
        }
        QMenu::item {
            background: transparent;
            color: #111827;
            padding: 4px 18px;
        }
        QMenu::item:selected {
            background: #dbe8ff;
            color: #111827;
        }
        QCheckBox:disabled, QRadioButton:disabled {
            color: #6b7280;
        }
        QCheckBox::indicator {
            background: #ffffff;
            border: 1px solid #6b7280;
            border-radius: 3px;
            width: 12px;
            height: 12px;
        }
        QCheckBox::indicator:checked {
            background: #2d6cdf;
            border-color: #1d4ed8;
        }
        QCheckBox::indicator:disabled {
            background: #e5e7eb;
            border-color: #9ca3af;
        }
        QTabBar {
            background: #f6f7f9;
            color: #1f2937;
        }
        QTabBar::tab {
            background: #ffffff;
            color: #1f2937;
            border: 1px solid #aab3c1;
            border-bottom-color: #c8ced8;
            padding: 5px 12px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: #dbe8ff;
            color: #111827;
            border-color: #2d6cdf;
        }
        QTabBar::tab:!selected {
            background: #eef2f6;
            color: #374151;
        }
        QLineEdit, QComboBox, QPlainTextEdit {
            background: #ffffff;
            color: #111827;
            border: 1px solid #aab3c1;
            border-radius: 4px;
            padding: 3px;
            selection-background-color: #2d6cdf;
            selection-color: #ffffff;
        }
        QComboBox QAbstractItemView {
            background: #ffffff;
            color: #111827;
            selection-background-color: #dbe8ff;
            selection-color: #111827;
        }
        QGroupBox {
            color: #111827;
            font-weight: 600;
            border: 1px solid #c8ced8;
            border-radius: 8px;
            margin-top: 10px;
            background: #eef2f6;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }
        QPushButton {
            border: 1px solid #aab3c1;
            border-radius: 8px;
            background: #ffffff;
            padding: 4px;
            color: #1f2937;
        }
        QPushButton:checked {
            border: 2px solid #2d6cdf;
            background: #dbe8ff;
            color: #1f2937;
        }
        QLabel#captureStatus {
            background: #eef6ff;
            color: #1f2937;
            border: 1px solid #bfdbfe;
            border-radius: 6px;
            padding: 6px;
        }
    """)
    window = Configurator(repo_root, args.layout)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
