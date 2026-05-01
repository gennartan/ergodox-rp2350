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
    "momentary_layer",
    "toggle_layer",
    "default_layer",
]

HID_KEYS = [
    "HID_KEY_A", "HID_KEY_B", "HID_KEY_C", "HID_KEY_D", "HID_KEY_E",
    "HID_KEY_F", "HID_KEY_G", "HID_KEY_H", "HID_KEY_I", "HID_KEY_J",
    "HID_KEY_K", "HID_KEY_L", "HID_KEY_M", "HID_KEY_N", "HID_KEY_O",
    "HID_KEY_P", "HID_KEY_Q", "HID_KEY_R", "HID_KEY_S", "HID_KEY_T",
    "HID_KEY_U", "HID_KEY_V", "HID_KEY_W", "HID_KEY_X", "HID_KEY_Y",
    "HID_KEY_Z",
    "HID_KEY_1", "HID_KEY_2", "HID_KEY_3", "HID_KEY_4", "HID_KEY_5",
    "HID_KEY_6", "HID_KEY_7", "HID_KEY_8", "HID_KEY_9", "HID_KEY_0",
    "HID_KEY_ENTER", "HID_KEY_ESC", "HID_KEY_BACKSPACE", "HID_KEY_TAB",
    "HID_KEY_SPACE", "HID_KEY_MINUS", "HID_KEY_EQUAL",
    "HID_KEY_LEFTBRACE", "HID_KEY_RIGHTBRACE", "HID_KEY_BACKSLASH",
    "HID_KEY_SEMICOLON", "HID_KEY_APOSTROPHE", "HID_KEY_GRAVE",
    "HID_KEY_COMMA", "HID_KEY_DOT", "HID_KEY_SLASH",
    "HID_KEY_CAPSLOCK",
    "HID_KEY_F1", "HID_KEY_F2", "HID_KEY_F3", "HID_KEY_F4",
    "HID_KEY_F5", "HID_KEY_F6", "HID_KEY_F7", "HID_KEY_F8",
    "HID_KEY_F9", "HID_KEY_F10", "HID_KEY_F11", "HID_KEY_F12",
    "HID_KEY_PRINTSCREEN", "HID_KEY_SCROLLLOCK", "HID_KEY_PAUSE",
    "HID_KEY_INSERT", "HID_KEY_HOME", "HID_KEY_PAGEUP",
    "HID_KEY_DELETE", "HID_KEY_END", "HID_KEY_PAGEDOWN",
    "HID_KEY_RIGHT", "HID_KEY_LEFT", "HID_KEY_DOWN", "HID_KEY_UP",
    "HID_KEY_NUMLOCK", "HID_KEY_KPSLASH", "HID_KEY_KPASTERISK",
    "HID_KEY_KPMINUS", "HID_KEY_KPPLUS", "HID_KEY_KPENTER",
    "HID_KEY_KP1", "HID_KEY_KP2", "HID_KEY_KP3", "HID_KEY_KP4",
    "HID_KEY_KP5", "HID_KEY_KP6", "HID_KEY_KP7", "HID_KEY_KP8",
    "HID_KEY_KP9", "HID_KEY_KP0", "HID_KEY_KPDOT",
]

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
        (27, 1.5, 1.0),
        (28, 1.5, 1.0),
        (29, 1.5, 1.0),
        (30, 1.5, 1.0),
        (31, 1.0, 1.0),
    ]),
]

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

RIGHT_THUMB_SPECS = [rotate_key(key, 0, 300, 0) for key in [
    KeyPlacement(32, 66, 300, 1.0, 1.0, 0),
    KeyPlacement(33, 0, 300, 1.0, 1.0, 0),
    KeyPlacement(34, 0, 366, 1.0, 1.0, 0),
    KeyPlacement(35, 0, 432, 1.0, 1.0, 0),
    KeyPlacement(36, 66, 374, 1.0, 2.0, 0),
    KeyPlacement(37, 132, 374, 1.0, 2.0, 0),
]]


