# Discovery Controller

Auto-discovery controller for CRUMBS lhwit_family peripherals on Linux I2C.

## Overview

This controller demonstrates proper CRUMBS usage with auto-discovery:

**CRUMBS Patterns Demonstrated:**

- `crumbs_linux_scan_for_crumbs_with_types()` - Type-aware bus scanning (auto-suppresses scan noise)
- Canonical `*_ops.h` helper functions - Protocol-defined command builders
- SET_REPLY query pattern - Two-step query/read for GET operations
- Platform-specific `crumbs_linux_read_message()` - Linux I2C read wrapper
- Version querying and compatibility checking per [protocol.md](../../../docs/protocol.md#versioning-convention)

**Application Features:**

- Automatically finds Calculator (0x03), LED (0x01), Servo (0x02), and Display (0x04) devices
- Queries and verifies version compatibility during scan
- Blocks incompatible devices with clear guidance
- Interactive shell for controlling discovered peripherals
- Device-found checks before executing commands

## Usage

```bash
# Build
cd examples/families_usage/controller_discovery
mkdir build && cd build
cmake ..
make

# Run (default /dev/i2c-1)
./controller_discovery

# Run with specific I2C device
./controller_discovery /dev/i2c-0
```

## Commands

### Discovery

- `scan` - Scan I2C bus for CRUMBS devices

### Calculator

- `calculator add <a> <b>` - Add two numbers
- `calculator sub <a> <b>` - Subtract
- `calculator mul <a> <b>` - Multiply
- `calculator div <a> <b>` - Divide
- `calculator result` - Get last result
- `calculator history` - Show operation history

### LED

- `led set_all <mask>` - Set all LEDs (e.g., 0x0F for all on)
- `led set_one <idx> <state>` - Set single LED
- `led blink <idx> <enable> <period_ms>` - Configure blink
- `led get_state` - Get current LED state

### Servo

- `servo set_pos <idx> <angle>` - Set position (0–180°)
- `servo set_speed <idx> <speed>` - Set speed (0–20)
- `servo sweep <idx> <enable> <min> <max> <step>` - Configure sweep
- `servo get_pos` - Get current positions

## Example Session

```sh
lhwit> scan
Scanning for CRUMBS devices...
Found Calculator at 0x10 (CRUMBS 0.12.2, Module 1.0.0) - Compatible
Found LED Array at 0x20 (CRUMBS 0.12.2, Module 1.0.0) - Compatible
Found Servo Controller at 0x30 (CRUMBS 0.12.2, Module 1.0.0) - Compatible

Found 3 CRUMBS device(s):
  [0] Address 0x10, Type 0x03 (Calculator)
  [1] Address 0x20, Type 0x01 (LED)
  [2] Address 0x30, Type 0x02 (Servo)

lhwit> calculator add 42 8
OK: add(42, 8) sent. Use 'calculator result' to get answer.

lhwit> calculator result
Result: 50

lhwit> led set_all 0x0F
OK: LEDs set to 0x0F

lhwit> servo set_pos 0 90
OK: Servo 0 position set to 90°
```

## When to Use This Controller

**Use discovery controller when:**

- Testing with unknown device addresses
- Working with dynamic bus configurations
- Prototyping with multiple device setups
- Need to identify available devices
- Need automatic version compatibility checking

**Use manual controller when:**

- Addresses are fixed and known
- Production deployments
- Faster startup (no scan required)
- Simpler code for reference

**Version Compatibility:**

- Controller queries peripheral versions during scan via opcode 0x00
- Checks CRUMBS version >= 0.10.0 (encoded as 1000)
- Verifies module major version match, minor version peripheral >= controller
- Incompatible devices are blocked from commands with clear error messages
- See [protocol.md](../../../docs/protocol.md#versioning-convention) for compatibility rules

## Requirements

- Linux with I2C support (Raspberry Pi, etc.)
- libi2c-dev installed
- User permissions for I2C access (or sudo)
