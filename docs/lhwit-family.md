# LHWIT Family Guide

**LHWIT** = Low Hardware Implementation Test

Reference implementation demonstrating multi-device I²C systems with canonical operation headers and CRUMBS 0.10+ SET_REPLY mechanism.

## Quick Reference

| Device     | Type | Addr | Pattern          | Purpose                        |
| ---------- | ---- | ---- | ---------------- | ------------------------------ |
| Calculator | 0x03 | 0x10 | Function-style   | 32-bit calculator with history |
| LED Array  | 0x01 | 0x20 | State-query      | 4 LEDs with blink              |
| Servo      | 0x02 | 0x30 | Position-control | 2 servos with sweep            |
| Display    | 0x04 | 0x40 | Display-control  | Quad 7-segment display         |

**Controllers:** Discovery (scan-based) and Manual (config-based)

---

## Architecture

### Canonical Headers

Operation definitions live in shared headers included by both peripherals and controllers:

- `calculator_ops.h` - Calculator operations (Type 0x03)
- `led_ops.h` - LED operations (Type 0x01)
- `servo_ops.h` - Servo operations (Type 0x02)
- `display_ops.h` - Display operations (Type 0x04)
- `lhwit_ops.h` - Convenience header (includes all four)

Benefits: no protocol drift, no magic numbers, single source of truth.

### SET_REPLY Pattern

Operations (0x01–0x7F) set state, queries (0x80–0xFF) retrieve it:

1. Send operation → peripheral stores result → returns ACK
2. Send query → peripheral returns stored data

---

## Device Specifications

### Calculator (Type 0x03)

#### Calculator Operations

| Op   | Name | Payload           | Description |
| ---- | ---- | ----------------- | ----------- |
| 0x01 | ADD  | 8B (u32 a, u32 b) | Add         |
| 0x02 | SUB  | 8B (u32 a, u32 b) | Subtract    |
| 0x03 | MUL  | 8B (u32 a, u32 b) | Multiply    |
| 0x04 | DIV  | 8B (u32 a, u32 b) | Divide      |

**Queries:**

| Op        | Name           | Reply | Description       |
| --------- | -------------- | ----- | ----------------- |
| 0x80      | GET_RESULT     | 4B    | Last result (i32) |
| 0x81      | GET_HIST_META  | 2B    | Count + position  |
| 0x82–0x8D | GET_HIST_0..11 | 16B   | History entries   |

**State:** Last result (i32), 12-entry ring buffer (192B total), each entry: operation, operands, result, timestamp.

**Notes:** Division by zero returns error. Overflow wraps. History wraps at 12 entries.

---

### LED Array (Type 0x01)

#### LED Array Operations

| Op   | Name    | Payload              | Description     |
| ---- | ------- | -------------------- | --------------- |
| 0x01 | SET_ALL | 1B bitmask           | Set all LEDs    |
| 0x02 | SET_ONE | 2B (led, state)      | Set single LED  |
| 0x03 | BLINK   | 4B (led, en, period) | Configure blink |

#### Queries

| Op   | Name      | Reply | Description             |
| ---- | --------- | ----- | ----------------------- |
| 0x80 | GET_STATE | 1B    | LED bitmask             |
| 0x81 | GET_BLINK | 12B   | Blink config (all LEDs) |

**Hardware:** D4-D7 → LEDs → 220Ω → GND

**Notes:** Bitmask: bit 0=LED0, bit 1=LED1, etc. Blink period is full cycle (50% duty). Independent timers per LED.

---

### Servo (Type 0x02)

#### Servo Operations

| Op   | Name      | Payload                        | Description         |
| ---- | --------- | ------------------------------ | ------------------- |
| 0x01 | SET_POS   | 2B (servo, pos)                | Set position 0–180° |
| 0x02 | SET_SPEED | 2B (servo, speed)              | Set speed 0–20      |
| 0x03 | SWEEP     | 5B (servo, en, min, max, step) | Configure sweep     |

**Queries:**

| Op   | Name      | Reply | Description      |
| ---- | --------- | ----- | ---------------- |
| 0x80 | GET_POS   | 2B    | Positions (both) |
| 0x81 | GET_SPEED | 2B    | Speed settings   |
| 0x82 | GET_SWEEP | 10B   | Sweep config     |

**Hardware:** D9/D10 → Servo signal. **External 5V power required** (2-3A). Common ground with Arduino.

**Notes:** Speed 0=instant, 1–20=gradual. Sweep oscillates between min/max. Position 0=CCW, 90=center, 180=CW.

---

### Display (Type 0x04)

**Operations:**

| Op   | Name           | Payload           | Description             |
| ---- | -------------- | ----------------- | ----------------------- |
| 0x01 | SET_NUMBER     | 3B (num, decimal) | Display number 0–9999   |
| 0x02 | SET_SEGMENTS   | 4B (patterns)     | Custom segment patterns |
| 0x03 | SET_BRIGHTNESS | 1B (level)        | Brightness control      |
| 0x04 | CLEAR          | —                 | Clear display           |

**Queries:**

