# Architecture

This document explains the design, architecture, and implementation of CRUMBS. It covers the conceptual model, stakeholder roles, system architecture, and key design decisions.

## What is CRUMBS?

CRUMBS is a lightweight I²C messaging protocol enabling controllers (e.g., single-board computers like Raspberry Pi) to communicate with peripheral modules (e.g., microcontrollers like Arduino) over a shared bus. The library provides the wire protocol (encoding, CRC, routing) but remains **protocol-agnostic**—it does not define specific commands. Instead, developers create **module families**—collections of device types with shared headers defining type identifiers and command vocabularies.

**Key Principle**: CRUMBS is the transport layer. Module families define the application layer.

**Key Constraint**: A single CRUMBS controller instance uses one module-family vocabulary (type_ids/opcodes). A physical I²C bus may also host non-CRUMBS peripherals, but CRUMBS discovery should then use constrained candidate-address scans rather than broad range probing.

### Design Goals

CRUMBS was created to solve practical problems in distributed embedded systems:

- **Compact wire format**: 4–31 bytes per message (variable length)
- **Robust communication**: CRC-8 integrity checking for noisy I²C buses
- **Portable C core**: Easy to integrate on resource-constrained microcontrollers
- **Platform abstraction**: Thin HALs for Arduino (Wire) and Linux (linux-wire)
- **Zero dynamic allocation**: Suitable for embedded systems without heap
- **Practical discovery**: Find devices that actually speak CRUMBS, not just ACK

### Background

Built for the BREAD system where function cards (each with an MCU) connect to a supervising SBC (Raspberry Pi) via an I²C backplane. The system needed reliable structured data exchange across potentially noisy wiring with minimal overhead and easy MCU integration.

---

## Stakeholders

Four distinct roles interact with CRUMBS:

### End Users

Operate CRUMBS-enabled systems without awareness of the protocol. They interact with applications built on the infrastructure.  
_Role: Indirect stakeholders_

### System Integrators

Select modules, assign I²C addresses, deploy hardware, and build controller applications that orchestrate multiple peripherals. They are the primary consumers of the CRUMBS library.  
_Role: Primary stakeholders_

### Module Developers

Design and implement peripheral firmware that communicates via CRUMBS. They publish headers defining their module family's type*id and command vocabulary (opcodes).  
\_Role: Application-layer developers*

### Library Maintainers

Develop and maintain the CRUMBS core library itself, ensuring it remains portable, efficient, and protocol-agnostic.  
_Role: Platform/framework developers_

---

## System Architecture

### The Contract Model

CRUMBS uses a **compile-time contract** between controllers and peripherals. Module families define device types and command vocabularies in C headers that both sides share.

**Module Family Headers** define:

- Device types (type_id values)
- Command opcodes for each type
- Payload structures (documented, not enforced)

**Example: LHWIT Family** (reference implementation)

```c
// led_module.h
#define LED_TYPE_ID          0x01
#define LED_CMD_SET_ALL      0x01   // Payload: u8 bitmask
#define LED_CMD_SET_ONE      0x02   // Payload: u8 index, u8 state
#define LED_OP_GET_STATE     0x10   // Reply: u8 state

// servo_module.h
#define SERVO_TYPE_ID        0x02
#define SERVO_CMD_SET_POS    0x01   // Payload: u8 channel, u16 pulse_us
#define SERVO_OP_GET_POS     0x10   // Reply: u8 channel, u16 pulse_us

// calculator_module.h
#define CALC_TYPE_ID         0x03
#define CALC_CMD_ADD         0x01   // Payload: i32 a, i32 b
#define CALC_OP_GET_RESULT   0x10   // Reply: i32 result
```

### Compile-Time Vocabulary Binding

Controllers are compiled with the headers for their target family. This establishes:

- What module types exist (type_id space)
- What commands each type understands (opcode space per type)
- Expected payload formats

The same headers are used by peripheral firmware to implement handlers.

**Benefits:**

