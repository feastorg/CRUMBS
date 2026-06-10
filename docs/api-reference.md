# API Reference

This document provides the complete CRUMBS C API reference for users implementing controllers and peripherals. All public functions, types, constants, and helper utilities are documented here.

## Overview

The CRUMBS API is organized into:

- **Core API**: Message encoding/decoding, context management, callbacks
- **Handler Dispatch**: Per-command function registration for peripherals
- **Message Helpers**: Type-safe payload builders and readers
- **Platform HAL**: Arduino and Linux adapter functions
- **Discovery**: Bus scanning for CRUMBS-compatible devices

## Key Types and Constants

### Message Structure

```c
typedef struct {
    uint8_t type_id;      // Device/module type identifier
    uint8_t opcode;       // Command/query opcode
    uint8_t data_len;     // Payload length (0–27)
    uint8_t data[27];     // Payload buffer
    uint8_t crc8;         // CRC-8 checksum over serialized frame
} crumbs_message_t;
```

> `address` was removed in v0.10.3 — it was never serialized and was redundant with `ctx->address` inside every callback.

**Serialized frame format** (4–31 bytes):

```text
[type_id:1][opcode:1][data_len:1][data:0–27][crc8:1]
```

CRC is computed over: `type_id + opcode + data_len + data[0..data_len-1]`

### Context Structure

```c
typedef struct crumbs_context_t {
    crumbs_role_t role;           // CONTROLLER or PERIPHERAL
    uint8_t address;              // Device I²C address (peripherals only)

    // CRC statistics
    uint32_t crc_error_count;     // Cumulative CRC failures
    int last_crc_ok;              // 1 if last decode succeeded, 0 otherwise

    // Callbacks
    crumbs_message_cb_t on_message;    // General message callback
    crumbs_request_cb_t on_request;    // Peripheral request callback (for reads)
    void *user_data;                   // Opaque user pointer

    // Handler dispatch table (if enabled)
    // ... internal fields ...
} crumbs_context_t;
```

### Constants

```c
#define CRUMBS_MAX_PAYLOAD      27    // Maximum payload bytes
#define CRUMBS_MESSAGE_MAX_SIZE 31    // Maximum serialized frame size
#define CRUMBS_CMD_SET_REPLY    0xFE  // Reserved opcode for SET_REPLY command
#define CRUMBS_VERSION          1203  // Library version (1203 = v0.12.3, formula: major*10000 + minor*100 + patch)
```

### Bound-Device Handle

```c
typedef struct {
    crumbs_context_t   *ctx;      // Shared controller context
    uint8_t             addr;     // 7-bit I²C address of this device
    crumbs_i2c_write_fn write_fn; // I²C write callback
    crumbs_i2c_read_fn  read_fn;  // I²C read callback (NULL if no GET ops)
    crumbs_delay_fn     delay_fn; // Microsecond delay callback (NULL if no GET ops)
    void               *io;       // Platform I/O context (Wire*, linux handle, etc.)
} crumbs_device_t;
```

Bundle all per-device transport state into one value that ops-header functions accept as `const crumbs_device_t *dev`. Populate once at startup (or after scan) and reuse for every call to that device. See `examples/families_usage/lhwit_family/` for usage.

### Callback Signatures

```c
// Message callback — invoked on all received messages
typedef void (*crumbs_message_cb_t)(
    crumbs_context_t *ctx,
    const crumbs_message_t *msg);

// Request callback — invoked when peripheral receives an I²C read request
// (fallback; prefer crumbs_register_reply_handler for per-opcode dispatch)
typedef void (*crumbs_request_cb_t)(
    crumbs_context_t *ctx,
    crumbs_message_t *reply);

// SET handler — per-opcode dispatch for incoming SET commands
typedef void (*crumbs_handler_fn)(
    crumbs_context_t *ctx,
    uint8_t opcode,
    const uint8_t *data,
    uint8_t data_len,
    void *user_data);

// Reply handler — per-opcode dispatch for GET reply building
typedef void (*crumbs_reply_fn)(
    crumbs_context_t *ctx,
    crumbs_message_t *reply,
    void *user_data);
```

---

## Core API