def make_right_placements() -> list[KeyPlacement]:
    placements: list[KeyPlacement] = []

    for x, y, keys in RIGHT_COLUMN_SPECS:
        current_y = y
        for key_index, width, height in keys:
            placements.append(KeyPlacement(key_index, x, current_y, width, height))
            current_y += height * KEY_UNIT + KEY_GAP

    placements.extend(RIGHT_THUMB_SPECS)

    keys = sorted(placement.key_index for placement in placements)
    expected = list(range(KEY_COUNT))
    if keys != expected:
        raise ValueError(
            f"Right-hand placement must contain each key index 0..{KEY_COUNT - 1} once; got {keys}"
        )

    return placements


KEY_PLACEMENTS_RIGHT = make_right_placements()


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
        "hand": "right",
        "layers": {
            layer: [empty_action(layer) for _ in range(KEY_COUNT)]
            for layer in LAYERS
        },
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
    layout["hand"] = data.get("hand", layout["hand"])
    if layout["hand"] not in HANDS:
        layout["hand"] = "right"

    for layer in LAYERS:
        source = data.get("layers", {}).get(layer, [])
        for index, action in enumerate(source[:KEY_COUNT]):
            layout["layers"][layer][index] = normalize_action(action, layer)

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
    elif action_type in {"momentary_layer", "toggle_layer", "default_layer"}:
        normalized["layer"] = action.get("layer", "NAV")

    return normalized


def action_label(action: dict[str, Any]) -> str:
    action_type = action.get("type")
    if action_type == "transparent":
        return "TRNS"
    if action_type == "none":
        return "NONE"
    if action_type == "key":
        key = action.get("keycode", "")
        mods = action.get("modifiers", [])
        if mods:
            return "+".join([short_modifier(mod) for mod in mods] + [short_key(key)])
        return short_key(key)
    if action_type == "momentary_layer":
        return f"MO({action.get('layer', '')})"
    if action_type == "toggle_layer":
        return f"TG({action.get('layer', '')})"
    if action_type == "default_layer":
        return f"DF({action.get('layer', '')})"
    return ""


def short_key(key: str) -> str:
    return key.removeprefix("HID_KEY_")


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


def key_placements(hand: str) -> list[KeyPlacement]:
    if hand == "left":
        return [
            KeyPlacement(
                placement.key_index,
                BOARD_WIDTH - placement.x - placement.w * KEY_UNIT,
                placement.y,
                placement.w,
                placement.h,
                -placement.rotation,
            )
            for placement in KEY_PLACEMENTS_RIGHT
        ]

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
    return "KB_NONE"


