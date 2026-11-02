# Human-readable layout files

Use **Download JSON** in the web configurator to export the displayed layout.
**Upload JSON** validates a selected file and previews it without writing to the
keyboard. Review its halves and layers, then choose **Save to keyboard**.
Saving affects only the USB-connected half's NVM and takes effect after reboot.
The slave keeps forwarding raw key events; it does not need this file.

If saving fails or the response is lost, retrying the same file is safe. A save
that was already committed is not rewritten by NVS when the bytes are unchanged.
Use **Discard preview** to return to the current running layout. After a save,
**Show current layout** returns to the still-active layout; it does not undo the
saved record. Download always exports the layout currently displayed, including
an import preview.

## JSON structure, version 1

The `display_layout` identifier is saved with the configuration and restored
from keyboard NVM on any computer. The default for JSON without this field is
`qwerty`. Available identifiers come from the pinned repository's catalog: they
are lowercase JSON filename stems, such as `fr_be` or `xkb_be__oss`. Changing the
selection creates a preview that must be saved to the keyboard. Locale labels
and compositions are downloaded from
[gennartan/layouts](https://github.com/gennartan/layouts) and cached in the
browser. Only the identifier is stored on the keyboard.

The browser uses native JSON parsing and named action values. The firmware does
not parse JSON: the browser converts the validated file to the existing binary
record, and the firmware independently validates its bounds, version, CRC, and
actions before saving it. The browser writes version-5 NVM records, with a
fixed macro bank followed by a 128-byte locale identifier. New firmware reads
versions 1–5; earlier firmware cannot read version 5. Legacy locale metadata in
versions 2/3 is retained; versions 1/4 use `qwerty`.
The old offline Python configurator JSON is a different format and is not
implicitly migrated.

| Root field | Value |
| --- | --- |
| `format` | `"ergodox-layout"` |
| `version` | `1` |
| `name` | At most 31 UTF-8 bytes, without NUL characters |
| `display_layout` | Optional locale identifier, 1–127 lowercase ASCII letters/digits/underscores/hyphens, starting with a letter or digit; default `qwerty` |
| `default_layer` | Zero-based index of an existing layer |
| `layers` | An array of 1–8 layer objects |
| `macros` | Optional array of exactly eight macro slots, each an array of 0–16 steps |

Each layer contains `left` and `right` arrays, each with exactly 38 action
objects. Every action has `position`, numbered **1–38** in array order to match
the physical key numbers in the viewer, and `type`. Layer indices are **0–7**.
Unknown fields, missing keys, duplicate modifiers/buttons, unsupported versions,
and invalid values are rejected. Files are limited to 256 KiB, sufficient for
a full eight-layer export with every modifier assigned.

An excerpt from a hand's key array looks like this (a complete hand has 38
entries):

```json
[
  {"position": 1, "type": "key", "key": "A", "modifiers": ["LEFT_CTRL"]},
  {"position": 2, "type": "layer_momentary", "layer": 1},
  {"position": 3, "type": "consumer", "usage": "VOLUME_UP"},
  {"position": 4, "type": "transparent"}
]
```

## Actions

| Type | Additional fields |
| --- | --- |
| `transparent` | None: use a lower active layer |
| `none` | None: no action |
| `key` | `key`, optional `modifiers` array |
| `consumer` | `usage` |
| `system` | `usage`: `NONE`, `POWER`, `SLEEP`, `WAKE` |
| `mouse_button` | `buttons` array: `LEFT`, `RIGHT`, `MIDDLE`, `BACK`, `FORWARD` |
| `mouse_move` | Integer `x` and `y`, each −127 through 127 |
| `mouse_wheel` | Integer `delta`, −127 through 127 |
| `lighting` | `command`: `TOGGLE`, `ON`, `OFF`, `BRIGHTNESS_UP`, `BRIGHTNESS_DOWN` |
| `layer_momentary` | `layer`: an existing zero-based layer index |
| `layer_toggle` | `layer`: an existing zero-based layer index |
| `layer_default` | `layer`: an existing zero-based layer index |
| `macro` | `macro`: a zero-based index (0–7) of a nonempty macro slot |

Keyboard key names include `A`–`Z`, `0`–`9`, `F1`–`F12`, `ENTER`, `ESCAPE`,
`BACKSPACE`, `TAB`, `SPACE`, `PAGE_UP`, `PAGE_DOWN`, `LEFT`, `RIGHT`, `UP`, `DOWN`,
`LEFT_BRACKET`, `RIGHT_BRACKET`, and keypad names such as `KP_0` and `KP_ENTER`.
Download the current layout to see the names for its existing assignments.
A numeric HID key usage from 0 to 101 is also accepted. `NONE` (0) with modifiers
creates a modifier-only key.

Modifiers are `LEFT_CTRL`, `LEFT_SHIFT`, `LEFT_ALT`, `LEFT_GUI`, `RIGHT_CTRL`,
`RIGHT_SHIFT`, `RIGHT_ALT`, and `RIGHT_GUI`.

Consumer usage names include `VOLUME_UP`, `VOLUME_DOWN`, `MUTE`, `PLAY_PAUSE`,
`NEXT_TRACK`, `PREVIOUS_TRACK`, `STOP`, `RECORD`, `FAST_FORWARD`, `REWIND`, `EJECT`,
`BRIGHTNESS_UP`, `BRIGHTNESS_DOWN`, `MAIL`, `CALCULATOR`, `SEARCH`, `BROWSER_HOME`,
`BROWSER_BACK`, `BROWSER_FORWARD`, `BROWSER_STOP`, `BROWSER_REFRESH`, `BOOKMARKS`,
and `NONE`. Numeric usages 0–1023 are accepted for other supported consumer
controls. Exports prefer names whenever one is known.

## Device upload endpoint

`POST /api/layout/import` accepts only `application/octet-stream`, containing one
complete binary record in versions 1–5. The JSON conversion happens in the browser.
The resource accumulates at most 3,168 bytes across request fragments. An aborted,
oversized, invalid, or wrong-content-type request does not save a layout.
Only a fully received, valid record is submitted to NVS in configuration mode.
The endpoint does not provide CORS access.

A successful response is:

```json
{"saved": true, "reboot_required": true}
```

The active RAM layout and `GET /api/layout` remain unchanged until reboot. The
browser pauses live key polling while saving to avoid competing HTTP requests
during flash programming. The NVS save/import tests verify deferred activation,
invalid-record rejection, and retry behavior. Physical flash interruptions still
need hardware testing.

## Macros

A macro slot is an array of taps. Each step contains a `key` (the same HID names
as ordinary keys), optional `modifiers` (default `[]`), and optional `delay_ms`
(default `0`, integer 0–5000). Each chord is pressed and released before the
delay and next step. Slots are shared by every action that references them.

For example, a root-level bank containing two key taps is:

```json
"macros": [
  [
    {"key": "A", "modifiers": ["LEFT_SHIFT"], "delay_ms": 100},
    {"key": "B", "modifiers": [], "delay_ms": 0}
  ],
  [], [], [], [], [], [], []
]
```

Assign it with `{"position": 1, "type": "macro", "macro": 0}`. A zero key
(`"NONE"`) without modifiers can represent a pause. Empty slots cannot be
assigned. Macro actions run once per press; overlapping macro triggers are
ignored, and physically held keyboard keys/modifiers are restored afterward.
The host's keyboard language determines the resulting characters.