| Op   | Name      | Reply | Description               |
| ---- | --------- | ----- | ------------------------- |
| 0x80 | GET_VALUE | 4B    | Current number+pos+bright |

**Hardware:** Quad 7-segment display (5641AS or compatible) with multiplexing control.

**Notes:** SET_NUMBER decimal_pos: 0=no decimal, 1–4=position after digit. Brightness level 0–10. GET_VALUE returns: `[number:u16][decimal_pos:u8][brightness:u8]`.

---

## Controllers

### Discovery Controller

Scans I²C bus (0x08–0x77), queries device types, stores addresses. Interactive shell with `scan`, `list`, `help` commands.

**Advantages:** Flexible addressing, type verification, handles address changes.  
**Limitations:** Slower startup (~1-2s scan), requires type query support.

### Manual Controller

Uses hardcoded addresses from `config.h`. Same command interface, no scan needed.

**Advantages:** Fast startup, predictable, production-ready.  
**Limitations:** Requires known addresses, rebuild for changes.

**Configuration:** Edit `config.h` with `CALCULATOR_ADDR`, `LED_ADDR`, `SERVO_ADDR`, `DISPLAY_ADDR`.

---

## Hardware Setup

### Bill of Materials

- 1x Linux SBC (Raspberry Pi, Orange Pi)
- 4x Arduino Nano (ATmega328P)
- 4x LEDs + 4x 220Ω resistors
- 2x Servos (SG90 or similar)
- 1x 5641AS Quad 7-segment display
- 1x External 5V supply (2-3A for servos)
- Breadboard + jumper wires

### I²C Bus Wiring

All Arduino Nanos share I²C bus:

- SDA: Linux SBC → A4 (all 4 Arduinos)
- SCL: Linux SBC → A5 (all 4 Arduinos)
- GND: Common ground

**Raspberry Pi I²C:** GPIO 2 (Pin 3) = SDA, GPIO 3 (Pin 5) = SCL

### LED Wiring

Arduino Nano 2:

- D4-D7 → LED → 220Ω → GND

### Servo Wiring

Arduino Nano 3:

- D9/D10 → Servo signal (orange/yellow)
- **Servo power:** External 5V supply only (NOT Arduino 5V)
- **Critical:** Common ground between Arduino and external supply

**⚠️ WARNING:** Each servo draws 500mA+. Do not power from Arduino. Use external 5V supply (2-3A minimum) with common ground.

### Safety Checklist

- [ ] All I²C connections correct (SDA, SCL, GND)
- [ ] LED resistors in place (220Ω)
- [ ] Servo power from external supply (NOT Arduino)
- [ ] Common ground between servo supply and Arduino
- [ ] External supply 4.8-6V, 2-3A minimum

---

## Software Setup

### Building Peripherals

```bash
cd examples/families_usage/lhwit_family/calculator && pio run
cd ../led && pio run
cd ../servo && pio run
cd ../display && pio run
```

### Flashing Peripherals

```bash
cd calculator && pio run -t upload  # Arduino 1 → 0x10
cd ../led && pio run -t upload      # Arduino 2 → 0x20
cd ../servo && pio run -t upload    # Arduino 3 → 0x30
cd ../display && pio run -t upload  # Arduino 4 → 0x40
```

Verify with: `i2cdetect -y 1`

### Building Controllers

```bash
cd examples/families_usage/controller_discovery
mkdir -p build && cd build && cmake .. && make

cd ../../controller_manual
mkdir -p build && cd build && cmake .. && make
```

### I²C Permissions

```bash
sudo usermod -a -G i2c $USER  # Log out and back in
```

---

## Usage Guide

### Starting System

1. Power on all 4 Arduino Nanos via USB
2. Verify: `i2cdetect -y 1` shows 0x10, 0x20, 0x30, 0x40
3. Run controller: `./controller_discovery /dev/i2c-1`

### Command Reference

#### Calculator

```text
calculator add <a> <b>      Add
calculator sub <a> <b>      Subtract
calculator mul <a> <b>      Multiply
calculator div <a> <b>      Divide
calculator result           Get last result
calculator history          Show all history
```

#### LED

```text
led set_all <mask>          Set all (0x00–0x0F bitmask)
led set_one <led> <state>   Set single (led: 0-3, state: 0/1)
led blink <led> <en> <ms>   Configure blink
led get_state               Get current state
```

#### Servo

```text
servo set_pos <n> <deg>         Set position (n: 0–1, deg: 0–180)
servo set_speed <n> <speed>     Set speed (0–20)
servo sweep <n> <en> <min> <max> <step>  Configure sweep
servo get_pos                   Get positions
```

#### Display

```text
display set_number <num> <dec>  Display number (num: 0–9999, dec: 0–4)
display clear                   Clear display
display get_value               Get displayed value
```

#### Discovery only

```text
scan                        Discover devices
list                        Show discovered devices
```

#### Common

```text
help                        Show help
quit / exit                 Exit program
```

### Example Session