### Context Initialization

```c
void crumbs_init(crumbs_context_t *ctx, crumbs_role_t role, uint8_t address);
```

Initialize a CRUMBS context. Must be called before any other operations.

**Parameters:**

- `ctx` — Context to initialize
- `role` — `CRUMBS_ROLE_CONTROLLER` or `CRUMBS_ROLE_PERIPHERAL`
- `address` — Device I²C address (only used for peripherals; controllers can use 0x00)

### Callback Registration

```c
void crumbs_set_callbacks(crumbs_context_t *ctx,
                          crumbs_message_cb_t on_message,
                          crumbs_request_cb_t on_request,
                          void *user_data);
```

Install callbacks for message handling.

**Parameters:**

- `on_message` — Invoked when a message is received (both roles). May be NULL.
- `on_request` — Invoked when peripheral receives an I²C read request. May be NULL.
- `user_data` — Opaque pointer passed to callbacks.

**Callback Execution Order:**

1. Message decoded and CRC validated
2. `on_message` callback invoked (if registered)
3. Handler dispatch (if registered for this opcode)

### Encoding and Decoding

```c
size_t crumbs_encode_message(const crumbs_message_t *msg,
                             uint8_t *buffer,
                             size_t buffer_len);
```

Encode a message into a wire-format frame.

**Returns:**

- Encoded frame length (4 + data_len) on success
- `0` on failure (buffer too small or data_len > 27)

---

```c
int crumbs_decode_message(const uint8_t *buffer,
                          size_t buffer_len,
                          crumbs_message_t *msg,
                          crumbs_context_t *ctx);
```

Decode a wire-format frame into a message structure.

**Parameters:**

- `ctx` — Optional context for CRC statistics (may be NULL)

**Returns:**

- `0` — Success
- `-1` — Invalid frame (too short, too long, bad data_len, truncated)
- `-2` — CRC mismatch

If `ctx` is non-NULL, CRC statistics are updated.

### Controller Operations

```c
int crumbs_controller_send(const crumbs_context_t *ctx,
                           uint8_t target_addr,
                           const crumbs_message_t *msg,
                           crumbs_i2c_write_fn write_fn,
                           void *write_ctx);
```

Send a message from controller to a peripheral device.

**Returns:**

- `0` — Success
- `-1` — Invalid arguments (NULL ctx/msg/write_fn)
- `-2` — Context not in controller role
- `-3` — Encode failed (data_len > 27 or buffer issue)
- `>0` — I2C write error (platform-specific, from write_fn)

**Common issues:**

- Return `-3`: Check `msg->data_len` ≤ 27
- Return `>0`: I²C bus error - check wiring, pull-ups, address
- No response from peripheral: Add 10ms delay before reading

**Example:**

```c
crumbs_message_t msg;
crumbs_msg_init(&msg, 0x01, 0x10);  // type=LED, opcode=query
int rc = crumbs_controller_send(&ctx, 0x08, &msg,
                                crumbs_arduino_wire_write, NULL);
if (rc != 0) {
    Serial.print("Send failed: ");
    Serial.println(rc);  // Debug error code
}
```

### Peripheral Operations

```c
int crumbs_peripheral_handle_receive(crumbs_context_t *ctx,
                                     const uint8_t *buffer,
                                     size_t len);
```

Process incoming data on a peripheral device. Decodes the message, validates CRC, and invokes callbacks/handlers.

**Returns:** `0`=success, `-1`=invalid/decode fail, `-2`=CRC error (check wiring, use `crumbs_get_crc_error_count()`)

Called from Wire `onReceive()` on Arduino.

---

```c
int crumbs_peripheral_build_reply(crumbs_context_t *ctx,
                                  uint8_t *out_buf,
                                  size_t out_buf_len,
                                  size_t *out_len);
```

Build a reply frame for an I²C read request. Dispatches in order:

1. Per-opcode reply handler table (`crumbs_register_reply_handler`) — checked first
2. `on_request` callback fallback — called only when no matching reply handler exists
3. Returns 0-length reply if neither is configured

**Returns:**

- `0` — Success (`out_len` set to frame size, or 0 if no reply configured)
- `-1` — Invalid arguments or not peripheral role
- `-2` — Encode failed

