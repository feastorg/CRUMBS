# Protocol Specification

**CRUMBS Protocol v0.10**  
Variable-length I²C messaging with CRC-8 validation

---

## Wire Format

```text
┌──────────┬──────────┬──────────┬─────────────────┬──────────┐
│  type_id │  opcode  │ data_len │   data[0..N]    │   crc8   │
│ (1 byte) │ (1 byte) │ (1 byte) │ (0–27 bytes)    │ (1 byte) │
└──────────┴──────────┴──────────┴─────────────────┴──────────┘
```

**Message size:** 4–31 bytes  
**Maximum payload:** 27 bytes (constrained by Arduino Wire's 32-byte buffer)

---

## Field Specifications

| Field      | Size       | Range         | Available | Description                      |
| ---------- | ---------- | ------------- | --------- | -------------------------------- |
| `type_id`  | 1 byte     | `0x00`-`0xFF` | 255       | Device type; `0x00` = wildcard   |
| `opcode`   | 1 byte     | `0x01`-`0xFD` | 253       | Command identifier (per type_id) |
| `data_len` | 1 byte     | `0`-`27`      | 28        | Payload byte count               |
| `data[]`   | 0–27 bytes | N/A           | N/A       | Opaque payload                   |
| `crc8`     | 1 byte     | N/A           | N/A       | CRC-8 (polynomial 0x07)          |

**Notes:**

- The I²C address is handled by the transport layer (not part of the CRUMBS frame)
- Maximum 31 bytes ensures Arduino Wire compatibility
- CRC covers `type_id`, `opcode`, `data_len`, and `data[]` (excludes `crc8` itself)
- Payload is opaque bytes; applications encode floats, ints, structs, etc. as needed

---

## Address Space

| Range         | Decimal | Status                           |
| ------------- | ------- | -------------------------------- |
| `0x00`–`0x07` | 0–7     | **Reserved** (I²C specification) |
| `0x08`–`0x77` | 8–119   | **Available** (112 addresses)    |
| `0x78`–`0x7F` | 120–127 | **Reserved** (I²C specification) |

---

## Type ID Space

| Range         | Decimal | Status                          |
| ------------- | ------- | ------------------------------- |
| `0x00`        | 0       | **Wildcard** (`CRUMBS_TYPE_ID_ANY`) — never a device type |
| `0x01`–`0xFF` | 1–255   | **Available** (255 type IDs)    |

**Semantics:**

- Type ID identifies device **class/type**, not individual device
- Multiple devices can share same type_id (distinguished by I²C address)
- Each type_id has independent opcode namespace
- A peripheral that has declared its type (`crumbs_set_type_id()`) drops any
  frame whose type_id is another non-zero value, before dispatch. A peripheral
  that has not declared one accepts every frame.
- `0x00` in a frame means "any type": it is never rejected by that check. The
  library's own SET_REPLY frames (from `CRUMBS_DEFINE_GET_OP` getters) and the
  non-strict scan probe carry `0x00`. Do not assign `0x00` to a device.

---

## Opcode Space

| Range         | Decimal | Status                          |
| ------------- | ------- | ------------------------------- |
| `0x00`        | 0       | **Convention** (version info)   |
| `0x01`–`0xFD` | 1–253   | **Available** (253 opcodes)     |
| `0xFE`        | 254     | **Reserved** (SET_REPLY)        |
| `0xFF`        | 255     | **Convention** (error response) |

### Opcode Allocation Convention

**Purpose:** Separate SET (commands) and GET (queries) operations to avoid reply ambiguity.

**Common strategies:** Each type_id chooses allocation based on device needs.

| Strategy           | SET Range     | GET Range     | Use Case                           |
| ------------------ | ------------- | ------------- | ---------------------------------- |
| **Balanced**       | `0x01`-`0x7F` | `0x80`-`0xFD` | General purpose (127 SET, 126 GET) |
| **Sensor-heavy**   | `0x01`-`0x1F` | `0x20`-`0xFD` | Many readings (31 SET, 222 GET)    |
| **Actuator-heavy** | `0x01`-`0xDF` | `0xE0`-`0xFD` | Many commands (223 SET, 30 GET)    |
| **Simple device**  | `0x01`-`0x0F` | `0x10`-`0x1F` | Minimal opcodes (15 SET, 16 GET)   |

**Example (balanced 0x80 split):**

```c
// LED controller
#define LED_OP_SET_ALL   0x01  // SET: Change all LEDs
#define LED_OP_BLINK     0x02  // SET: Configure blinking
#define LED_OP_GET_STATE 0x80  // GET: Query current state
```

---

## Reserved Opcodes

### Opcode 0xFE: SET_REPLY

The SET_REPLY command allows a controller to specify which data a peripheral should return on the next I²C read request.

#### Wire format

```text
┌──────────┬──────────┬──────────┬───────────────┬──────────┐
│  type_id │   0xFE   │   0x01   │ target_opcode │   crc8   │
│ (1 byte) │ (1 byte) │ (1 byte) │   (1 byte)    │ (1 byte) │
└──────────┴──────────┴──────────┴───────────────┴──────────┘
```

#### Workflow

```text
1. Controller → SET_REPLY(0x80) → Peripheral  # Request opcode 0x80
2. Library intercepts, stores ctx->requested_opcode = 0x80
3. Controller → I²C read request → Peripheral
4. Peripheral on_request() switches on ctx->requested_opcode
5. Controller ← Reply with opcode 0x80 data ← Peripheral
```

#### Properties

- SET_REPLY is NOT dispatched to user handlers or callbacks
- `type_id` in a SET_REPLY frame is `0x00` (wildcard; what the generated getters
  send) or the target's type; either passes the peripheral's type check. Any
  other non-zero value is dropped like any other frame
- `requested_opcode` persists until another SET_REPLY is received
- Initial value is `0x00` (by convention: device/version info)
- Empty payload is ignored (no change to requested_opcode)

### Opcode 0x00: Version Info Convention

By convention, opcode `0x00` should return device identification and version information.

#### Recommended payload format (5 bytes)

```text
┌─────────────────┬───────────────┬───────────────┬───────────────┐
│ CRUMBS_VERSION  │  module_major │  module_minor │  module_patch │
│    (2 bytes)    │    (1 byte)   │    (1 byte)   │    (1 byte)   │
└─────────────────┴───────────────┴───────────────┴───────────────┘
```

#### Example implementation

```c
void on_request(crumbs_context_t *ctx, crumbs_message_t *reply) {
    switch (ctx->requested_opcode) {
        case 0x00:  // Version info
            crumbs_msg_init(reply, MY_TYPE_ID, 0x00);
            crumbs_msg_add_u16(reply, CRUMBS_VERSION);  // Library version
            crumbs_msg_add_u8(reply, 1);  // Module major version
            crumbs_msg_add_u8(reply, 0);  // Module minor version
            crumbs_msg_add_u8(reply, 0);  // Module patch version
            break;
        // ... other opcodes ...
    }
}
```

**Benefits:**

- Controllers can identify device types during bus scan
- Version compatibility checking
- Debugging aid

---

## Versioning

**Library version:** `CRUMBS_VERSION` macro (integer: major*10000 + minor*100 + patch)

**Module compatibility:** Follow [Semantic Versioning](https://semver.org/):

- **MAJOR**: Incompatible protocol changes (must match)
- **MINOR**: New commands added (peripheral >= controller required)
- **PATCH**: Bug fixes (no compatibility impact)

**Version check example:**

```c
if (crumbs_version < 1003) {  // Require >= 0.10.3
    fprintf(stderr, "CRUMBS library too old\n");
}
```

---

## CRC-8 Specification

| Property   | Value                                     |
| ---------- | ----------------------------------------- |
| Polynomial | `0x07` (x^8 + x^2 + x + 1)                |
| Initial    | `0x00`                                    |
| Algorithm  | Nibble-based (4-bit chunks)               |
| Coverage   | `type_id`, `opcode`, `data_len`, `data[]` |
| Excluded   | `crc8` field itself                       |

Implementation: `crc8_nibble_calculate()` from `src/crc/crc8_nibble.c`

---

## Communication Patterns

```text
Controller → [4–31 byte message] → Peripheral    # Send command (SET)
Controller → [SET_REPLY + target] → Peripheral   # Request specific data
Controller → [I²C read request] → Peripheral     # Read staged reply
Controller ← [4–31 byte response] ← Peripheral   # Receive data (GET)
```

---

## Timing

| Parameter       | Value        | Notes                                |
| --------------- | ------------ | ------------------------------------ |
| Message spacing | 10ms min     | delay() between send/read            |
| Read timeout    | 50ms typical | Processing time                      |
| I²C clock       | 100kHz       | Use 50kHz if CRC errors              |
| Bus length      | <30cm        | Longer needs lower clock/termination |

**Critical:** `delay(10)` between send and read:

```c
crumbs_controller_send(&ctx, 0x08, &msg, write_fn, NULL);
delay(10);
crumbs_arduino_read(NULL, 0x08, buf, sizeof(buf), 5000);
```

---

## Example: Encoding Data Types

### Float

```c
crumbs_message_t msg;
crumbs_msg_init(&msg, 0x01, 0x01);
crumbs_msg_add_float(&msg, 25.5f);
```

### Multiple values

```c
crumbs_msg_init(&msg, 0x01, 0x02);
crumbs_msg_add_u16(&msg, 1234);    // 2 bytes
crumbs_msg_add_u8(&msg, 0xAB);     // 1 byte
crumbs_msg_add_float(&msg, 3.14f); // 4 bytes
// Total: 7 bytes payload
```

---

## See Also

- [api-reference.md](api-reference.md) - Complete API documentation including message helpers