def export_c(layout: dict[str, Any]) -> str:
    c_name = re.sub(r"[^a-zA-Z0-9_]", "_", layout["name"]).lower()
    lines = [
        "#include \"keyboard_layout.h\"",
        "",
        f"/* Generated layout: {layout['name']} ({layout.get('hand', 'right')} half, {KEY_COUNT} keys). */",
        f"static const struct keyboard_action {c_name}_layers[KEYBOARD_LAYER_COUNT][KEYBOARD_KEY_COUNT] = {{",
    ]

    for layer in LAYERS:
        lines.append(f"    [{layer_enum(layer)}] = {{")
        for index, action in enumerate(layout["layers"][layer]):
            if action.get("type") == "transparent" and layer != "BASE":
                continue
            if action.get("type") == "none" and layer == "BASE":
                continue
            lines.append(f"        [{index}] = {action_to_c(action)},")
        lines.append("    },")

    lines.extend([
        "};",
        "",
        f"static const struct keyboard_layout {c_name}_layout = {{",
        f"    .name = \"{layout['name']}\",",
        "    .layer_count = KEYBOARD_LAYER_COUNT,",
        f"    .layers = {c_name}_layers,",
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
        self.key_infos = parse_key_infos(repo_root)
        self.current_layer = LAYERS[0]
        self.current_key = 0
        self.buttons: list[KeyButton] = []

        self.setWindowTitle("Ergodox Custom Keyboard Configurator")
        self.resize(1180, 760)
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
        self.hand_selector.setCurrentIndex(HANDS.index(self.layout["hand"]))
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

        self.refresh_buttons()

    def build_editor(self) -> QtWidgets.QWidget:
        panel = QtWidgets.QWidget()
        form = QtWidgets.QFormLayout(panel)

        self.layout_name = QtWidgets.QLineEdit(self.layout["name"])
        self.layout_name.textChanged.connect(self.on_layout_name_changed)
        form.addRow("Layout", self.layout_name)

        self.selected_label = QtWidgets.QLabel()
        form.addRow("Selected", self.selected_label)

        self.action_type = QtWidgets.QComboBox()
        self.action_type.addItems(ACTION_TYPES)
        self.action_type.currentTextChanged.connect(self.on_action_changed)
        form.addRow("Action", self.action_type)

        self.keycode = QtWidgets.QComboBox()
        self.keycode.setEditable(True)
        self.keycode.addItems(HID_KEYS)
        self.keycode.currentTextChanged.connect(self.on_action_changed)
        form.addRow("Key", self.keycode)

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
        self.board_box.setTitle(f"{self.layout['hand'].title()} half, {KEY_COUNT} keys")

        for placement in key_placements(self.layout["hand"]):
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

    def current_action(self) -> dict[str, Any]:
        return self.layout["layers"][self.current_layer][self.current_key]

    def set_current_action(self, action: dict[str, Any]) -> None:
        self.layout["layers"][self.current_layer][self.current_key] = normalize_action(
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
        self.load_action_to_editor(self.current_action())

    def on_layer_changed(self, index: int) -> None:
        self.current_layer = LAYERS[index]
        self.refresh_buttons()
        self.select_key(self.current_key)

    def on_hand_changed(self, index: int) -> None:
        self.layout["hand"] = HANDS[index]
        self.render_keyboard()
        self.select_key(min(self.current_key, KEY_COUNT - 1))
        self.refresh_preview()

    def load_action_to_editor(self, action: dict[str, Any]) -> None:
        self.block_editor_signals(True)
        self.action_type.setCurrentText(action.get("type", empty_action(self.current_layer)["type"]))
        self.keycode.setCurrentText(action.get("keycode", "HID_KEY_A"))
        self.target_layer.setCurrentText(action.get("layer", "NAV"))
        modifiers = set(action.get("modifiers", []))
        for checkbox in self.modifier_checks:
            checkbox.setChecked(checkbox.property("modifier_value") in modifiers)
        self.block_editor_signals(False)
        self.update_editor_visibility()
        self.refresh_preview()

    def block_editor_signals(self, block: bool) -> None:
        widgets = [self.action_type, self.keycode, self.target_layer, *self.modifier_checks]
        for widget in widgets:
            widget.blockSignals(block)

    def on_action_changed(self, *_args: Any) -> None:
        action_type = self.action_type.currentText()
        action: dict[str, Any] = {"type": action_type}
        if action_type == "key":
            action["keycode"] = self.keycode.currentText()
            action["modifiers"] = [
                checkbox.property("modifier_value")
                for checkbox in self.modifier_checks
                if checkbox.isChecked()
            ]
        elif action_type in {"momentary_layer", "toggle_layer", "default_layer"}:
            action["layer"] = self.target_layer.currentText()
        self.set_current_action(action)
        self.update_editor_visibility()

    def update_editor_visibility(self) -> None:
        action_type = self.action_type.currentText()
        key_visible = action_type == "key"
        layer_visible = action_type in {"momentary_layer", "toggle_layer", "default_layer"}
        self.keycode.setEnabled(key_visible)
        for checkbox in self.modifier_checks:
            checkbox.setEnabled(key_visible)
        self.target_layer.setEnabled(layer_visible)

    def refresh_buttons(self) -> None:
        for button in self.buttons:
            action = self.layout["layers"][self.current_layer][button.info.index]
            button.setText(
                f"{button.info.label}\n{action_label(action)}\n{short_gpio(button.info.gpio)}"
            )

    def refresh_preview(self) -> None:
        self.c_preview.setPlainText(export_c(self.layout))

    def on_layout_name_changed(self, text: str) -> None:
        self.layout["name"] = text.strip() or "custom"
        self.refresh_preview()

    def copy_base_to_current_layer(self) -> None:
        if self.current_layer == "BASE":
            return
        self.layout["layers"][self.current_layer] = [
            dict(action) for action in self.layout["layers"]["BASE"]
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
        self.hand_selector.setCurrentIndex(HANDS.index(self.layout["hand"]))
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
        self.hand_selector.setCurrentIndex(HANDS.index(self.layout["hand"]))
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
        QMainWindow { background: #f6f7f9; }
        QGroupBox {
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
        }
        QPushButton:checked {
            border: 2px solid #2d6cdf;
            background: #dbe8ff;
        }
    """)
    window = Configurator(repo_root, args.layout)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