Typically called from Wire `onRequest()` on Arduino.

### CRC Statistics

```c
uint32_t crumbs_get_crc_error_count(const crumbs_context_t *ctx);
int crumbs_last_crc_ok(const crumbs_context_t *ctx);
void crumbs_reset_crc_stats(crumbs_context_t *ctx);
```

Access CRC validation statistics. Use these for diagnostics or to detect noisy I²C bus conditions.

### ABI Compatibility Check

```c
size_t crumbs_context_size(void);
```

Returns the size of `crumbs_context_t` as compiled into the library. Use this to detect mismatches when `CRUMBS_MAX_HANDLERS` differs between library and application.

**When to use:** Precompiled libraries (PlatformIO lib_deps)

**Example:**

```c
void setup() {
    if (sizeof(crumbs_context_t) != crumbs_context_size()) {
        Serial.println("ERROR: CRUMBS_MAX_HANDLERS mismatch!");
        while(1);  // Halt
    }
}
```

**Fix:** Set `CRUMBS_MAX_HANDLERS` via `build_flags` in `platformio.ini`.

---

## Handler Dispatch

The handler dispatch system provides per-opcode function registration for structured command processing.

### Handler Registration

```c
int crumbs_register_handler(crumbs_context_t *ctx,
                            uint8_t opcode,
                            crumbs_handler_fn fn,
                            void *user_data);
```

Register a handler function for a specific opcode.

**Returns:**

- `0` — Success
- `-1` — NULL context or handler table full

**Example:**

```c
crumbs_register_handler(&ctx, 0x01, handle_led_set, &led_state);
crumbs_register_handler(&ctx, 0x10, handle_led_query, NULL);
```

---

```c
int crumbs_unregister_handler(crumbs_context_t *ctx, uint8_t opcode);
```

Remove a handler for a specific opcode. Always returns `0`.

Alternatively, call `crumbs_register_handler()` with `fn = NULL`.

### Handler Function Signature

```c
void my_handler(crumbs_context_t *ctx,
                uint8_t opcode,
                const uint8_t *data,
                uint8_t data_len,
                void *user_data) {
    // Process command...
}
```

**Parameters:**

- `ctx` — The CRUMBS context
- `opcode` — The command that triggered this handler
- `data` — Payload bytes (may be NULL if data_len == 0)
- `data_len` — Payload length (0–27)
- `user_data` — Opaque pointer registered with this handler

---

```c
int crumbs_register_reply_handler(crumbs_context_t *ctx,
                                  uint8_t opcode,
                                  crumbs_reply_fn fn,
                                  void *user_data);
```

Register a reply-builder function for a specific GET opcode. Invoked by `crumbs_peripheral_build_reply()` when `ctx->requested_opcode` matches — symmetric counterpart to `crumbs_register_handler()` for SET ops.

**Returns:** `0` on success, `-1` if ctx is NULL or handler table is full.

When both a reply handler and `on_request` are configured, the reply handler takes priority; `on_request` is called only when no matching reply handler is found (fully backward-compatible).

**Example:**

```c
static void reply_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, MY_TYPE_ID, 1, 0, 0);
}

static void reply_get_state(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, MY_TYPE_ID, MY_OP_GET_STATE);
    crumbs_msg_add_u8(reply, g_state);
}

// In setup():
crumbs_register_reply_handler(&ctx, 0x00,             reply_version,   NULL);
crumbs_register_reply_handler(&ctx, MY_OP_GET_STATE,  reply_get_state, NULL);
```

### SET Dispatch Flow

When a SET message arrives at a peripheral:

1. Message decoded and CRC validated
2. `on_message` callback invoked (if registered)
3. Handler dispatch searches for matching opcode
4. Handler invoked (if found)

Both mechanisms coexist — use `on_message` for logging, handlers for command logic.

### GET Dispatch Flow

When the bus master issues an I²C read request:

1. `crumbs_peripheral_build_reply()` called by the HAL
2. Reply handler table searched for `ctx->requested_opcode`
3. If found: corresponding `crumbs_reply_fn` called
4. If not found: `on_request` callback called (backward-compatible fallback)
5. If neither configured: reply length set to 0 (empty response)

