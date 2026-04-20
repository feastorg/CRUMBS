# Core Usage Examples (Tier 1)

**START HERE** if you're new to CRUMBS.

## Learning Path

**Level 1 (15min):** [hello_peripheral](arduino/hello_peripheral/) • [hello_controller](arduino/hello_controller/)  
**Level 2 (30min):** [basic_peripheral](arduino/basic_peripheral/) • [basic_controller](arduino/basic_controller/)  
**Level 3 (1hr):** [advanced_controller](arduino/advanced_controller/)

**Next:** [handlers_usage](../handlers_usage/) | [families_usage](../families_usage/)

---

## All Arduino Examples

| Example                                                            | Description                                       |
| ------------------------------------------------------------------ | ------------------------------------------------- |
| [hello_peripheral/](arduino/hello_peripheral/)                     | Minimal peripheral (START HERE)                   |
| [hello_controller/](arduino/hello_controller/)                     | Minimal controller (pair with hello_peripheral)   |
| [basic_peripheral/](arduino/basic_peripheral/)                     | Basic peripheral with multiple commands           |
| [basic_controller/](arduino/basic_controller/)                     | Basic controller with key commands                |
| [advanced_controller/](arduino/advanced_controller/)               | Advanced controller with CSV parsing and scanning |
| [mixed_bus_controller/](arduino/mixed_bus_controller/)             | CRUMBS (`basic_peripheral`) + raw sensor access on one bus |
| [mixed_bus_controller_vendor/](arduino/mixed_bus_controller_vendor/) | CRUMBS + EZO (`ezo-driver`) + SparkFun BMP/BME280 on one bus |
| [basic_peripheral_noncrumbs/](arduino/basic_peripheral_noncrumbs/) | Raw I²C peripheral (no CRUMBS) for comparison     |

**Quick Start:** Upload [hello_peripheral](arduino/hello_peripheral/) and [hello_controller](arduino/hello_controller/) to see CRUMBS working in 15 minutes.

For mixed-bus validation, run `basic_peripheral` on unique addresses (default full profile: `0x08`, `0x09`, `0x0A`).

### Getting Started (Arduino)

1. Install the CRUMBS library via Arduino Library Manager or copy `src/` to your libraries folder
2. Open any example sketch in Arduino IDE
3. Select your board and port
4. Upload and open Serial Monitor (115200 baud)

---

## PlatformIO Examples

| Example                                             | Description                     |
| --------------------------------------------------- | ------------------------------- |
| [simple_peripheral/](platformio/simple_peripheral/) | Basic peripheral for PlatformIO |
| [simple_controller/](platformio/simple_controller/) | Basic controller for PlatformIO |

### Getting Started (PlatformIO)

```bash
# Build and upload peripheral
cd examples/core_usage/platformio/simple_peripheral
pio run --target upload

# Build and upload controller
cd examples/core_usage/platformio/simple_controller
pio run --target upload
```

---

## Linux Examples

| Example                                        | Description                               |
| ---------------------------------------------- | ----------------------------------------- |
| [simple_controller/](linux/simple_controller/) | Linux controller using linux-wire library |
| [mixed_bus_probe/](linux/mixed_bus_probe/) | Generic raw mixed-bus probe tool (CRUMBS + register I/O) |
| [mixed_bus_lab_validation/](linux/mixed_bus_lab_validation/) | Bench validation pass: 2x DCMT + 1x RLHT + EZO pH/DO (+ optional BMP/BME) |

### Getting Started (Linux)

```bash
# From CRUMBS repo root
cmake --preset linux
cmake --build --preset linux

./build-linux/crumbs_simple_linux_controller /dev/i2c-1 0x14
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 scan 0x20,0x21 strict

# Topology-specific lab pass (3 CRUMBS + EZO pH/DO, optional BMP/BME)
./build-linux/crumbs_mixed_bus_lab_validation /dev/i2c-1
```

**Note:** Requires [linux-wire](https://github.com/feastorg/linux-wire) installed (recommended: `cmake --preset minimal && cmake --build --preset minimal && sudo cmake --install build/minimal --prefix /usr/local`).

### Mixed-Bus Validation Path (Pre-Release)

Use this sequence to validate the new raw-I2C helper APIs end-to-end before release:

1. `scan` known CRUMBS addresses only:

```bash
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 scan 0x20,0x21,0x30 strict
```

2. read sensor chip-ID register with repeated-start:

```bash
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 read-u8 0x76 0xD0 1 repeat
```

For BMP/BME280, expected chip ID from that command is:
- `0x58` (BMP280) or `0x60` (BME280)

3. read u16-addressed register device:

```bash
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 read-u16 0x40 0x0100 6 repeat
```

4. optional generic preamble read/write using `-ex` APIs:

```bash
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 read-ex 0x1E 0x20,0x08 6 repeat
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 write-ex 0x1E 0x20,0x09 0x10,0x00
```

5. optional sensor register write:

```bash
./build-linux/crumbs_mixed_bus_probe /dev/i2c-1 write-u8 0x76 0xF4 0x27
```

---

## Quick Start Path

1. **Start:** [hello_peripheral](arduino/hello_peripheral/) + [hello_controller](arduino/hello_controller/)
2. **Learn:** [basic_peripheral](arduino/basic_peripheral/) + [basic_controller](arduino/basic_controller/) (commands & queries)
3. **Compare:** [basic_peripheral_noncrumbs](arduino/basic_peripheral_noncrumbs/) (CRUMBS vs raw I²C)
4. **Next tier:** [handlers_usage](../handlers_usage/) (modular command routing)