- Type safety: Compiler catches mismatches
- Self-documenting: Headers define the complete vocabulary
- Version control: Headers can be versioned with the family

**Trade-off:** A controller cannot speak multiple CRUMBS vocabularies at once. This is a vocabulary constraint, not a hard prohibition on non-CRUMBS devices sharing the same physical bus.

### Physical Deployment

**Bus topology:**

- Modules connect to shared I²C bus (SDA/SCL)
- Each peripheral at unique 7-bit address (0x08–0x77)
- Controller initiates all communication (I²C master)
- Pull-up resistors required (typically 4.7kΩ)

**Address assignment methods:**

- Firmware defaults (simple, inflexible)
- EEPROM configuration (flexible, requires setup)
- Hardware pins/DIP switches (visible, easy to verify)

### Discovery and Identification

Controllers use CRUMBS-aware scanning to discover devices on the bus:

**How Discovery Works:**

1. **Bus scan**: Controller probes each address (0x08–0x77)
2. **Read attempt**: Tries to read a CRUMBS frame from each address
3. **Validation**: Decodes message and validates CRC
4. **Type extraction**: Valid response contains type_id in frame
5. **Mapping**: Controller builds device map `{address → type_id}`

**Key insight**: No special IDENTIFY command needed. Any valid CRUMBS response reveals the module's type_id.

**Implementation:**

```c
// Basic scan (addresses only)
uint8_t addrs[16];
int count = crumbs_controller_scan_for_crumbs(
    &ctx, 0x08, 0x77, 1,  // strict mode
    write_fn, read_fn, io_ctx,
    addrs, 16, 10000);

// Enhanced scan (addresses + types, v0.10.1+)
uint8_t addrs[16], types[16];
int count = crumbs_controller_scan_for_crumbs_with_types(
    &ctx, 0x08, 0x77, 0,
    write_fn, read_fn, io_ctx,
    addrs, types, 16, 10000);

// Build runtime map
for (int i = 0; i < count; i++) {
    printf("0x%02X → type 0x%02X\n", addrs[i], types[i]);
}
```

**Scan modes:**

- **Strict mode** (`strict=1`): Read-only probes. Nothing is written, but each probe clocks up to 31 bytes out of every address, which advances a register-addressed device's pointer and consumes a stream device's pending response.
- **Non-strict mode** (`strict=0`): Also sends the probe frame `00 00 00 00` to stimulate a response — a page write at address 0 to a 24Cxx-style EEPROM. Do not use on a bus that may carry one.

On mixed buses (CRUMBS + non-CRUMBS), avoid broad address-range scans and prefer explicit candidate address lists.

**Runtime device map example:**

```
0x20 → LED (type 0x01)
0x21 → LED (type 0x01)
0x30 → SERVO (type 0x02)
0x40 → CALCULATOR (type 0x03)
```

Controller now knows "two LED modules, one servo, and one calculator" and can address each appropriately.

### Message Flow

**Controller → Peripheral (Command):**

```c
// Send servo command to address 0x30
crumbs_message_t msg;
crumbs_msg_init(&msg, SERVO_TYPE_ID, SERVO_CMD_SET_POS);
crumbs_msg_add_u8(&msg, 0);      // channel 0
crumbs_msg_add_u16(&msg, 1500);  // 1500μs pulse
crumbs_controller_send(&ctx, 0x30, &msg, write_fn, io_ctx);
```

**Controller ← Peripheral (Query via SET_REPLY):**

CRUMBS reserves opcode `0xFE` for the SET_REPLY pattern:

1. Controller sends SET_REPLY specifying target opcode
2. Library stores target in `ctx->requested_opcode`
3. Controller issues I²C read
4. Peripheral's `on_request` callback switches on `requested_opcode`
5. Controller receives appropriate data

**Peripheral implementation:**

