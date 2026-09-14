# LHWIT Family - Low Hardware Implementation Test

Four-device reference family demonstrating different handler patterns with minimal hardware requirements.

## Overview

The LHWIT family consists of four devices showcasing different interaction patterns:

| Device         | Type ID | Pattern          | Description                                                       |
| -------------- | ------- | ---------------- | ----------------------------------------------------------------- |
| **Calculator** | 0x03    | Function-style   | Executes operations (ADD/SUB/MUL/DIV), returns results on request |
| **LED Array**  | 0x01    | State-query      | Controls 4 LEDs, reports current state on request                 |
| **Servo**      | 0x02    | Position-control | Moves 2 servos, reports positions on request                      |
| **Display**    | 0x04    | Display-control  | 4-digit 7-segment display, shows numbers and custom patterns      |

**Why four patterns?** Each represents a common device interaction model:

- **Function-style:** Device performs computation, stores result (like sensors with processing)
- **State-query:** Device manages output state, reports on demand (like GPIO expanders)
- **Position-control:** Device controls physical actuators with feedback (like motor controllers)
- **Display-control:** Device manages visual output with multiplexing (like displays and indicators)

## Hardware Requirements

### Components

- **1x Linux SBC** - Raspberry Pi, Orange Pi, etc. (for controllers)
- **4x Arduino Nano** - ATmega328P boards (for peripherals)
- **4x LEDs** - Standard 5mm LEDs (any color)
- **4x 220Ω resistors** - For LED current limiting
- **2x Servo motors** - SG90 or similar (180° range)
- **1x 5641AS display** - Quad 7-segment display (common cathode)
- **1x External 5V power supply** - At least 2A capacity (for servos)
- **1x Breadboard** - For connections
- **Jumper wires** - Male-to-male and male-to-female

### Wiring Overview

All four Arduino Nano boards connect to the same I²C bus:

- **SDA** → A4 (Arduino Nano)
- **SCL** → A5 (Arduino Nano)
- **GND** → Common ground

**LED connections** (LED peripheral only):

- D4-D7 → LEDs → 220Ω resistors → GND

**Servo connections** (Servo peripheral only):

- D9 → Servo 0 signal
- D10 → Servo 1 signal
- Servo power: **EXTERNAL 5V supply** (NOT from Arduino!)

**Display connections** (Display peripheral only):

- D2-D9 → 5641AS segments (a, b, c, d, e, f, g, dp)
- D10-D13 → 5641AS digit select (D1, D2, D3, D4)

⚠️ **Critical:** Servos draw significant current. Never power servos from Arduino's 5V pin. Use external 5V power supply with common ground.

## Quick Start

### 1. Flash Peripherals

```bash
cd calculator && pio run -t upload
cd ../led && pio run -t upload
cd ../servo && pio run -t upload
cd ../display && pio run -t upload
```

**Addresses:**

- Calculator: 0x10
- LED: 0x20
- Servo: 0x30
- Display: 0x40

### 2. Build Controllers

```bash
# Discovery controller (auto-finds devices)
cd ../../controller_discovery
mkdir -p build && cd build
cmake ..
make

# Manual controller (uses config.h addresses)
cd ../../controller_manual
mkdir -p build && cd build
cmake ..
make
```

### 3. Test System

**With discovery controller:**

```bash
./controller_discovery /dev/i2c-1
lhwit> scan
lhwit> calculator 0 add 10 20
lhwit> calculator 0 result
lhwit> led 0 set_all 0x0F
lhwit> servo 0 set_pos 0 90
lhwit> display 0 set_number 1234 0
```

**With manual controller:**

```bash
./controller_manual /dev/i2c-1
lhwit> calculator 0 add 5 3
lhwit> led 0 get_state
lhwit> servo 0 get_pos
lhwit> display 0 set_number 42 0
```

## Module Details

### Calculator (Type 0x03)

32-bit integer calculator with operation history.

**Operations:**

- `ADD` (0x01) - Add two u32 values
- `SUB` (0x02) - Subtract (a - b)
- `MUL` (0x03) - Multiply two u32 values
- `DIV` (0x04) - Divide (a / b)

**Queries:**

- `GET_RESULT` (0x80) - Last calculation result
- `GET_HIST_META` (0x81) - History metadata (count + position)
- `GET_HIST_0` to `GET_HIST_11` (0x82–0x8D) - Individual history entries

**State:**

- Last result (32-bit)
- 12-entry history buffer (192 bytes total)
- Each entry: operation, operands, result, timestamp

**Example usage:**

```c
#include "calculator_ops.h"

uint8_t msg[32];
int len = calculator_send_add(&ctx, 0x10, 100, 200, msg);
crumbs_linux_i2c_write(&lw, 0x10, msg, len);

// Later: get result
len = calculator_send_get_result(&ctx, 0x10, msg);
crumbs_linux_i2c_write(&lw, 0x10, msg, len);

uint8_t reply[32];
int reply_len = crumbs_linux_read_message(&ctx, &lw, 0x10, reply, sizeof(reply), 100);

int32_t result;
calculator_parse_result_reply(reply, reply_len, &result);
printf("Result: %d\n", result);
```

### LED Array (Type 0x01)

Controls 4 LEDs (D4-D7) with individual and blink control.

**Operations:**

- `SET_ALL` (0x01) - Set all LEDs via bitmask
- `SET_ONE` (0x02) - Set single LED on/off
- `BLINK` (0x03) - Configure LED blinking

**Queries:**

- `GET_STATE` (0x80) - Current LED state (bitmask)
- `GET_BLINK` (0x81) - Blink configuration

**State:**

- LED bitmask (bits 0–3 for LEDs 0–3)
- Blink timers (per-LED)

**Hardware:**