### Memory Configuration

Handler dispatch uses `CRUMBS_MAX_HANDLERS` slots (default: 16).

**Adjust handler table size:**

```ini
# platformio.ini
build_flags = -DCRUMBS_MAX_HANDLERS=8
```

**Disable handler dispatch:**

```ini
build_flags = -DCRUMBS_MAX_HANDLERS=0
```

> **Important:** Always set via `build_flags`. Defining in sketch before `#include` does not work on Arduino/PlatformIO due to separate library compilation.

**Memory usage:**

| Max Handlers | AVR (2-byte ptr) | 32-bit (4-byte ptr) |
| ------------ | ---------------- | ------------------- |
| 16           | ~68 bytes        | ~132 bytes          |
| 8            | ~36 bytes        | ~68 bytes           |
| 4            | ~21 bytes        | ~37 bytes           |
| 0            | 0 bytes          | 0 bytes             |

---

## Message Helpers

The `crumbs_message_helpers.h` header provides zero-overhead inline helpers for building and reading message payloads. These eliminate manual byte manipulation.

**Include:**

```c
#include "crumbs_message_helpers.h"    // Linux/CMake
#include <crumbs_message_helpers.h>    // Arduino
```

### Message Builder

#### Initialization

```c
void crumbs_msg_init(crumbs_message_t *msg, uint8_t type_id, uint8_t opcode);
```

Initialize a message with type and opcode. Sets `data_len = 0` and zeros all fields.

#### Adding Values

All add functions return `0` on success, `-1` if the value would exceed the 27-byte payload limit.

```c
int crumbs_msg_add_u8(crumbs_message_t *msg, uint8_t value);
int crumbs_msg_add_u16(crumbs_message_t *msg, uint16_t value);     // little-endian
int crumbs_msg_add_u32(crumbs_message_t *msg, uint32_t value);     // little-endian
int crumbs_msg_add_i8(crumbs_message_t *msg, int8_t value);
int crumbs_msg_add_i16(crumbs_message_t *msg, int16_t value);      // little-endian
int crumbs_msg_add_i32(crumbs_message_t *msg, int32_t value);      // little-endian
int crumbs_msg_add_float(crumbs_message_t *msg, float value);      // native byte order
int crumbs_msg_add_bytes(crumbs_message_t *msg, const void *data, uint8_t len);
```

**Encoding notes:**

- Multi-byte integers use **little-endian** (LSB first)
- Floats use **native byte order** (memcpy)

**Example:**

```c
crumbs_message_t msg;
crumbs_msg_init(&msg, 0x02, 0x02);  // type=Servo, opcode=SetBoth
crumbs_msg_add_u16(&msg, 1500);     // Servo 1
crumbs_msg_add_u16(&msg, 2000);     // Servo 2
```

### Message Reader

All read functions return `0` on success, `-1` if reading would exceed buffer bounds.

```c
int crumbs_msg_read_u8(const uint8_t *data, uint8_t len, uint8_t offset, uint8_t *out);
int crumbs_msg_read_u16(const uint8_t *data, uint8_t len, uint8_t offset, uint16_t *out);
int crumbs_msg_read_u32(const uint8_t *data, uint8_t len, uint8_t offset, uint32_t *out);
int crumbs_msg_read_i8(const uint8_t *data, uint8_t len, uint8_t offset, int8_t *out);
int crumbs_msg_read_i16(const uint8_t *data, uint8_t len, uint8_t offset, int16_t *out);
int crumbs_msg_read_i32(const uint8_t *data, uint8_t len, uint8_t offset, int32_t *out);
int crumbs_msg_read_float(const uint8_t *data, uint8_t len, uint8_t offset, float *out);
int crumbs_msg_read_bytes(const uint8_t *data, uint8_t len, uint8_t offset, void *out, uint8_t count);
```

**Example (peripheral handler):**

