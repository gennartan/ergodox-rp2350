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

From the project directory, build the firmware:

```sh
west build -b rp2350b_core/rp2350b/m33
```

The board is also set in `CMakeLists.txt`, so a plain build can work once the
Zephyr workspace is configured:

```sh
west build
```

Select the hand and layout in `prj.conf` or through Zephyr configuration before
building:

```conf
CONFIG_ERGODOX_KEYBOARD_HAND_LEFT=y
CONFIG_ERGODOX_KEYBOARD_HAND_RIGHT=n
CONFIG_ERGODOX_KEYBOARD_LAYOUT_CUSTOM=y
```

Flash the generated firmware using your RP flash hardware tool. The exact
command depends on the probe and runner you use, but the usual Zephyr entry
point is:

```sh
west flash --openocd /usr/local/bin/openocd
```

For a split keyboard, build and flash each side with the matching hand
configuration. The USB-connected half will enumerate as the keyboard; the other
half will fall back to UART and forward its key events.

## 4. How To Create A Custom Design

Start with the layout. The firmware supports multiple hands and layers through
`src/keyboard_layout.h`. You can edit the C layout files directly, or use the
layout JSON files and `scripts/keyboard_configurator.py` to generate a custom
layout.

Useful places to change behavior:

- `layouts/custom.json`: custom layout source data;
- `src/custom_layout.c`: generated/custom C layout;
- `Kconfig`: available layout choices;
- `prj.conf`: selected hand and selected layout;
- `app.overlay`: GPIO mapping, LED pins, USB HID device, and split UART pins.

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