- D4 → LED 0
- D5 → LED 1
- D6 → LED 2
- D7 → LED 3
- All through 220Ω resistors to GND

### Servo Controller (Type 0x02)

Controls 2 hobby servos (D9-D10) with speed and sweep.

**Operations:**

- `SET_POS` (0x01) - Set servo position (0–180°)
- `SET_SPEED` (0x02) - Set movement speed (0=instant, 1–20=slow)
- `SWEEP` (0x03) - Configure sweep pattern

**Queries:**

- `GET_POS` (0x80) - Current positions (both servos)
- `GET_SPEED` (0x81) - Speed settings

**State:**

- Servo positions (uint8_t[2], degrees)
- Speed settings (uint8_t[2])
- Sweep state (enabled, min, max, step per servo)

**Hardware:**

- D9 → Servo 0 signal (PWM)
- D10 → Servo 1 signal (PWM)
- **External 5V supply required** (NOT from Arduino)
- Common ground between Arduino and servo power supply

⚠️ **Servo Power Warning:** Each servo can draw 500mA+ under load. Arduino's 5V regulator cannot supply this. Use dedicated 5V supply (2A minimum for 2 servos). Connect grounds together.

### Display (Type 0x04)

Controls 4-digit 7-segment display (5641AS or compatible) with multiplexing.

**Operations:**

- `SET_NUMBER` (0x01) - Display number (0–9999) with optional decimal point
- `SET_SEGMENTS` (0x02) - Set custom segment patterns for all 4 digits
- `SET_BRIGHTNESS` (0x03) - Set brightness level (0–10)
- `CLEAR` (0x04) - Clear display

**Queries:**

- `GET_VALUE` (0x80) - Current displayed number, decimal position, and brightness

**State:**

- Current number (uint16_t, 0–9999)
- Decimal position (uint8_t, 0=none, 1-4)
- Brightness level (uint8_t, 0–10)
- Display active flag

**Hardware:**

- D2-D9 → 5641AS segments (a, b, c, d, e, f, g, dp)
- D10-D13 → 5641AS digit select (D1, D2, D3, D4)
- Multiplexing: Continuously cycles through digits at ~2ms intervals

**Example usage:**

```c
#include "display_ops.h"

// Display "123.4" (decimal on digit 3)
display_send_set_number(&ctx, 0x40, write_fn, io_ctx, 1234, 3);

// Display "12.34" (decimal on digit 2)
display_send_set_number(&ctx, 0x40, write_fn, io_ctx, 1234, 2);

// Clear display
display_send_clear(&ctx, 0x40, write_fn, io_ctx);
```

## Controllers

### Discovery Controller

Auto-discovers devices by scanning I²C bus and identifying by type ID.

**Workflow:**

1. Run `scan` command
2. Controller queries 0x08–0x77 for CRUMBS devices
3. Identifies devices by type ID (0x01=LED, 0x02=Servo, 0x03=Calculator)
4. Stores addresses for use in commands

**Advantages:**

- Works regardless of device addresses
- Verifies device types
- Flexible for development

**Use when:** Addresses may change, testing new hardware, learning the system.

### Manual Controller

Uses hardcoded addresses from `config.h`.

**Workflow:**

1. Edit `config.h` with device addresses
2. Rebuild controller
3. Run - immediate command access (no scan)

**Advantages:**

- Faster startup (no scan)
- Simpler code flow
- Production-ready

**Use when:** Addresses are fixed and known, performance matters, production deployment.

## Canonical Headers

All operation definitions live in shared headers:

- `calculator_ops.h` - Calculator operations + helper functions
- `led_ops.h` - LED operations + helper functions
- `servo_ops.h` - Servo operations + helper functions
- `lhwit_ops.h` - Convenience header (includes all three)

**Pattern:** Both peripherals and controllers include the same headers, ensuring protocol consistency. No hardcoded magic numbers, no protocol drift.

## Testing Procedure

1. **Power on all three Arduino Nano boards**
2. **Verify connections:** Check I²C wiring, LED resistors, servo power
3. **Test discovery:** Run controller_discovery and execute `scan`
4. **Test each device:**
   - Calculator: `add 5 10`, then `result`
   - LED: `set_all 0x0F`, then `get_state`
   - Servo: `set_pos 0 90`, then `get_pos`
5. **Test complex operations:**
   - Calculator history: multiple operations, then `history`
   - LED blinking: `blink 0 1 500` (LED 0, 500ms period)
   - Servo sweep: `sweep 0 1 0 180 10` (servo 0, 0–180°, step 10)

## Troubleshooting

**No devices found during scan:**

- Check I²C wiring (SDA/SCL/GND)
- Verify peripherals are powered and running
- Use `i2cdetect -y 1` to see what's on the bus
- Check for address conflicts

**Servo not moving:**

- Verify external 5V power supply connected
- Check servo signal wire on D9/D10
- Check common ground between Arduino and servo supply
- Test with simple position command: `servo set_pos 0 90`

**LED not responding:**

- Check wiring: D4-D7 → LED → 220Ω → GND
- Test with: `led set_all 0x0F` (all on)
- Verify LED polarity (long leg = anode/+)

**Calculator returns wrong result:**

- Test simple operation: `add 2 2`, then `result`
- Check for overflow (32-bit integer limits)
- View history: `history` command

## Further Documentation

- **Comprehensive Guide:** [docs/lhwit-family.md](../../../docs/lhwit-family.md)
- **Discovery Controller:** [../controller_discovery/README.md](../controller_discovery/README.md)
- **Manual Controller:** [../controller_manual/README.md](../controller_manual/README.md)
- **Calculator Peripheral:** [calculator/README.md](calculator/README.md)
- **LED Peripheral:** [led/README.md](led/README.md)
- **Servo Peripheral:** [servo/README.md](servo/README.md)