```c
void on_request(crumbs_context_t *ctx, crumbs_message_t *reply) {
    switch (ctx->requested_opcode) {
        case 0x00:  // Default: version info
            crumbs_msg_init(reply, MY_TYPE_ID, 0x00);
            crumbs_msg_add_u16(reply, CRUMBS_VERSION);
            crumbs_msg_add_u8(reply, MODULE_VER_MAJOR);
            crumbs_msg_add_u8(reply, MODULE_VER_MINOR);
            crumbs_msg_add_u8(reply, MODULE_VER_PATCH);
            break;

        case 0x10:  // Sensor reading
            crumbs_msg_init(reply, MY_TYPE_ID, 0x10);
            crumbs_msg_add_u16(reply, sensor_read());
            break;

        default:
            crumbs_msg_init(reply, MY_TYPE_ID, ctx->requested_opcode);
            break;
    }
}
```

**Controller usage:**

```c
// Request sensor data (opcode 0x10)
crumbs_message_t req, reply;
crumbs_msg_init(&req, 0, CRUMBS_CMD_SET_REPLY);  // 0xFE
crumbs_msg_add_u8(&req, 0x10);  // target opcode
crumbs_controller_send(&ctx, addr, &req, write_fn, io_ctx);

// Read response (portable, validates CRC)
crumbs_controller_read(&ctx, addr, &reply, read_fn, io_ctx);
```

### Multiple Module Instances

Multiple peripherals can share the same type_id (same command vocabulary) but operate independently at different addresses:

```
0x20 → LED (instance 1) ──┐
0x21 → LED (instance 2) ──┼── Share LED_CMD_SET_ALL vocabulary
0x22 → LED (instance 3) ──┘    but control different hardware
```

Controller can:

- Broadcast same command to all LEDs (loop over addresses)
- Control individual LEDs (send to specific address)
- Query each LED's state independently

---

## Core Architecture

### Layer Organization

**Transport Layer (CRUMBS Core):**

- Wire format encoding/decoding
- CRC-8 computation and validation
- Frame boundaries (4–31 bytes)
- Role management (controller/peripheral)

**Platform Layer (HALs):**

- Arduino HAL: Wire library integration
- Linux HAL: linux-wire integration
- I²C primitives: write, read, scan

**Application Layer (Module Families):**

- Type ID space allocation
- Opcode vocabulary definition
- Payload structure documentation
- Version management

### Wire Format

**Serialized frame** (4–31 bytes):

```
[type_id:1][opcode:1][data_len:1][data:0–27][crc8:1]
```

**Key properties:**

- Variable length: Efficient for short messages, bounded for buffer sizing
- Raw payload: Application defines encoding (no mandated structure)
- CRC over entire frame: Detects corruption in header and payload
- Little-endian integers: Standard in message helpers (crumbs_msg_add_u16, etc.)

**CRC-8 specification:**

- Polynomial: 0x07 (x^8 + x^2 + x + 1)
- Initial value: 0x00
- Input: type_id + opcode + data_len + data[0..data_len-1]
- Output: 8-bit checksum appended to frame

**Design rationale:**

- Variable length avoids padding overhead
- Single-byte CRC sufficient for short I²C messages (typical bus length < 1m)
- Raw payloads give application full control (floats, structs, text)
- Bounded maximum (31 bytes) allows fixed buffer allocation

### Role Definitions

**Controller (Master):**

- Initiates all communication
- Sends commands via `crumbs_controller_send()`
- Requests data via I²C read operations
- Discovers devices via scanning
- Typically runs on SBC (Raspberry Pi, PC)

**Peripheral (Target/Slave):**

- Responds to controller requests
- Receives commands via I²C writes (triggers `on_message` callback)
- Provides data via I²C reads (triggers `on_request` callback)
- Implements command handlers
- Typically runs on MCU (Arduino, STM32)

**Note:** Linux HAL currently supports controller mode only (peripheral requires kernel I²C target support).

### Command Handler Dispatch

CRUMBS provides two patterns for processing incoming messages:

**Pattern 1: General callback**

```c
void on_message(crumbs_context_t *ctx, const crumbs_message_t *msg) {
    switch (msg->opcode) {
        case 0x01: /* ... */ break;
        case 0x02: /* ... */ break;
    }
}
```