```c
void handle_servo(crumbs_context_t *ctx, uint8_t cmd,
                  const uint8_t *data, uint8_t len, void *user) {
    uint8_t channel;
    uint16_t pulse_us;

    if (crumbs_msg_read_u8(data, len, 0, &channel) < 0) return;
    if (crumbs_msg_read_u16(data, len, 1, &pulse_us) < 0) return;

    set_servo(channel, pulse_us);
}
```

### Command Header Pattern

For reusable command definitions, create a shared header file. See `examples/handlers_usage/mock_ops.h` for a complete example.

**Pattern:**

```c
// my_device_commands.h
#ifndef MY_DEVICE_COMMANDS_H
#define MY_DEVICE_COMMANDS_H

#include "crumbs.h"
#include "crumbs_message_helpers.h"

#define MY_TYPE_ID        0x10
#define MY_CMD_ACTION_A   0x01
#define MY_CMD_ACTION_B   0x02

static inline int my_send_action_a(
    crumbs_context_t *ctx, uint8_t addr,
    uint16_t param1, uint8_t param2,
    crumbs_i2c_write_fn write_fn, void *write_ctx) {

    crumbs_message_t msg;
    crumbs_msg_init(&msg, MY_TYPE_ID, MY_CMD_ACTION_A);
    crumbs_msg_add_u16(&msg, param1);
    crumbs_msg_add_u8(&msg, param2);

    return crumbs_controller_send(ctx, addr, &msg, write_fn, write_ctx);
}

#endif
```

**Benefits:**

- Type-safe: Function parameters enforce correct types
- Self-documenting: Command names and parameters are explicit
- Reusable: Same header works on controller and peripheral
- Composable: Include multiple device headers in one controller

---

## Version Reply Helper

```c
void crumbs_build_version_reply(crumbs_message_t *reply,
                                uint8_t type_id,
                                uint8_t major,
                                uint8_t minor,
                                uint8_t patch);
```

Convenience helper declared in `crumbs_message_helpers.h`. Populates a standard opcode-0x00 version reply in one call: initialises the message and packs `[CRUMBS_VERSION:u16][major:u8][minor:u8][patch:u8]`.

**Example:**

```c
static void reply_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *u)
{
    (void)ctx; (void)u;
    crumbs_build_version_reply(reply, MY_TYPE_ID,
                               MY_VER_MAJOR, MY_VER_MINOR, MY_VER_PATCH);
}
```

---

## Ops Header Macros (`crumbs_ops.h`)

`src/crumbs_ops.h` provides code-generation macros for authoring family ops headers. Include it in your ops header to eliminate boilerplate for standard GET and SET wrapper functions.

```c
#include "crumbs_ops.h"
```

### `CRUMBS_DEFINE_GET_OP`

```c
CRUMBS_DEFINE_GET_OP(family, name, type_id, opcode, result_t, parse_fn)
```

Generates:

- `static inline int family_query_name(const crumbs_device_t *dev)` — internal, sends SET_REPLY + reads reply
- `static inline int family_get_name(const crumbs_device_t *dev, result_t *out)` — public, calls query + parse

Use for standard 1:1 opcode→result GETs. Multi-opcode GETs must still be written by hand.

### `CRUMBS_DEFINE_SEND_OP`

```c
CRUMBS_DEFINE_SEND_OP(family, name, type_id, opcode, param_decl, pack_stmt)
```

Generates `static inline int family_send_name(const crumbs_device_t *dev, <params>)`.

### `CRUMBS_DEFINE_SEND_OP_0`

```c
CRUMBS_DEFINE_SEND_OP_0(family, name, type_id, opcode)
```

Zero-parameter variant for commands with no payload (e.g. `display_send_clear`).

See `examples/families_usage/lhwit_family/led_ops.h` for a complete usage example.

---

## Platform HAL: Arduino

### Initialization

```c
void crumbs_arduino_init_controller(crumbs_context_t *ctx);
void crumbs_arduino_init_peripheral(crumbs_context_t *ctx, uint8_t address);
```

Initialize context and register Wire callbacks. Uses default Wire instance.

**Example (controller):**

```c
crumbs_context_t ctx;
crumbs_arduino_init_controller(&ctx);
```

**Example (peripheral):**

