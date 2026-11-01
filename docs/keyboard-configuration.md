# Keyboard configuration formats

The USB-connected half is the master. It owns a configuration for both hands,
resolves actions, and produces HID reports. The slave sends raw hand/key-index
press and release events over UART; it does not load or interpret a layout.
There is no configuration synchronization between halves. If the USB cable is
moved to the other half, that half uses its own saved configuration or factory
fallback.

## RAM

`struct keyboard_config` owns a name, an active layer count (1–8), a default
layer, and a fixed `actions[2][8][38]` table. `struct keyboard_layout` is a
read-only view into that RAM. The table uses 2,432 bytes, and the complete owned
configuration uses 2,466 bytes on the target.

Each naturally aligned action uses four bytes:

| Field | Size | Meaning |
| --- | --- | --- |
| type | 1 byte | Explicit action ID, 0–11 |
| modifier | 1 byte | Keyboard modifier mask; zero for other action types |
| payload | 2 bytes | Keycode, usage, mouse deltas, or target layer |

The master loads and validates once before scanning keys or accepting slave
events. A press checks active layers from highest to lowest, with at most eight
direct table lookups. The selected four-byte action is cached until release, so
releasing a layer key before another key does not change that key's release
behavior. HID aggregation retains the existing fixed scans over 76 key slots.
There is no allocation, decoding, CRC calculation, NVM access, or configuration
store mutex in the key-event path. Hardware latency has not been benchmarked.

The four named factory layers remain defaults; the runtime accepts all eight
layer slots. The C layout files provide a factory fallback rather than the live
runtime table. The offline configurator's C export remains usable for generating
factory defaults.

## NVM binary record, version 1

This is an explicit byte encoding, not a dump of a C structure. All multibyte
integers are little-endian. Signed mouse deltas use two's-complement bytes.
No migration is attempted: an unsupported version is rejected.

| Header offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | ASCII magic `EKCF` |
| 4 | 2 | Format version, currently 1 |
| 6 | 2 | Header size, 64 |
| 8 | 4 | Total record length, including header |
| 12 | 1 | Active layer count, 1–8 |
| 13 | 1 | Default layer index, less than layer count |
| 14 | 1 | Hand count, exactly 2 |
| 15 | 1 | Keys per hand, exactly 38 |
| 16 | 32 | NUL-terminated name, at most 31 bytes |
| 48 | 12 | Reserved, zero |
| 60 | 4 | IEEE CRC-32 of the entire record, treating this field as zero |
| 64 | variable | Four-byte actions, ordered by hand, active layer, key index |

Hand 0 is left; hand 1 is right. Only active layers are serialized. An action is
`[type, modifier, payload0, payload1]`:

| Type | ID | Payload bytes |
| --- | --- | --- |
| Transparent | 0 | `0, 0` |
| None | 1 | `0, 0` |
| Keyboard | 2 | `keycode, 0`; modifiers are in byte 1 |
| Consumer | 3 | 16-bit usage, little-endian |
| System | 4 | 16-bit usage, little-endian |
| Mouse buttons | 5 | 16-bit button mask, little-endian |
| Mouse move | 6 | signed X, signed Y |
| Mouse wheel | 7 | `0`, signed wheel delta |
| Lighting | 8 | 16-bit lighting command, little-endian |
| Momentary layer | 9 | `0`, target layer |
| Toggle layer | 10 | `0`, target layer |
| Default layer | 11 | `0`, target layer |

Unused action fields must be zero. Validation checks action types, supported HID
usage ranges, mouse ranges, lighting commands, and layer references. The maximum
record size is **2,496 bytes**; four layers use **1,280 bytes**. Malformed data
never partially replaces the RAM configuration.

## Flash storage and boot behavior

The application image is limited to `0x000000..0x3fbfff`. A dedicated 16 KiB
`keyboard-config` partition occupies flash offsets `0x3fc000..0x3fffff`, within
the first 4 MiB. These are offsets in the flash device, not CPU addresses.
The board's other image/storage partitions remain untouched.

Zephyr NVS stores the whole binary configuration under record ID 1, across four
4 KiB sectors. The maximum blob fits in one sector with NVS metadata. NVS appends
a record and completes its metadata after writing its data; it also handles
sector recycling and skips unchanged writes. The application CRC covers both
configuration metadata and actions. This avoids a filesystem, per-key records,
and application-managed multi-record transactions.

On master boot or configuration-mode entry:

1. Copy and validate the compiled factory configuration into RAM.
2. Mount NVS and try to load/decode its configuration record.
3. Use the factory configuration if storage is absent, invalid, corrupt, or has
   an unsupported format. Log the reason through RTT.
4. In normal master mode, initialize the runtime with the validated RAM view
   before enabling split reception and GPIO scanning.

`keyboard_config_save()` validates and persists a complete candidate for the
next boot. It is permitted only in configuration mode. It does not replace the
live RAM table, and no HTTP write endpoint or editor is added in this change.
A save that is interrupted before NVS commits can leave the previous committed
record available; malformed committed records fall back to factory defaults.
Power interruption on physical RP2350 flash still needs hardware testing.

A regular UF2 image does not include the reserved configuration partition, so
reflashing can preserve saved data. A chip erase clears it. Firmware changes
that alter the binary meaning should bump the version and use the fallback;
there is no compatibility/migration layer.

## Tests

Run the codec/runtime and real NVS backend tests using Zephyr's native simulator:

```sh
docker compose run --rm build west build -b native_sim/native/64 \
  -d /workspace/build/config-tests /workspace/tests/config
docker compose run --rm build sh -c \
  'cd /workspace/build/config-tests && ./zephyr/zephyr.exe'
```

Tests cover round trips, every single-byte corruption, truncation, invalid
configuration values, all eight layers, cross-hand layer resolution, release
semantics, saves/reloads, repeated writes that force NVS sector recycling,
write rejection during normal keyboard mode, and corrupt-record fallback.
