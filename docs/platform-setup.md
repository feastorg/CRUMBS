# Platform Setup

How to get CRUMBS compiling and a first pair of devices talking on each
platform. Peripherals run on Arduino (AVR and ESP32 are the tested cores); a
controller runs on Arduino or on Linux over `i2c-dev`.

## Arduino IDE

CRUMBS is not in the Library Manager index. Install it as a folder:

```sh
git clone https://github.com/feastorg/CRUMBS.git ~/Arduino/libraries/CRUMBS
```

or **Sketch → Include Library → Add .ZIP Library…** with a release archive.
Restart the IDE; the sketches appear under **File → Examples → CRUMBS**.

Start with `hello_peripheral` on one board and `hello_controller` on another,
wired as in [Wiring](#wiring). Both default to address `0x10`
(`config.h` in each sketch); the controller's serial monitor at 115200 baud
shows the exchange.

With `arduino-cli`, the same compile CI runs:

```sh
arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/hello_peripheral
```

To change `CRUMBS_MAX_HANDLERS` for an IDE sketch you must rebuild the library
with the same value; a `#define` in the sketch is not enough (see
[architecture.md](architecture.md#handler-tables)). PlatformIO makes this
easy; the IDE does not.

## PlatformIO

Depend on the registry package:

```ini
[env:nanoatmega328new]
platform = atmelavr
board = nanoatmega328new
framework = arduino
lib_deps = cameronbrooks11/CRUMBS@^0.12.5
build_flags = -DCRUMBS_MAX_HANDLERS=8   ; optional; applies to library and sketch alike
```

The registry owner is `cameronbrooks11` (the publishing account), not the
GitHub organisation. To build against a working copy instead, point
`lib_deps` at it: `lib_deps = symlink:///path/to/CRUMBS`.

The shipped projects under `examples/*/platformio/` and
`examples/families_usage/lhwit_family/` build for `nanoatmega328new` and
`esp32dev`:

```sh
pio run -d examples/core_usage/platformio/simple_peripheral -e nanoatmega328new -t upload
pio run -d examples/core_usage/platformio/simple_controller -e nanoatmega328new -t upload
pio device monitor            # monitor_speed = 115200 is set in the project
```

Their `platformio.ini` pins the registry version, so they build the published
library, not the checkout they sit in.

## Linux

The Linux HAL is controller-only and needs
[linux-wire](https://github.com/feastorg/linux-wire) **0.1.3 or newer**.

### linux-wire

Build from source on any architecture (the only prebuilt tarball is x86_64):

```sh
git clone --branch v0.1.3 https://github.com/feastorg/linux-wire.git
cmake -S linux-wire --preset minimal
cmake --build --preset minimal
sudo cmake --install linux-wire/build/minimal          # to /usr/local
```

Use `--prefix <dir>` on the install to keep it local; a linux-wire *build*
tree is not a usable prefix (it exports nothing until installed).

### Build CRUMBS

```sh
cmake --preset linux                                   # add -DCMAKE_PREFIX_PATH=<prefix> if not /usr/local
cmake --build --preset linux
ctest --test-dir build-linux
```

The presets need CMake 3.21; the project itself needs 3.13, so on older CMake:

```sh
cmake -S . -B build-linux -DCRUMBS_ENABLE_LINUX_HAL=ON
cmake --build build-linux
```

Either way the six Linux example programs are in `build-linux/`. Without
`CRUMBS_ENABLE_LINUX_HAL=ON` (the `default` preset) the library and tests
build but no examples do.

### Use CRUMBS from your own CMake project

```sh
cmake --install build-linux --prefix ~/crumbs-install
```

```cmake
find_package(crumbs CONFIG REQUIRED)        # CMAKE_PREFIX_PATH=~/crumbs-install
target_link_libraries(app PRIVATE crumbs::crumbs)
```

Headers install to `include/crumbs/`; include them as `<crumbs.h>` and
`<crumbs_linux.h>`. The installed package pulls linux-wire in through
`find_dependency`, so the same prefix path (or a system install) must
contain it.

### The I²C bus

On a Raspberry Pi enable the interface (`raspi-config` → Interface Options →
I2C) and use `/dev/i2c-1`. Elsewhere load `i2c-dev` (`sudo modprobe i2c-dev`)
and find the bus with `i2cdetect -l`. Add your user to the `i2c` group rather
than running as root.

`i2cdetect -y 1` shows what ACKs; a CRUMBS peripheral shows up like any other
device. Then:

```sh
build-linux/crumbs_simple_linux_controller /dev/i2c-1 0x10      # send a test frame, read the reply
build-linux/crumbs_simple_linux_controller scan                 # CRUMBS devices on /dev/i2c-1
build-linux/crumbs_simple_linux_controller scan strict          # read-only probe
```

Non-strict scanning writes an all-zero frame to addresses that do not answer
a read; do not run it on a bus with an EEPROM at an unknown address
([protocol.md](protocol.md#discovery)).

## Wiring

SDA to SDA, SCL to SCL, and a common ground; one pair of pull-ups per bus.

- Arduino-only buses: 4.7 kΩ from SDA and SCL to the boards' logic voltage.
  The AVR `Wire` core turns on the chip's internal pull-ups, but those are
  tens of kΩ and only adequate for two boards on a few centimetres of wire.
- A Raspberry Pi controller: the Pi already pulls both lines to 3.3 V through
  its onboard resistors; add nothing, and never pull the bus to 5 V.
- Every device on one bus must sit at a distinct address. The examples use
  `0x10`; the LHWIT family uses `0x10`–`0x40` in steps of `0x10`; the address
  avoid-list is in [protocol.md](protocol.md#ic-address).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Controller sends, peripheral silent | Same address in both sketches; SDA/SCL not swapped; common ground; pull-ups present. `i2cdetect` (Linux) or `crumbs_arduino_scan` sees the peripheral? |
| `crumbs_controller_read` returns `-1` | The peripheral had nothing staged (no reply handler for the requested opcode) — see [protocol.md](protocol.md#the-read). |
| `-2` (CRC) on most reads from a Pi | The Pi's I²C controller does not honour clock stretching; a peripheral whose interrupt is late returns `0xFF`s. Keep handlers short; see [protocol.md](protocol.md#when-the-reply-is-built). |
| `CRUMBS_MAX_HANDLERS mismatch` at boot | The value was set in the sketch, not as a build flag for the library too. |
| `linux_wire not found` at configure | Install linux-wire ≥ 0.1.3 (a build tree is not enough) or pass its install prefix in `CMAKE_PREFIX_PATH`. |
| `crumbs_linux_scan` returns `-2` | The adapter cannot do an SMBus Quick Write; use strict mode. |
| `/dev/i2c-1: Permission denied` | Add yourself to the `i2c` group and log in again. |
