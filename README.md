# ErgoDox Custom Keyboard

This is a Zephyr-based custom keyboard project for the Raspberry Pi Pico 2 (RP2350) board.

## Features

- 38 direct GPIO keys
- USB HID keyboard interface
- GPIO scanning with debouncing
- Configurable keymap

## Hardware Setup

Connect your keyboard switches directly to the board GPIO pins:

- Key 1: GPIO 1
- Key 2: GPIO 2
- Key 3: GPIO 3
- ...
- Key 39: GPIO 39

Each switch should be wired so that pressing it pulls its GPIO input low.

## Keymap

The current keymap is:

```
Q W E R T Y
A S D F G H
Z X C V B N
ESC TAB SPACE ENTER BKSP LEFT
1 2 3 4 5 6
```

## Building and Flashing

1. Set up Zephyr development environment
2. Navigate to the project directory
3. Build: `west build`
4. Flash: `west flash`

## Customization

- Modify the `keymap` array in `src/main.c` to change key assignments
- Adjust matrix dimensions by changing `ROWS` and `COLS` defines
- Update GPIO assignments in `app.overlay` for different pinouts

## Notes

- This is a basic implementation that supports only one key press at a time (6KRO limitation of simple HID report)
- For full NKRO support, a more complex HID report structure would be needed
- Debouncing is implemented with a simple timer - you may want to adjust timing based on your switches