```c
#define I2C_ADDRESS 0x08
crumbs_context_t ctx;
crumbs_arduino_init_peripheral(&ctx, I2C_ADDRESS);
// Register per-opcode reply handlers for GET ops (preferred)
crumbs_register_reply_handler(&ctx, 0x00, reply_version, NULL);
crumbs_register_reply_handler(&ctx, MY_OP_GET_STATE, reply_state, NULL);
// on_request callback is also accepted as a backward-compatible fallback
```

### I²C Write Function

```c
int crumbs_arduino_wire_write(void *user_ctx, uint8_t addr,
                              const uint8_t *data, size_t len);
```

Wire-based write function for `crumbs_controller_send()`.

**Returns:**

- `0` — Success
- `>0` — Wire error code (from `endTransmission()`)

**Example:**

```c
crumbs_message_t msg;
crumbs_msg_init(&msg, 0x01, 0x01);
crumbs_controller_send(&ctx, 0x08, &msg, crumbs_arduino_wire_write, NULL);
```

### I²C Read Function

```c
int crumbs_arduino_read(void *user_ctx, uint8_t addr,
                       uint8_t *buffer, size_t len, uint32_t timeout_us);
```

Wire-based read function for scanner and diagnostics.

**Returns:**

- Number of bytes read (0–31)
- Negative values on error

### Bus Scanner

```c
int crumbs_arduino_scan(TwoWire *wire, uint8_t start_addr, uint8_t end_addr,
                       int strict, uint8_t *found, size_t max_found);
```

Scan I²C bus for CRUMBS-compatible devices.

**Parameters:**

- `strict` — Non-zero: requires valid CRUMBS frame. Zero: ACK-based detection.
- `found` — Output array for discovered addresses
- `max_found` — Size of found array

**Returns:** Number of devices found

**Example:**

```c
uint8_t devices[10];
int count = crumbs_arduino_scan(&Wire, 0x08, 0x77, 1, devices, 10);
```

---

## Platform HAL: Linux

> **Note:** Linux HAL supports **controller mode only**. Peripheral mode (I²C target) is not yet implemented.

### Initialization

```c
int crumbs_linux_init_controller(crumbs_context_t *ctx,
                                 crumbs_linux_i2c_t *i2c,
                                 const char *device_path,
                                 uint32_t timeout_us);
```

Initialize controller context and open I²C bus device.

**Parameters:**

- `device_path` — e.g., `"/dev/i2c-1"`
- `timeout_us` — Read timeout in microseconds

**Returns:**

- `0` — Success
- `-1` — Invalid arguments
- `-2` — Failed to open device

**Example:**

```c
crumbs_context_t ctx;
crumbs_linux_i2c_t bus;
int rc = crumbs_linux_init_controller(&ctx, &bus, "/dev/i2c-1", 10000);
```

### Cleanup

```c
void crumbs_linux_close(crumbs_linux_i2c_t *i2c);
```

Close I²C bus file descriptor.

### I²C Write Function

```c
int crumbs_linux_i2c_write(void *user_ctx, uint8_t target_addr,
                           const uint8_t *data, size_t len);
```

Linux I²C write function for `crumbs_controller_send()`.

**Returns:**

- `0` — Success
- `-1` — Invalid arguments or bus not open
- `-2` — Failed to select slave address
- `-3` — Write I/O error
- `-4` — Incomplete write

### I²C Read Function

```c
int crumbs_linux_read_message(crumbs_linux_i2c_t *i2c, uint8_t target_addr,
                              crumbs_context_t *ctx, crumbs_message_t *out_msg);
```

> **Deprecated since v0.10.3.** Prefer `crumbs_controller_read(ctx, addr, &msg, crumbs_linux_read, &bus)` for portable code. `crumbs_linux_read_message` is retained for compatibility.

Read and decode a CRUMBS message from a peripheral.

**Returns:**

- `0` — Success
- `-1` — Invalid arguments or bus not open
- `-2` — Failed to select slave address
- `-3` — Read I/O error
- `-4` — No bytes read
- `-1/-2` — Decode/CRC error (from decode)

---

## Discovery and Scanning

### Core Scanner