**Pattern 2: Per-command handlers** (preferred for complex peripherals)

```c
void handle_led_set(crumbs_context_t *ctx, uint8_t cmd,
                    const uint8_t *data, uint8_t len, void *user) {
    // Process LED command
}

void handle_servo_set(crumbs_context_t *ctx, uint8_t cmd,
                      const uint8_t *data, uint8_t len, void *user) {
    // Process servo command
}

void setup() {
    crumbs_register_handler(&ctx, 0x01, handle_led_set, NULL);
    crumbs_register_handler(&ctx, 0x02, handle_servo_set, NULL);
}
```

**Dispatch flow:**

1. Message decoded, CRC validated
2. Type check: a declared type (`crumbs_set_type_id()`) drops frames for any other non-zero type
3. SET_REPLY (`0xFE`) intercepted; not dispatched further
4. `on_message` callback invoked (if registered)
5. Handler lookup by opcode
6. Handler invoked (if registered)

**Benefits:**

- Cleaner code organization (one function per command)
- Per-handler user_data (state separation)
- Easy to add/remove commands
- Can combine both patterns (callback for logging, handlers for dispatch)

**Implementation:**

- O(n) linear search through handler table
- Default: 16 handlers (~68 bytes on AVR, ~132 bytes on 32-bit)
- Configurable via `CRUMBS_MAX_HANDLERS` build flag
- Set to 0 to disable entirely (use callbacks only)

---

## Design Philosophy

### Protocol-Agnostic Core

CRUMBS does **not** enforce specific type_ids or opcodes (except reserved 0xFE for SET_REPLY). The library handles:

- Framing and encoding
- CRC computation and validation
- Transport mechanics

Application semantics are defined by module families. This makes CRUMBS:

- Reusable across different systems
- Simple to understand (small responsibility)
- Easy to test (no application logic in core)

### One Family Per Bus

A controller is compiled for one module family and only understands that family's vocabulary. You cannot mix modules from different families on the same bus.

**Why this constraint?**

- Type safety: Compiler verifies command usage against headers
- Simplicity: No runtime type negotiation or vocabulary discovery
- Performance: No dynamic dispatch overhead
- Clarity: Bus purpose is clear from controller code

**Consequence:** Multi-family systems require multiple I²C buses or gateway translation.

### Compile-Time Safety, Runtime Flexibility

**Compile-time:** Shared headers provide type safety and documentation. Controller and peripheral agree on payload formats at compile time.

**Runtime:** Controllers discover what's physically connected and adapt. Adding a new module requires:

- No controller code changes
- No recompilation
- Just plug it in at an unused address

This balance enables:

- Safe command usage (compiler-checked)
- Flexible deployment (runtime discovery)
- Easy system expansion (hot-plug capable)

### Reference Implementation

The CRUMBS repository includes the **LHWIT family** (LEDs, servos, calculator, 7-segment display) as a working example. This serves multiple purposes:

**For users:**

- Study well-documented examples
- Copy patterns for their own families
- Test integration without custom hardware

**For maintainers:**

- Regression testing (known-good family)
- Hardware validation (real I²C testing)
- API demonstration (shows intended usage)
- Performance baseline (memory/timing benchmarks)

The LHWIT family is intentionally simple (4 module types, basic commands) to remain approachable while demonstrating key patterns.

---

## Implementation Details

### Memory Management

**No dynamic allocation:** All buffers and structures are stack-allocated or static. This ensures:

- Deterministic memory usage
- No heap fragmentation
- No malloc/free overhead
- Safe for RTOS environments

**Buffer sizing:**

- `crumbs_message_t`: 31 bytes (fixed maximum)
- `crumbs_context_t`: ~100–200 bytes depending on handler configuration
- Handler table: configurable (0–255 entries)

### CRC Implementation

Multiple CRC-8 implementations provided for different platforms:

- **Nibble table** (default): 16-byte table, good balance
- **Full table**: 256-byte table, fastest but large
- **Bitwise**: No table, slowest but minimal memory

