# CRUMBS Documentation

I²C messaging library for controller/peripheral communication with CRC validation.

## Start Here

1. [Platform Setup](platform-setup.md) → PlatformIO
2. Test I²C: [scanner](https://playground.arduino.cc/Main/I2cScanner/)
3. Upload [hello examples](../examples/core_usage/arduino/)
4. Read [Protocol](protocol.md) and [examples](examples.md)

Issues? [Troubleshooting](../README.md#troubleshooting)

---

## Quick Start

```c
// Controller (Arduino HAL)
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

crumbs_context_t ctx;
crumbs_arduino_init_controller(&ctx);

crumbs_message_t msg;
crumbs_msg_init(&msg, 0x01, 0x01);  // type_id=1, opcode=1
crumbs_msg_add_u8(&msg, 1);         // payload: LED ON

crumbs_controller_send(&ctx, 0x08, &msg, crumbs_arduino_wire_write, NULL);
```

```c
// Peripheral (Arduino HAL)
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

crumbs_context_t ctx;
static uint8_t g_state = 0;

static void reply_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *u)
{
    (void)ctx; (void)u;
    crumbs_build_version_reply(reply, 0x01, 1, 0, 0);
}

static void reply_get_state(crumbs_context_t *ctx, crumbs_message_t *reply, void *u)
{
    (void)ctx; (void)u;
    crumbs_msg_init(reply, 0x01, 0x80);
    crumbs_msg_add_u8(reply, g_state);
}

void on_message(crumbs_context_t *ctx, const crumbs_message_t *msg) {
    if (msg->opcode == 0x01 && msg->data_len >= 1)
        g_state = msg->data[0];
}

void setup() {
    crumbs_arduino_init_peripheral(&ctx, 0x08);
    crumbs_set_callbacks(&ctx, on_message, NULL, NULL);
    crumbs_register_reply_handler(&ctx, 0x00, reply_version,   NULL);
    crumbs_register_reply_handler(&ctx, 0x80, reply_get_state, NULL);
}
```

## Features

- **Variable-length payload** (0–27 bytes, 4–31 total frame)
- **Controller/peripheral** (one controller, up to 112 devices)
- **Handler dispatch** (per-opcode SET handlers via `crumbs_register_handler`)
- **Reply handler dispatch** (per-opcode GET handlers via `crumbs_register_reply_handler`)
- **Message helpers** (type-safe: u8, u16, u32, i32, float)
- **Event callbacks** (`on_message`, `on_request` fallback)
- **CRC-8 integrity** (auto validation, error stats)
- **Discovery** (scan for compatible devices)
- **Bound-device handle** (`crumbs_device_t` groups transport fields per device)
- **Platforms** (Arduino, PlatformIO, Linux)
- **Zero allocation** (deterministic, RTOS-safe)

---

## Core Documentation

### Getting Started

| Document                              | Description                                                       |
| ------------------------------------- | ----------------------------------------------------------------- |
| [Platform Setup](platform-setup.md)   | Installation and configuration for Arduino, PlatformIO, and Linux |
| [Protocol Specification](protocol.md) | Wire format, versioning, CRC-8, reserved opcodes                  |
| [Examples](examples.md)               | Three-tier learning path with platform coverage                   |

### Reference

| Document                              | Description                                                      |
| ------------------------------------- | ---------------------------------------------------------------- |
| [API Reference](api-reference.md)     | Complete C API, handler dispatch, message helpers, platform HALs |
| [Architecture](architecture.md)       | Design philosophy, stakeholder roles, system architecture        |
| [LHWIT Family](lhwit-family.md)       | Reference implementation (LEDs, servos, calculator, display)     |
| [Create a Family](create-a-family.md) | Step-by-step guide for authoring custom device families          |

### Developer

| Document                                          | Description                                  |
| ------------------------------------------------- | -------------------------------------------- |
| [CONTRIBUTING.md](../CONTRIBUTING.md)             | How to build, test, and contribute to CRUMBS |
| [Character Usage Guide](character-usage-guide.md) | ASCII vs Unicode in documentation            |
| [Doxygen Style Guide](doxygen-style-guide.md)     | In-source documentation standards            |

### Meta

| Document                        | Description                            |
| ------------------------------- | -------------------------------------- |
| [PlatformIO](platformio.md)     | Registry publishing and CI integration |
| [CHANGELOG.md](../CHANGELOG.md) | Version history and release notes      |

---

## Architecture Overview

**CRUMBS is a transport layer protocol.** It handles framing, CRC, and routing but does not define specific commands. Instead, developers create **module families**—collections of device types with shared headers defining type identifiers and command vocabularies.

**Key Constraint:** A single I²C bus uses one module family. The controller is compiled with that family's headers and only understands those type_ids and opcodes.

**Stakeholders:**

- **End Users** — Operate systems built on CRUMBS (indirect)
- **System Integrators** — Build controller applications (primary)
- **Module Developers** — Implement peripheral firmware (application layer)
- **Library Maintainers** — Develop the core library (platform layer)

See [Architecture](architecture.md) for complete design documentation.

---

## Quick Reference

### Message Structure

```c
typedef struct {
    uint8_t type_id;      // Device/module type identifier
    uint8_t opcode;       // Command/query opcode
    uint8_t data_len;     // Payload length (0–27)
    uint8_t data[27];     // Payload buffer
    uint8_t crc8;         // CRC-8 checksum
} crumbs_message_t;
```

**Wire format:** `[type_id:1][opcode:1][data_len:1][data:0–27][crc8:1]` (4–31 bytes)

### Core Functions

```c
// Initialization
void crumbs_init(crumbs_context_t *ctx, crumbs_role_t role, uint8_t address);
void crumbs_set_callbacks(crumbs_context_t *ctx,
                          crumbs_message_cb_t on_message,
                          crumbs_request_cb_t on_request,
                          void *user_data);

// Controller operations
int crumbs_controller_send(const crumbs_context_t *ctx,
                           uint8_t target_addr,
                           const crumbs_message_t *msg,
                           crumbs_i2c_write_fn write_fn,
                           void *write_ctx);

// Peripheral operations
int crumbs_peripheral_handle_receive(crumbs_context_t *ctx,
                                     const uint8_t *buffer,
                                     size_t len);
int crumbs_peripheral_build_reply(crumbs_context_t *ctx,
                                  uint8_t *out_buf,
                                  size_t out_buf_len,
                                  size_t *out_len);

// Handler dispatch (SET ops)
int crumbs_register_handler(crumbs_context_t *ctx,
                            uint8_t opcode,
                            crumbs_handler_fn fn,
                            void *user_data);

// Reply handler dispatch (GET ops)
int crumbs_register_reply_handler(crumbs_context_t *ctx,
                                  uint8_t opcode,
                                  crumbs_reply_fn fn,
                                  void *user_data);

// Controller read (symmetric counterpart to crumbs_controller_send)
int crumbs_controller_read(crumbs_context_t *ctx,
                           uint8_t target_addr,
                           crumbs_message_t *out_msg,
                           crumbs_i2c_read_fn read_fn,
                           void *read_ctx);
```

### Message Helpers

```c
#include <crumbs_message_helpers.h>

// Building messages
crumbs_msg_init(&msg, type_id, opcode);
crumbs_msg_add_u8(&msg, value);
crumbs_msg_add_u16(&msg, value);
crumbs_msg_add_u32(&msg, value);
crumbs_msg_add_float(&msg, value);

// Reading payloads
crumbs_msg_read_u8(data, len, offset, &out);
crumbs_msg_read_u16(data, len, offset, &out);
crumbs_msg_read_u32(data, len, offset, &out);
crumbs_msg_read_float(data, len, offset, &out);
```

---

## Archived Documentation

Historical and technical deep-dive documents are in [archive/](archive/):

- `developer-notes.md` — Historical design decisions
- `crc.md` — CRC-8 implementation details

---

**Version**: 0.12.3
**Author**: Cameron K. Brooks  
**Dependencies**: Wire library (Arduino), linux-wire (Linux HAL)  
**License**: AGPL-3.0-or-later