```bash
lhwit> scan
Found Calculator at 0x10, LED Array at 0x20, Servo Controller at 0x30, Display at 0x40

lhwit> calculator add 10 20
lhwit> calculator result
Result: 30

lhwit> led set_all 0x0F
All LEDs on

lhwit> servo set_pos 0 90
Servo 0 to center

lhwit> display set_number 1234 2
Display shows: 12.34
```

---

## Common Patterns

### Function-Style (Calculator)

Device performs computation, stores result internally. Controller queries when convenient.

**Use cases:** Sensor processing, mathematical operations, state machines.

### State-Query (LED)

Device maintains output state. Operations modify state, queries report it. Background processes run autonomously.

**Use cases:** GPIO expanders, PWM controllers, display controllers.

### Position-Control (Servo)

Device controls physical actuators. Operations set targets, queries provide feedback. Autonomous movement patterns.

**Use cases:** Motor controllers, steppers, robotic actuators.

---

## Testing Procedures

### Initial Verification

1. `i2cdetect -y 1` → verify 0x10, 0x20, 0x30
2. `scan` → verify all 3 devices found
3. Test basic operations on each device

### Calculator Testing

```bash
calculator add 100 200 → result → expect 300
calculator sub 100 50 → result → expect 50
calculator mul 12 13 → result → expect 156
calculator div 100 5 → result → expect 20
calculator div 10 0 → expect error
calculator history → verify all operations listed
```

### LED Testing

```bash
led set_all 0x01 → LED 0 on only
led set_all 0x0F → all LEDs on
led set_all 0x05 → LED 0 and 2 on
led blink 0 1 1000 → LED 0 blinks 1Hz
led blink 0 0 0 → LED 0 stops
led get_state → verify current state
```

### Servo Testing

```bash
servo set_pos 0 0 → fully CCW
servo set_pos 0 90 → center
servo set_pos 0 180 → fully CW
servo set_speed 0 10 → medium speed
servo sweep 0 1 0 180 10 → oscillate 0–180°, 10° steps
servo sweep 0 0 0 0 0 → stop sweep
servo get_pos → verify positions
```

---

## Troubleshooting

### No Devices Found

- Check I²C wiring (SDA=A4, SCL=A5, GND)
- Verify USB power to all Arduinos
- Try `/dev/i2c-0` instead of `-1`
- Check unique addresses (0x10, 0x20, 0x30)
- Re-flash peripherals

### Device Not Responding

- Verify CRUMBS 0.10+ in peripherals
- Check I²C wiring quality (short wires, good connections)
- Add external 4.7kΩ pull-ups if needed
- Reset Arduino and retry

### LED Not Working

- Check LED polarity (long leg to Arduino pin)
- Verify 220Ω resistor present
- Confirm D4-D7 pins (not A4-A7)
- Test LED with multimeter

### Servo Not Moving

- **Verify external 5V power connected** (most common issue)
- Check common ground between supply and Arduino
- Verify 2-3A supply capacity
- Check signal wire to D9/D10
- Confirm servo is 180° type

### Calculator Wrong Result

- Check 32-bit signed limits (-2B to 2B)
- Division by zero returns error
- Use `history` to verify operation order

### I²C Communication Errors

- Shorten I²C wires (<30cm)
- Add external pull-ups (4.7kΩ)
- Separate I²C from power wires
- Reduce bus speed if errors persist

### Controller Issues

- Verify `i2c` group membership: `groups | grep i2c`
- Check I²C device permissions: `ls -l /dev/i2c-1`
- Increase timeout if slow peripherals

---

## Extending the Family

### Adding New Device

1. Create `newdevice_ops.h` with type ID, operations, queries, helpers
2. Create PlatformIO peripheral project
3. Update controllers with device commands
4. Document wiring and operations

### Design Checklist

- [ ] Unique type ID (0x04+)
- [ ] Unique I²C address
- [ ] Operations 0x01–0x7F, queries 0x80–0xFF
- [ ] Helper functions in header
- [ ] SET_REPLY pattern followed
- [ ] Hardware documented
- [ ] README created

### Recommended Devices

- Temperature sensor (Type 0x04) - DHT22/DS18B20
- Buzzer (Type 0x05) - tone generation
- Button array (Type 0x06) - debounce + events
- RGB LED (Type 0x07) - WS2812
- Stepper motor (Type 0x08) - position control
- Display (Type 0x09) - OLED/LCD

---

## Additional Resources

- [CRUMBS README](../README.md)
- [Families Usage Overview](../examples/families_usage/README.md)
- [LHWIT Family README](../examples/families_usage/lhwit_family/README.md)
- [Calculator README](../examples/families_usage/lhwit_family/calculator/README.md)
- [LED README](../examples/families_usage/lhwit_family/led/README.md)
- [Servo README](../examples/families_usage/lhwit_family/servo/README.md)
- [Discovery Controller README](../examples/families_usage/controller_discovery/README.md)
- [Manual Controller README](../examples/families_usage/controller_manual/README.md)
- [Protocol Specification](protocol.md)
- [API Reference - Handler Dispatch](api-reference.md#peripheral--crumbsh)

---

**Version:** 1.0  
**CRUMBS:** 0.10.1+  
**Updated:** 2026-02-01