```c
int crumbs_controller_scan_for_crumbs(
    const crumbs_context_t *ctx,
    uint8_t start_addr,
    uint8_t end_addr,
    int strict,
    crumbs_i2c_write_fn write_fn,
    crumbs_i2c_read_fn read_fn,
    void *io_ctx,
    uint8_t *found,
    size_t max_found,
    uint32_t timeout_us);
```

Platform-independent bus scanner for CRUMBS-compatible devices.

**Parameters:**

- `start_addr`/`end_addr` — Inclusive probe range (0x08–0x77 typical)
- `strict` — Non-zero: read-only scan (no probe writes).
- `strict == 0` — Attempts read first; if no valid CRUMBS frame and `ctx`+`write_fn` are available, sends a small probe frame and reads again.
- `write_fn`/`read_fn` — Platform primitives
- `found` — Output array for discovered addresses
- `max_found` — Size of found array
- `timeout_us` — Read timeout per device

On mixed buses (CRUMBS + non-CRUMBS sensors), avoid broad range scanning and use constrained candidate address lists.

**Returns:** Number of devices found, or negative on error

**Example (Arduino):**

```c
uint8_t devices[10];
int count = crumbs_controller_scan_for_crumbs(
    &ctx, 0x08, 0x77, 1,
    crumbs_arduino_wire_write, crumbs_arduino_read, NULL,
    devices, 10, 10000);
```

### Candidate-Address Scanner

```c
int crumbs_controller_scan_for_crumbs_candidates(
    const crumbs_context_t *ctx,
    const uint8_t *candidates,
    size_t candidate_count,
    int strict,
    crumbs_i2c_write_fn write_fn,
    crumbs_i2c_read_fn read_fn,
    void *io_ctx,
    uint8_t *found,
    uint8_t *types,
    size_t max_found,
    uint32_t timeout_us);
```

Use this helper on mixed buses. It probes only the explicit addresses in `candidates` (deduplicated), avoiding full-range scans.

---

## Raw I2C Device Helpers

```c
typedef int (*crumbs_i2c_write_read_fn)(
    void *user_ctx, uint8_t addr,
    const uint8_t *tx, size_t tx_len,
    uint8_t *rx, size_t rx_len,
    uint32_t timeout_us,
    int require_repeated_start);
```

`crumbs_i2c_write_read_fn` performs a combined write-then-read transaction. Return value is bytes read (`>=0`) or negative on error.

Helper APIs bound to `crumbs_device_t`:

```c
int crumbs_i2c_dev_write(...);
int crumbs_i2c_dev_read(...);
int crumbs_i2c_dev_write_then_read(...);
int crumbs_i2c_dev_read_reg_ex(...);
int crumbs_i2c_dev_write_reg_ex(...);
int crumbs_i2c_dev_read_reg_u8(...);
int crumbs_i2c_dev_write_reg_u8(...);
int crumbs_i2c_dev_read_reg_u16be(...);
int crumbs_i2c_dev_write_reg_u16be(...);
```

Standard helper error codes:

- `CRUMBS_I2C_DEV_E_INVALID` (`-1`)
- `CRUMBS_I2C_DEV_E_WRITE` (`-2`)
- `CRUMBS_I2C_DEV_E_READ` (`-3`)
- `CRUMBS_I2C_DEV_E_SHORT_READ` (`-4`)
- `CRUMBS_I2C_DEV_E_NO_REPEATED_START` (`-5`)
- `CRUMBS_I2C_DEV_E_SIZE` (`-6`)

`crumbs_i2c_dev_write_then_read` fallback behavior:

- if `write_read_fn` is provided, it is used,
- if it is NULL and `require_repeated_start == 0`, fallback is `dev->write_fn` + `dev->read_fn`,
- if it is NULL and `require_repeated_start != 0`, returns `CRUMBS_I2C_DEV_E_NO_REPEATED_START`.

---

## Return Values and Error Codes

All CRUMBS functions use consistent conventions:

- **Success**: `0` (or positive value where documented)
- **Failure**: Negative values indicate specific error conditions

### Core Functions