Generated via `scripts/generate_crc8.py`. Selected variant compiled into library based on platform needs.

### HAL Abstraction

**Core primitives defined in `crumbs_i2c.h`:**

```c
// Write primitive (controller → peripheral)
typedef int (*crumbs_i2c_write_fn)(
    void *user_ctx,
    uint8_t target_addr,
    const uint8_t *data,
    size_t len);

// Read primitive (controller ← peripheral)
typedef int (*crumbs_i2c_read_fn)(
    void *user_ctx,
    uint8_t target_addr,
    uint8_t *buffer,
    size_t len,
    uint32_t timeout_us);
```

**Platform implementations:**

- **Arduino:** Maps to Wire.beginTransmission/write/endTransmission and requestFrom
- **Linux:** Maps to linux-wire ioctl-based I²C operations

**Design benefits:**

- Core remains platform-agnostic (pure C)
- HALs can use platform idioms (Arduino C++, Linux ioctl)
- Easy to add new platforms (implement two functions)
- Testable via mock implementations

### Discovery Implementation

**Two-layer scanning:**

1. **Generic scanner** (`crumbs_arduino_scan`, `crumbs_linux_scan`):
   - Address-level probing (address ACK, or a one-byte read in strict mode)
   - Finds any I²C device
   - Fast but non-specific

2. **CRUMBS-aware scanner** (`crumbs_controller_scan_for_crumbs`):
   - Read full frame from each address
   - Validate CRC and structure
   - Extract type_id
   - Only accepts valid CRUMBS devices

**Production hardening recommendations:**

- Implement explicit PING/PONG handshake (dedicated opcode)
- Validate type_id against known values
- Check version compatibility
- Retry failed reads (transient noise)

---

## Versioning and Compatibility

Module families should implement version reporting (opcode 0x00 by convention):

**Version payload format:**

```c
crumbs_msg_add_u16(reply, CRUMBS_VERSION);    // Library version
crumbs_msg_add_u8(reply, MODULE_VER_MAJOR);   // Family major
crumbs_msg_add_u8(reply, MODULE_VER_MINOR);   // Family minor
crumbs_msg_add_u8(reply, MODULE_VER_PATCH);   // Family patch
```

**Semantic versioning:**

- **MAJOR**: Breaking changes (incompatible commands/payloads)
- **MINOR**: New commands added (backward compatible)
- **PATCH**: Bugfixes only (no command changes)

**Compatibility checking:**

```c
// Query version
// ... send SET_REPLY(0x00), read response ...

if (reply_major != EXPECTED_MAJOR) {
    fprintf(stderr, "Incompatible module version!\n");
    return -1;
}
if (reply_minor < EXPECTED_MINOR) {
    fprintf(stderr, "Missing features (old firmware)\n");
    // Degrade gracefully
}
```

See [Protocol Specification](protocol.md) for detailed versioning convention.

---

## Future Directions

**Gateway Services:**
Higher-level services could expose module functionality over networks (REST, MQTT, WebSocket). The gateway:

- Maintains registry of connected devices
- Translates network requests to CRUMBS messages
- Abstracts I²C details from remote clients

Example: HTTP endpoint `/devices/servo-0x30/position` maps to `SERVO_CMD_SET_POS` message.

**Multi-Family Systems:**
Research middleware approaches for integrating multiple module families:

- Bridge controllers between buses
- Vocabulary translation layers
- Dynamic family discovery protocols

**Enhanced Discovery:**

- Standardized capability reporting (supported commands, parameter ranges)
- Hierarchical addressing (bus → controller → peripheral)
- Multicast addressing (group commands)

---

## See Also

- [Protocol Specification](protocol.md) — Wire format, CRC, versioning
- [API Reference](api-reference.md) — Complete function documentation
- [Platform Setup](platform-setup.md) — Installation and configuration
- [Examples](examples.md) — Working code for all platforms
- [LHWIT Family](lhwit-family.md) — Reference implementation
