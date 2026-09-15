# LHWIT family

The reference CRUMBS family: four Arduino Nano peripherals with trivial
hardware and two Linux controllers that drive them. Each device has its own
README with its opcodes and pins; this page is the bus, the build and the
controllers. The family's vocabulary lives in the four `*_ops.h` headers here
plus `lhwit_ops.h`, which includes them all and adds the version-check helpers.

| Device | `type_id` | Address | Project | Hardware | README |
| --- | --- | --- | --- | --- | --- |
| LED array | `0x01` | `0x20` | `led/` | 4 LEDs on D4–D7 | [led/README.md](led/README.md) |
| Servo | `0x02` | `0x30` | `servo/` | 2 servos on D9, D10 | [servo/README.md](servo/README.md) |
| Calculator | `0x03` | `0x10` | `calculator/` | none | [calculator/README.md](calculator/README.md) |
| Display | `0x04` | `0x40` | `display/` | 5641AS 4-digit 7-segment | [display/README.md](display/README.md) |

All four report module version 1.0.0 to opcode `0x00`. Addresses are
`PERIPHERAL_ADDR` in each `src/main.cpp`.

## Bus

Every Nano joins the same I²C bus: A4 (SDA), A5 (SCL), common ground. The
controller is a Raspberry Pi or any Linux host with `i2c-dev`; the Pi's
onboard pull-ups are enough, and the bus runs at 3.3 V — do not add pull-ups
to 5 V. Servos need their own 5 V supply sharing ground with the Nano; the
Nano's 5 V pin cannot source them.

## Build and flash the peripherals

Each device is a PlatformIO project with envs `nanoatmega328new` (default),
`nanoatmega328old` (57600-baud bootloader) and `esp32dev`:

```sh
pio run -d examples/families_usage/lhwit_family/led -e nanoatmega328new -t upload
```

The projects depend on the published library (`lib_deps =
cameronbrooks11/CRUMBS@^0.14.0`),
set `-DCRUMBS_MAX_HANDLERS=8` (6 for the display) and add `-I ..` so the ops
headers resolve. Each peripheral prints a banner and a ready line on its
serial port at 115200 baud; after that the LED array is silent, the
calculator speaks only on divide-by-zero, and the servo and display echo
every command.

## Controllers

Both Linux controllers are built by the root CMake with the Linux HAL
([platform-setup.md](../../../docs/platform-setup.md#linux)):

```sh
build-linux/crumbs_controller_discovery [/dev/i2c-1]    # scans, version-checks, binds what it finds
build-linux/crumbs_controller_manual    [/dev/i2c-1]    # uses the fixed list in controller_manual/config.h
```

`controller_discovery` starts empty: `scan` sweeps `0x08`–`0x77` with the
non-strict CRUMBS probe, queries each device's version, prints
`OK Compatible` or why not, and binds only the compatible ones.
`controller_manual` binds the table in `config.h` (the four defaults above)
with no scan and no version check. Their READMEs show each one's output.

### Shell

Prompt `lhwit> `. `help`, `list`, `quit` (or `exit`). Every device command names its
target, by index among devices of that type or by address:

```text
<type> <idx> <cmd> [args]        e.g.  led 0 set_all 0x0F
<type> @<addr> <cmd> [args]      e.g.  led @0x20 set_all 0x0F
```

`@addr` accepts `0x20`, `32` or `040` and does not check the device's type;
numeric arguments are decimal except `led set_all <mask>`, which also takes
hex.

| Type | Command | Arguments |
| --- | --- | --- |
| `calculator` | `add` `sub` `mul` `div` | `<a> <b>` (unsigned 32-bit) |
| | `result` | — |
| | `history` | — |
| `led` | `set_all` | `<mask>` (bits 0–3) |
| | `set_one` | `<idx 0-3> <state 0/1>` |
| | `blink` | `<idx> <enable 0/1> <period_ms>` |
| | `get_state` `get_blink` | — |
| `servo` | `set_pos` | `<idx 0-1> <angle 0-180>` |
| | `set_speed` | `<idx> <speed 0-20>` |
| | `sweep` | `<idx> <enable 0/1> <min> <max> <step>` |
| | `get_pos` `get_speed` | — |
| `display` | `set_number` | `<number 0-9999> <decimal_pos 0-4>` |
| | `set_brightness` | `<level 0-10>` (stored only; the display has no brightness control) |
| | `clear` `get_value` | — |

There is no controller command for the display's `SET_SEGMENTS` opcode;
`display_send_set_segments()` in `display_ops.h` is the only way to send it.

## Checking the whole family

```text
lhwit> scan
Scanning I2C bus for CRUMBS devices (0x08-0x77)...

Found 4 device(s):
--------------------------------------------
[0x10] Calculator
       CRUMBS: v0.14.0 (controller: v0.14.0)
       Module: v1.0.0 (expected: v1.0.x)
       OK Compatible
...
--------------------------------------------
Usable: 4/4 devices

lhwit> calculator 0 add 40 2
OK: add(40, 2) sent. Use 'calculator result' to get answer.
lhwit> calculator 0 result
Result: 42
lhwit> led 0 set_all 0x0F
OK: LEDs set to 0x0F
lhwit> servo 0 set_pos 0 45
OK: Servo 0 position set to 45deg
lhwit> display 0 set_number 1234 2
OK: Display showing 1234 (decimal pos 2)
```

(`controller_discovery` output; the `...` elides the other three devices'
identical blocks. `controller_manual` adds `to 0x10` / `at 0x20` to the
calculator, LED and servo confirmations and query lines; its display lines
and `Result:` / `History:` are the same as above.)

## Adopting the family

Copy the four headers, keep their type IDs, and bind a `crumbs_device_t` per
device the way `controller_manual/main.c` does. Each header declares its
type and opcodes with `CRUMBS_DEFINE_FAMILY`, each payload layout once with
`CRUMBS_DEFINE_PAYLOAD`, and its wrappers with the `crumbs_ops.h` macros; the
peripherals unpack and pack through the same generated codec, and
`tests/test_lhwit_roundtrip.c` sends every operation through it in both
directions. Two layouts the codec cannot express — the LED blink table and
the calculator's history entry — have hand-written `pack`/`unpack` pairs in
the same shape. Every getter reads with `crumbs_controller_read_expect()`,
so a reply of the wrong type or opcode returns `CRUMBS_RX_REPLY_MISMATCH`. Each
peripheral declares its type with `crumbs_set_type_id()`, so a frame
addressed to another family's type is dropped before any handler runs — the
controllers' `@addr` selector still does not check types, but the device
itself now does.
