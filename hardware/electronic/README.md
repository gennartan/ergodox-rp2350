# Electronic Hardware

This folder contains the KiCad projects and production outputs for the
electronic part of the ErgoDox Custom split keyboard.

The RP2350 is clearly overkill for a keyboard controller. It was chosen
intentionally as part of the project goal: learning more about KiCad, PCB
design, and electronic development in general, rather than optimizing for the
simplest possible keyboard hardware.

The keyboard is built as two independent halves:

- `LeftHand`: PCB and schematic for the left half.
- `RightHand`: PCB and schematic for the right half.
- `ErgoDoxCustom`: shared/root KiCad project files used by the design.
- `production/v1`: generated fabrication outputs for the first hardware
  revision, split by hand.

Each hand has the same general role: scan its local switches, drive its local
indicator LEDs, and expose the split communication connector. The firmware
selects whether the build represents the left or right hand, then labels key
events before sending them to the keyboard runtime.

## Split Structure

The keyboard uses one USB-connected half and one secondary half. Since each half
has USB capabilities, each hand can act as the master (and this can optimize
cable management).

The USB-connected half enumerates as the USB HID keyboard. It handles its own
key events locally and receives key events from the other half over UART.

The secondary half scans its local keys and forwards raw key events over the
split UART link. It does not need to enumerate as a USB keyboard during normal
operation.

Both halves therefore need:

- local key switch inputs;
- local LED drive circuitry;
- power rails for the controller and peripherals;
- UART TX/RX wiring between the two halves;
- a common ground between the two halves.

## Left Hand

The left-hand hardware lives in:

- `LeftHand.kicad_pro`
- `LeftHand.kicad_sch`
- `LeftHand.kicad_pcb`
- `production/v1/LeftHand`

The left hand is one half of the split keyboard and should expose the same
functional blocks as the right hand: key inputs, LEDs, controller power, USB,
and UART split communication.

## Right Hand

The right-hand hardware lives in:

- `RightHand.kicad_pro`
- `RightHand.kicad_sch`
- `RightHand.kicad_pcb`
- `production/v1/RightHand`

The right hand is the matching half of the split keyboard. In the current
firmware layout it can also act as the USB-connected half, receiving split
events from the other side and sending the final HID report to the host.

## Known Fixes Needed

The current hardware revision still needs these corrections before the next
production release:

- Left hand: change the USB/UART power reference from `3V3` to `VBUS` (`5V`).
- Right hand: replace the NPN transistor used for the LEDs with a MOSFET.

## Production Notes

The v1 production files were prepared for JLCPCB assembly. The component
references in the BOM are therefore aligned with JLCPCB/LCSC part references,
which makes the files easier to reuse or audit when preparing another order.

As a rough reference point, producing five assembled left-hand PCBs was
estimated at about 90 EUR for this revision (5 parts ordered) excluding
the cherry-mx switch to buy separately.

Before ordering boards, regenerate and review the production files for both
hands:

- BOM CSV;
- component positions CSV;
- designators CSV;
- IPC netlist;
- fabrication ZIP.

Keep the production output grouped by hardware revision so that each ordered
PCB set can be traced back to the exact KiCad state used to generate it.
