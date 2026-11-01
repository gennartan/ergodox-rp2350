# ErgoDox Custom Keyboard

## 1. Introduction

This project is a custom split ErgoDox-style keyboard built around the RP2350
and Zephyr RTOS.

The goal is not only to build a working keyboard. It is also a learning project
for KiCad, PCB design, electronic development, Zephyr, USB HID, and split
keyboard firmware. The RP2350 is clearly more powerful than a keyboard needs,
but that is part of the fun: it gives enough room to experiment without being
limited by the controller.

The firmware is designed to keep the time between a physical key press and the
USB HID report as small as reasonably possible. Keys are scanned through direct
GPIO interrupts with debounce handling, then converted into USB HID keyboard
reports by the active half.

The keyboard is split into two hands. One half is connected to the computer over
USB and acts as the HID keyboard. The other half sends its key events over UART
to the USB-connected half.

![Assembled split keyboard](docs/images/full_keyboard.jpeg)

## 2. Project Structure

The repository contains the mechanical, electronic, and firmware parts of the
keyboard.

### Mechanical files

`hardware/mechanical` contains the STL files for the printed case parts. The
current files are for the right hand; the left-hand mechanical design is made by
mirroring the right-hand design.

### Electronic files

`hardware/electronic` contains the KiCad projects for the PCBs:

- `LeftHand.kicad_pro`: left-hand PCB project;
- `RightHand.kicad_pro`: right-hand PCB project;
- `ErgoDoxCustom.kicad_pro`: shared/root KiCad project files;
- `production/v1`: generated fabrication and assembly files.

The production files were prepared for JLCPCB assembly, and the BOM contains
JLCPCB/LCSC component references. More hardware-specific notes are documented in
`hardware/electronic/README.md`.

![Left-hand PCB](docs/images/left_hand.jpeg)

![Left-hand PCB in KiCad 3D viewer](docs/images/left_hand_kicad_3d_view.png)

### Zephyr firmware

The root of the repository is a Zephyr application for the RP2350:

- `CMakeLists.txt`: Zephyr application definition and target board;
- `prj.conf`: main firmware configuration;
- `Kconfig`: keyboard hand, layout, and split UART options;
- `app.overlay`: GPIO, LED, USB HID, and UART device tree configuration;
- `src/main.c`: key scanning, USB HID, split transport selection, and LED state;
- `src/keyboard_runtime.c`: layer and HID report runtime;
- `src/keyboard_layouts.c` and `src/custom_layout.c`: keyboard layouts;
- `src/split_transport_uart.c`: UART transport between the two halves;
- `layouts`: JSON layout descriptions;
- `scripts/keyboard_configurator.py`: helper UI for generating custom layouts.

## 3. How To Get Started

Install Zephyr and make sure `west` is available in your shell. This project is
built as a Zephyr application, so it expects the usual Zephyr SDK and Python
environment to be installed first.

From the project directory, build both keyboard halves with sysbuild:

```sh
west build --sysbuild -b rp2350b_core/rp2350b/m33
```

The board is also set in `CMakeLists.txt`, so the board argument can be omitted
once the Zephyr workspace is configured:

```sh
west build --sysbuild
```

Sysbuild compiles the application twice and selects the hand independently for
each image. The resulting UF2 files are:

```text
build/ergodox_left/zephyr/ergodox_left.uf2
build/ergodox_right/zephyr/ergodox_right.uf2
```

Flash the generated firmware using your RP flash hardware tool. The exact
command depends on the probe and runner you use, but the usual Zephyr entry
point is:

```sh
west flash --domain ergodox_left --openocd /usr/local/bin/openocd
```

Flash the right half by selecting its sysbuild domain:

```sh
west flash --domain ergodox_right --openocd /usr/local/bin/openocd
```

The USB-connected half will enumerate as the keyboard; the other half will fall
back to UART and forward its key events.

## 4. How To Create A Custom Design

Start with the layout. The firmware supports multiple hands and layers through
`src/keyboard_layout.h`. You can edit the C layout files directly, or use the
layout JSON files and `scripts/keyboard_configurator.py` to generate a custom
layout.

![Keyboard configurator](docs/images/keyboard_configurator.png)

Useful places to change behavior:

- `layouts/custom.json`: custom layout source data;
- `src/custom_layout.c`: generated/custom C layout;
- `Kconfig`: available layout choices;
- `prj.conf`: selected hand and selected layout;
- `app.overlay`: GPIO mapping, LED pins, USB HID device, and split UART pins.

The configurator separates localized keyboard keys from special actions. Select
one of the dedicated action types for:

- consumer controls such as volume, playback, display brightness, browser,
  mail, and calculator;
- system power, sleep, and wake controls;
- mouse buttons, cursor movement, and vertical scrolling;
- keyboard lighting toggle, on/off, and brightness controls.

The generated layout uses `KB_CONSUMER`, `KB_SYSTEM`, `KB_MOUSE_*`, and
`KB_LIGHTING` macros. Keyboard, consumer, system, and mouse reports share one
HID interface and use report IDs to distinguish them. In boot protocol, only the standard eight-byte
keyboard report is sent, without a report ID. The current board drives its four
indicator LEDs as binary GPIOs, so lighting brightness up/down maps to enabled/disabled rather
than PWM dimming.

For example, exported volume keys now look like:

```c
KB_CONSUMER(HID_CONSUMER_VOLUME_INCREMENT)
KB_CONSUMER(HID_CONSUMER_VOLUME_DECREMENT)
```

For electronic changes, open the KiCad projects in `hardware/electronic`. The
left and right hands are separate projects, so changes that affect both halves
should be applied and checked on both PCBs.

For mechanical changes, start from `hardware/mechanical`. The current case files
are based on a modified right-hand design; mirror or adapt them for the left
hand as needed.

Before producing a new hardware revision:

- update the KiCad schematic and PCB;
- run electrical rules and design rules checks;
- regenerate the BOM, position files, designator files, netlist, and fabrication
  ZIP;
- store the generated files under a new production revision folder;
- update the hardware README with any known fixes or ordering notes.

## 5. How To Contribute

Contributions are welcome, especially in areas that make the keyboard easier to
build, flash, customize, or manufacture.

Good contribution topics include:

- firmware fixes and latency improvements;
- layout improvements;
- Zephyr configuration cleanup;
- KiCad schematic or PCB fixes;
- mechanical case improvements;
- clearer build, flashing, and production documentation.

When contributing, avoid committing local generated noise such as `build/`,
Python `__pycache__` folders, KiCad lock files, or editor-specific files.
Production files are useful to commit when they represent a real hardware
revision that was ordered or is ready to order.
