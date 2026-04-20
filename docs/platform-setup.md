# Platform Setup

This guide covers installation, configuration, and first-run setup for CRUMBS on all supported platforms: Arduino, PlatformIO, and Linux.

## Quick Start

**Platform:** [PlatformIO](#platformio-setup) (recommended), [Arduino IDE](#arduino-setup) (simple projects), or [Linux](#linux-setup) (Raspberry Pi/PC)

**Hardware:** 4.7kΩ pull-ups on SDA/SCL, common ground. **Level shifter for 5V↔3.3V.**

---

## Arduino Setup

### Installation

#### Method 1: Library Manager (recommended)

1. Open Arduino IDE
2. Go to Sketch → Include Library → Manage Libraries
3. Search for "CRUMBS"
4. Click Install

#### Method 2: Manual Installation

1. Download or clone the CRUMBS repository
2. Place the `CRUMBS` folder in your Arduino `libraries` directory
   - Windows: `Documents\Arduino\libraries\`
   - macOS: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`
3. Restart Arduino IDE

### First Program (Peripheral)

Create a new sketch:

```cpp
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

#define I2C_ADDRESS 0x08

crumbs_context_t ctx;
static uint8_t g_led_state = 0;

static void reply_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *u)
{
    (void)ctx; (void)u;
    crumbs_build_version_reply(reply, 0x01, 1, 0, 0);  // type_id, major, minor, patch
}

static void reply_get_state(crumbs_context_t *ctx, crumbs_message_t *reply, void *u)
{
    (void)ctx; (void)u;
    crumbs_msg_init(reply, 0x01, 0x80);
    crumbs_msg_add_u8(reply, g_led_state);
}

void on_message(crumbs_context_t *ctx, const crumbs_message_t *msg)
{
    if (msg->opcode == 0x01 && msg->data_len >= 1) {
        g_led_state = msg->data[0];
        digitalWrite(LED_BUILTIN, g_led_state ? HIGH : LOW);
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    crumbs_arduino_init_peripheral(&ctx, I2C_ADDRESS);
    crumbs_set_callbacks(&ctx, on_message, NULL, NULL);
    crumbs_register_reply_handler(&ctx, 0x00, reply_version,   NULL);  // version query
    crumbs_register_reply_handler(&ctx, 0x80, reply_get_state, NULL);  // GET state
}

void loop() {
    // Wire callbacks handle everything
}
```

**Upload:** Select board/port, upload, wire I²C + GND + pull-ups.

**Always delay 10 ms between send/read:**

```cpp
crumbs_controller_send(&ctx, 0x08, &msg, crumbs_arduino_wire_write, NULL);
delay(10);

crumbs_message_t reply;
crumbs_controller_read(&ctx, 0x08, &reply, crumbs_arduino_read, NULL);
```

### First Program (Controller)

```cpp
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

crumbs_context_t ctx;

void setup() {
    Serial.begin(9600);
    crumbs_arduino_init_controller(&ctx);

    // Send LED ON command to peripheral at 0x08
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x01);  // type=1, opcode=1
    crumbs_msg_add_u8(&msg, 1);         // payload: LED ON

    int rc = crumbs_controller_send(&ctx, 0x08, &msg,
                                    crumbs_arduino_wire_write, NULL);

    Serial.print("Send result: ");
    Serial.println(rc);  // 0 = success
}

void loop() {
    delay(1000);
}
```

### Troubleshooting (Arduino)

| Issue                               | Solution                                                               |
| ----------------------------------- | ---------------------------------------------------------------------- |
| Compile error: "crumbs.h not found" | Verify library in `libraries/` folder, restart IDE                     |
| No response from peripheral         | Check wiring, verify addresses, measure 3.3V on SDA/SCL with pull-ups  |
| Data corruption                     | Add `Wire.setClock(100000)` to slow clock, add delays between messages |
| Multiple definitions error          | Include CRUMBS headers in only one file, or use header guards          |

### Hardware Notes

#### I²C pins by board

- Arduino Uno/Nano: A4 (SDA), A5 (SCL)
- Arduino Mega: 20 (SDA), 21 (SCL)
- Arduino Due: 20 (SDA), 21 (SCL) — 3.3V logic
- ESP32: GPIO 21 (SDA), GPIO 22 (SCL) — configurable
- ESP8266: GPIO 4 (SDA), GPIO 5 (SCL)

#### Pull-up resistors

- Required on SDA and SCL lines
- Typical value: 4.7kΩ to Vcc (3.3V or 5V depending on board)
- Many boards have built-in pull-ups (may need external ones for longer wires)

---

## PlatformIO Setup

### Installation (PlatformIO)

PlatformIO can automatically fetch CRUMBS as a library dependency.

**Example platformio.ini (Arduino Nano):**

```ini
[env:nanoatmega328new]
platform = atmelavr
board = nanoatmega328new
framework = arduino

lib_deps =
    cameronbrooks11/CRUMBS@^0.12.0

build_flags =
    -DCRUMBS_MAX_HANDLERS=8  ; optional: reduce handler table size
```

**Example platformio.ini (ESP32):**

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps =
    cameronbrooks11/CRUMBS@^0.12.0

build_flags =
    -DCRUMBS_MAX_HANDLERS=8  ; optional
```

> **ESP32 I²C pins:** GPIO 21 (SDA), GPIO 22 (SCL) by default. Use `Wire.begin(sda, scl)` to override.

**To install and build:**

```bash
pio lib install
pio run
```

### First Program

Create `src/main.cpp` with the same Arduino code shown above. PlatformIO will automatically:

- Download CRUMBS library
- Compile with correct build flags
- Link everything together

### Advanced Configuration

**To adjust handler memory usage:**

```ini
build_flags =
    -DCRUMBS_MAX_HANDLERS=4  # Reduce from default 16
```

**For multiple environments:**

```ini
[env:controller]
board = uno
build_flags =
    -DROLE_CONTROLLER

[env:peripheral]
board = nano
build_flags =
    -DROLE_PERIPHERAL
    -DI2C_ADDRESS=0x08
```

### Troubleshooting (PlatformIO)

| Issue              | Solution                                                       |
| ------------------ | -------------------------------------------------------------- |
| Library not found  | Run `pio lib install`, check internet connection               |
| Handler table full | Set `-DCRUMBS_MAX_HANDLERS=<smaller number>`                   |
| ABI mismatch error | Ensure `CRUMBS_MAX_HANDLERS` set in `build_flags`, not in code |
| Upload fails       | Check USB port permissions, verify board selection             |

---

## Linux Setup

> **Note:** Linux HAL supports **controller mode only**. Peripheral mode (I²C target) is not implemented. For peripheral devices, use Arduino or other microcontroller.

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake git

# Raspberry Pi OS
sudo apt-get install cmake git

# Fedora/RHEL
sudo dnf install cmake gcc git
```

### Install linux-wire Dependency

CRUMBS Linux HAL requires the `linux-wire` library for I²C bus access.

#### System-wide installation (recommended)

```bash
# Clone linux-wire
git clone https://github.com/feastorg/linux-wire.git
cd linux-wire

# Build and install (linux-wire preset flow)
cmake --preset minimal
cmake --build --preset minimal
sudo cmake --install build/minimal --prefix /usr/local
```

Presets require CMake 3.20+. If your host has older CMake, use the fallback `cmake -S . -B build` flow from the linux-wire README.

**To verify installation:**

```bash
cmake --find-package -DNAME=linux_wire -DCOMPILER_ID=GNU \
      -DLANGUAGE=C -DMODE=EXIST
# Should print: "linux_wire found."
```

#### Local build (development)

For development without system install:

```bash
# Build linux-wire locally (preset flow)
cd linux-wire
cmake --preset dev
cmake --build --preset dev

# Point CRUMBS at local linux-wire preset build
cd ../CRUMBS
cmake --preset linux -DCMAKE_PREFIX_PATH=$HOME/linux-wire/build/dev
cmake --build --preset linux
```

#### Long-term CMake Integration Pattern

The canonical CMake dependency contract for Linux HAL consumers is the namespaced target `linux_wire::linux_wire`.

Supported integration modes:

- **Installed package**: `find_package(linux_wire CONFIG REQUIRED)` provides `linux_wire::linux_wire` directly.
- **Sibling source ingestion**: add `linux-wire` before `CRUMBS` in the parent build. If a raw build-tree target named `linux_wire` already exists, `CRUMBS` creates a local bridge target named `linux_wire::linux_wire` and links against that.

This preserves one canonical target name for `CRUMBS` while still supporting active multi-repo development.

Parent-build example:

```cmake
cmake_minimum_required(VERSION 3.13)
project(my_linux_stack C CXX)

set(LINUX_WIRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CRUMBS_ENABLE_LINUX_HAL ON CACHE BOOL "" FORCE)
set(CRUMBS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CRUMBS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(../linux-wire linux_wire_subbuild)
add_subdirectory(../CRUMBS crumbs_subbuild)
```

Avoid linking exported or installable CRUMBS targets directly to a raw `linux_wire` target. That breaks CMake export generation. The bridge or installed-package path is the stable pattern.

### Install CRUMBS

```bash
# Clone CRUMBS
git clone https://github.com/feastorg/CRUMBS.git
cd CRUMBS

# Configure and build (Linux preset enables the HAL and examples)
cmake --preset linux
cmake --build --preset linux

# Optional: system-wide install
sudo cmake --install build-linux --prefix /usr/local
```

### I²C Device Permissions

Linux I²C devices (`/dev/i2c-*`) typically require root access or group membership.

#### Option 1: Add user to i2c group

```bash
sudo usermod -a -G i2c $USER
# Log out and back in for changes to take effect
```

#### Option 2: Use sudo

```bash
sudo ./build/crumbs_simple_linux_controller
```

#### Option 3: udev rule (advanced)

Create `/etc/udev/rules.d/99-i2c.rules`:

```text
KERNEL=="i2c-[0-9]*", GROUP="i2c", MODE="0660"
```

Then reload rules:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Enable I²C on Raspberry Pi

```bash
# Enable I²C interface
sudo raspi-config
# Navigate to: Interfacing Options → I2C → Enable

# Verify I²C device exists
ls -l /dev/i2c-*
# Should show /dev/i2c-1 (or /dev/i2c-0 on older models)

# Load kernel module (if needed)
sudo modprobe i2c-dev
```

### First Program (Linux Controller)

The example programs are built automatically when `CRUMBS_BUILD_EXAMPLES=ON`.

**Run simple controller:**

```bash
sudo ./build/crumbs_simple_linux_controller /dev/i2c-1 0x08
```

**Scan for CRUMBS devices:**

```bash
# Non-strict mode (attempt reads, send probe if needed)
sudo ./build/crumbs_simple_linux_controller scan

# Strict mode (read-only checks, safer for sensitive devices)
sudo ./build/crumbs_simple_linux_controller scan strict
```

### Write Your Own Linux Controller

**Example CMake project structure:**

```text
my_project/
├── CMakeLists.txt
└── src/
    └── main.c
```

**CMakeLists.txt:**

```cmake
cmake_minimum_required(VERSION 3.13)
project(my_controller C)

find_package(crumbs CONFIG REQUIRED)

add_executable(my_controller src/main.c)
target_link_libraries(my_controller PRIVATE crumbs::crumbs)
```

**src/main.c:**

```c
#include "crumbs.h"
#include "crumbs_linux.h"
#include "crumbs_message_helpers.h"
#include <stdio.h>

int main() {
    crumbs_context_t ctx;
    crumbs_linux_i2c_t bus;

    // Open I²C bus
    int rc = crumbs_linux_init_controller(&ctx, &bus, "/dev/i2c-1", 10000);
    if (rc != 0) {
        fprintf(stderr, "Failed to open I2C bus: %d\n", rc);
        return 1;
    }

    // Send message
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x01);
    crumbs_msg_add_u8(&msg, 1);  // LED ON

    rc = crumbs_controller_send(&ctx, 0x08, &msg,
                                crumbs_linux_i2c_write, &bus);

    printf("Send result: %d\n", rc);

    crumbs_linux_close(&bus);
    return 0;
}
```

**Build:**

```bash
cmake -S . -B build
cmake --build build
./build/my_controller
```

### Troubleshooting (Linux)

| Issue                       | Solution                                                     |
| --------------------------- | ------------------------------------------------------------ |
| `linux_wire not found`      | Install linux-wire with presets, or set `-DCMAKE_PREFIX_PATH=$HOME/linux-wire/build/dev` (or `build/minimal`) |
| `/dev/i2c-1` not found      | Enable I²C in raspi-config, load `i2c-dev` module            |
| Permission denied           | Add user to `i2c` group or use `sudo`                        |
| `i2c-dev` module not loaded | Run `sudo modprobe i2c-dev`, add to `/etc/modules` for boot  |
| Link errors                 | Verify `crumbs::crumbs` target linked in CMake               |
| Wrong I²C bus               | Use `i2cdetect -l` to list buses, adjust device path         |

---

## Hardware Wiring

### Standard I²C Connection

```text
Controller          Peripheral
---------          ----------
    SDA  ───────────  SDA
    SCL  ───────────  SCL
    GND  ───────────  GND
     |                 |
     ├── 4.7kΩ ── Vcc  (pull-up)
     └── 4.7kΩ ── Vcc  (pull-up)
```

### Multiple Peripherals

```text
Controller          Peripheral 1 (0x08)    Peripheral 2 (0x09)
---------          ------------------      ------------------
    SDA  ────┬──────  SDA                      SDA
             │
    SCL  ────┼──┬───  SCL                      SCL
             │  │
    GND  ────┼──┼───  GND                      GND
             │  │
       4.7kΩ │  └── 4.7kΩ to Vcc
             │
             └────── Vcc (3.3V or 5V)
```

**Important:**

- All devices must share a common ground
- Only one set of pull-up resistors needed per bus
- Use 3.3V logic if any device is 3.3V-only
- Maximum bus length: ~1 meter (longer requires lower pull-up resistance)

---

## Verification and Testing

### I²C Bus Scanner (Arduino)

Use the built-in Wire scanner to verify devices are visible:

```cpp
#include <Wire.h>

void setup() {
    Serial.begin(9600);
    Wire.begin();

    Serial.println("Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("Found device at 0x");
            Serial.println(addr, HEX);
        }
    }
}

void loop() {}
```

### CRUMBS Device Scanner (Arduino)

Verify devices speak CRUMBS protocol:

```cpp
#include <crumbs_arduino.h>

crumbs_context_t ctx;

void setup() {
    Serial.begin(9600);
    crumbs_arduino_init_controller(&ctx);

    uint8_t found[32];
    int count = crumbs_controller_scan_for_crumbs(
        &ctx, 0x08, 0x77, 1,  // strict mode
        crumbs_arduino_wire_write, crumbs_arduino_read, NULL,
        found, 32, 50000);

    Serial.print("Found ");
    Serial.print(count);
    Serial.println(" CRUMBS devices:");

    for (int i = 0; i < count; i++) {
        Serial.print("  0x");
        Serial.println(found[i], HEX);
    }
}

void loop() {}
```

If your physical bus also includes non-CRUMBS peripherals, avoid broad range scans and use an explicit candidate list:

```c
uint8_t candidates[] = {0x10, 0x12, 0x20};
uint8_t found[8], types[8];
int count = crumbs_controller_scan_for_crumbs_candidates(
    &ctx, candidates, sizeof(candidates), 1,
    crumbs_arduino_wire_write, crumbs_arduino_read, NULL,
    found, types, 8, 10000);
```

### I²C Bus Scanner (Linux)

```bash
# List I²C buses
i2cdetect -l

# Scan bus 1 for devices
i2cdetect -y 1
```

### CRUMBS Device Scanner (Linux)

```bash
# Non-strict mode
sudo ./build/crumbs_simple_linux_controller scan

# Strict mode (safer, read-only)
sudo ./build/crumbs_simple_linux_controller scan strict
```

For mixed buses, prefer candidate-address scanning in application code instead of broad `0x08..0x77` sweeps.

---

## Next Steps

Once your platform is set up and basic communication is working:

1. **Work through examples** — Progressive tutorials in `examples/core_usage/`
   - Start with `hello_peripheral/` + `hello_controller/` (Arduino)
   - Or `simple_controller/` (Linux)

2. **Learn handler dispatch** — Register per-opcode handlers instead of switch statements
   - SET ops: `crumbs_register_handler()` — see `examples/handlers_usage/`
   - GET ops: `crumbs_register_reply_handler()` — see `examples/families_usage/lhwit_family/`
   - Read [API Reference: Handler Dispatch](api-reference.md#handler-dispatch)

3. **Use message helpers** — Type-safe payload builders
   - Include `crumbs_message_helpers.h`
   - See [API Reference: Message Helpers](api-reference.md#message-helpers)

4. **Create command headers** — Reusable command definitions
   - See `examples/handlers_usage/mock_ops.h`

5. **Optimize memory** — Reduce handler table size for constrained devices
   - Set `CRUMBS_MAX_HANDLERS` via build flags

---

## See Also

- [API Reference](api-reference.md) — Complete function documentation
- [Protocol Specification](protocol.md) — Wire format and versioning
- [Examples](examples.md) — Working code for all platforms
- [Architecture](architecture.md) — Design decisions and internals