| Function                             | Success             | Error                                                     |
| ------------------------------------ | ------------------- | --------------------------------------------------------- |
| `crumbs_encode_message()`            | `>0` (frame length) | `0` (buffer too small)                                    |
| `crumbs_decode_message()`            | `0`                 | `-1` (frame error), `-2` (CRC mismatch)                   |
| `crumbs_controller_send()`           | `0`                 | `-1` (args), `-2` (role), `-3` (encode), `>0` (I2C error) |
| `crumbs_peripheral_handle_receive()` | `0`                 | `-1` (args/decode), `-2` (CRC)                            |
| `crumbs_peripheral_build_reply()`    | `0`                 | `-1` (args/role), `-2` (encode)                           |
| `crumbs_controller_read()`           | `0`                 | `-1` (args/short read), decode error codes                |
| `crumbs_register_handler()`          | `0`                 | `-1` (NULL ctx or table full)                             |
| `crumbs_register_reply_handler()`    | `0`                 | `-1` (NULL ctx or table full)                             |
| `crumbs_unregister_handler()`        | `0`                 | Never fails                                               |

### Arduino HAL

| Function                      | Success | Error                  |
| ----------------------------- | ------- | ---------------------- |
| `crumbs_arduino_wire_write()` | `0`     | `>0` (Wire error code) |

### Linux HAL

| Function                         | Success | Error                                                                 |
| -------------------------------- | ------- | --------------------------------------------------------------------- |
| `crumbs_linux_init_controller()` | `0`     | `-1` (args), `-2` (open failed)                                       |
| `crumbs_linux_i2c_write()`       | `0`     | `-1` (args), `-2` (select), `-3` (I/O), `-4` (incomplete)             |
| `crumbs_linux_read_message()`    | `0`     | `-1` (args), `-2` (select), `-3` (I/O), `-4` (no data), decode errors |

---

## Complete Examples

### Peripheral with Handler Dispatch

```cpp
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

#define I2C_ADDRESS 0x08
static crumbs_context_t ctx;
static uint8_t led_state = 0;

void handle_set_all(crumbs_context_t *c, uint8_t cmd,
                    const uint8_t *data, uint8_t len, void *user) {
    uint8_t bitmask;
    if (crumbs_msg_read_u8(data, len, 0, &bitmask) == 0) {
        led_state = bitmask;
    }
}

void handle_set_one(crumbs_context_t *c, uint8_t cmd,
                    const uint8_t *data, uint8_t len, void *user) {
    uint8_t index, state;
    if (crumbs_msg_read_u8(data, len, 0, &index) == 0 &&
        crumbs_msg_read_u8(data, len, 1, &state) == 0) {
        if (state) led_state |= (1 << index);
        else led_state &= ~(1 << index);
    }
}

static void reply_get_state(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, 0x01, 0x10);
    crumbs_msg_add_u8(reply, led_state);
}

void setup() {
    crumbs_arduino_init_peripheral(&ctx, I2C_ADDRESS);
    crumbs_register_handler(&ctx, 0x01, handle_set_all, NULL);
    crumbs_register_handler(&ctx, 0x02, handle_set_one, NULL);
    crumbs_register_reply_handler(&ctx, 0x10, reply_get_state, NULL);
}

void loop() {}
```

### Controller with Message Helpers

```cpp
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

static crumbs_context_t ctx;

void setup() {
    Serial.begin(9600);
    crumbs_arduino_init_controller(&ctx);
}

void loop() {
    // Send LED command
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);  // type=LED, opcode=set_one
    crumbs_msg_add_u8(&msg, 0);         // LED index 0
    crumbs_msg_add_u8(&msg, 1);         // State ON

    int rc = crumbs_controller_send(&ctx, 0x08, &msg,
                                    crumbs_arduino_wire_write, NULL);

    if (rc != 0) {
        Serial.print("Send failed: ");
        Serial.println(rc);
    }

    delay(1000);
}
```

---

## See Also

- [Protocol Specification](protocol.md) — Wire format, versioning, CRC-8
- [Platform Setup](platform-setup.md) — Installation and configuration
- [Examples](examples.md) — Complete working examples
- Source headers: `src/crumbs.h`, `src/crumbs_message_helpers.h`, `src/crumbs_arduino.h`, `src/crumbs_linux.h`
