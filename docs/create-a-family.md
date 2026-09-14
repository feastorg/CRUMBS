# Creating a CRUMBS Device Family

A **family** is a named collection of CRUMBS peripheral types that share a common firmware base. For each type in the family you write one `*_ops.h` header that encapsulates every command the controller can send or query. This keeps application code clean and platform-agnostic.

This guide walks through creating a complete ops header from scratch, using a toy two-channel thermometer (`therm_ops.h`) as the running example.

---

## Anatomy of an ops header

An ops header is a **single C header** containing:

1. A `#define` for your device's `TYPE_ID`
2. `#define` constants for every opcode
3. `static inline` sender functions (`*_send_*`) — one per SET command
4. `static inline` query functions (`*_query_*`) — one per GET command (sends the SET_REPLY trigger)
5. Result structs (`*_result_t`) — one per GET command
6. `static inline` combined get functions (`*_get_*`) — query + delay + read + parse in one call

All functions are `static inline` so the header is self-contained — no `.c` file is needed.
All of them take a `const crumbs_device_t *dev` as their first argument — a bound device handle
that bundles the context, address, and platform I/O callbacks (see `src/crumbs_i2c.h`).

---

## Step 1: Choose a type ID and opcodes

Pick a type ID that does not conflict with existing families. The LHWIT family uses 0x01–0x04. Document your choices clearly.

```c
/** @brief Type ID for 2-channel thermometer device. */
#define THERM_TYPE_ID 0x07

/* SET operations */
#define THERM_OP_SET_SAMPLE_RATE 0x01   /**< Set samples/second [rate:u8] */

/* GET operations (via SET_REPLY) */
#define THERM_OP_GET_TEMP        0x80   /**< Get both temperatures [ch0:i16][ch1:i16] */
#define THERM_OP_GET_SAMPLE_RATE 0x81   /**< Get current sample rate [rate:u8] */
```

Convention: SET opcodes start at 0x01, GET opcodes start at 0x80. This avoids the reserved opcode 0xFE (SET_REPLY).

---

## Step 2: Write the boilerplate

```c
#ifndef THERM_OPS_H
#define THERM_OPS_H

#include "crumbs.h"
#include "crumbs_message_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ... your type ID and opcode defines here ... */

/* ... your functions and structs here ... */

#ifdef __cplusplus
}
#endif

#endif /* THERM_OPS_H */
```

---

## Step 3: Write the sender functions

One sender per SET command. Use `crumbs_msg_init` + `crumbs_msg_add_*` helpers to build the payload, then `crumbs_controller_send` to transmit.

```c
/**
 * @brief Set thermometer sample rate.
 * @param dev  Bound device handle.
 * @param rate Samples per second (1–10).
 * @return 0 on success.
 */
static inline int therm_send_set_sample_rate(const crumbs_device_t *dev, uint8_t rate)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, THERM_TYPE_ID, THERM_OP_SET_SAMPLE_RATE);
    crumbs_msg_add_u8(&msg, rate);
    return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
}
```

---

## Step 4: Write the query functions

One query function per GET command. It sends the SET*REPLY trigger that tells the peripheral which opcode to include in its next read response. Mark them `@internal` — they are called by the corresponding `\_get*\*` wrapper and should rarely be called directly.

```c
/** @internal Used by therm_get_temp(); prefer that for combined query+read. */
static inline int therm_query_temp(const crumbs_device_t *dev)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);   /* type_id=0 is fine for SET_REPLY */
    crumbs_msg_add_u8(&msg, THERM_OP_GET_TEMP);
    return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
}

/** @internal Used by therm_get_sample_rate(); prefer that for combined query+read. */
static inline int therm_query_sample_rate(const crumbs_device_t *dev)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
    crumbs_msg_add_u8(&msg, THERM_OP_GET_SAMPLE_RATE);
    return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
}
```

---

## Step 5: Define result structs

One struct per GET command, matching the wire payload exactly.

```c
/**
 * @brief Result of THERM_OP_GET_TEMP.
 * Wire payload: [ch0:i16][ch1:i16] (little-endian, 0.01 °C units)
 */
typedef struct {
    int16_t temp_ch0; /**< Channel 0 temperature in 0.01 °C units. */
    int16_t temp_ch1; /**< Channel 1 temperature in 0.01 °C units. */
} therm_temp_result_t;

/**
 * @brief Result of THERM_OP_GET_SAMPLE_RATE.
 */
typedef struct {
    uint8_t rate; /**< Samples per second. */
} therm_sample_rate_result_t;
```

---

## Step 6: Write the combined `_get_*` functions

This is the key abstraction. Each `_get_*` function performs the full three-step flow internally:

1. `*_query_*()` — sends the SET_REPLY write
2. `delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US)` — waits for the peripheral to stage its reply
3. `crumbs_controller_read()` — reads + decodes the reply frame
4. Parse the payload into the result struct

```c
/**
 * @brief Read both temperature channels from the thermometer.
 * @param dev Bound device handle.
 * @param out Output struct (must not be NULL).
 * @return 0 on success, non-zero on error.
 */
static inline int therm_get_temp(const crumbs_device_t *dev, therm_temp_result_t *out)
{
    crumbs_message_t reply;
    int rc;
    if (!out)
        return -1;

    rc = therm_query_temp(dev);
    if (rc != 0)
        return rc;

    dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);

    rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
    if (rc != 0)
        return rc;

    if (reply.type_id != THERM_TYPE_ID || reply.opcode != THERM_OP_GET_TEMP)
        return -1;

    /* Parse [ch0:i16][ch1:i16] using crumbs_msg_read_u16 then cast */
    uint16_t raw0, raw1;
    rc = crumbs_msg_read_u16(reply.data, reply.data_len, 0, &raw0);
    if (rc != 0)
        return rc;
    rc = crumbs_msg_read_u16(reply.data, reply.data_len, 2, &raw1);
    if (rc != 0)
        return rc;
    out->temp_ch0 = (int16_t)raw0;
    out->temp_ch1 = (int16_t)raw1;
    return 0;
}
```

> **Why `crumbs_controller_read` instead of calling `read_fn` directly?**
>
> `crumbs_controller_read` validates the frame length, calls `crumbs_decode_message` (CRC check + struct population), and returns a clean `crumbs_message_t`. Calling `read_fn` directly skips CRC validation and leaves raw bytes in a local buffer, which every caller would have to manage identically. The core function exists precisely so ops headers don't have to repeat this 4-line pattern.

---

## Step 7: Reduce boilerplate with `crumbs_ops.h` (optional)

The `_query_*` + `_get_*` pair and single-parameter `_send_*` functions are entirely deterministic.
`src/crumbs_ops.h` provides macros that generate the identical code from a single line:

```c
#include "crumbs_ops.h"

/* Replaces therm_query_sample_rate + therm_get_sample_rate (~22 lines) */
CRUMBS_DEFINE_GET_OP(therm, sample_rate,
                     THERM_TYPE_ID, THERM_OP_GET_SAMPLE_RATE,
                     therm_sample_rate_result_t, therm_parse_sample_rate)

/* Replaces therm_send_set_sample_rate (~6 lines) */
CRUMBS_DEFINE_SEND_OP(therm, set_sample_rate,
                      THERM_TYPE_ID, THERM_OP_SET_SAMPLE_RATE,
                      uint8_t rate,
                      crumbs_msg_add_u8(&_m, rate))
```

For `therm_get_temp` the multi-line parse step must first be extracted into a named function
(`therm_parse_temp`) and then passed to the macro, or written by hand as in Step 6.

**What the macros cover:**

- `CRUMBS_DEFINE_GET_OP` — standard 1:1 opcode→result GETs (no extra query parameters)
- `CRUMBS_DEFINE_SEND_OP` — single-parameter SETs
- `CRUMBS_DEFINE_SEND_OP_0` — zero-parameter SETs (e.g. a `clear` command)

**What must still be written by hand:**

- SET operations with 2+ parameters
- Parameterized queries (e.g. "get history entry N") where the index must be packed into the query payload
- Parse logic — result structs and parse functions are always hand-written

The existing lhwit_family ops headers (`led_ops.h`, `servo_ops.h`, etc.) are concrete
reference implementations showing both covered and uncovered cases.

---

## Step 8: Use the ops header on Linux

```c
#include "crumbs_linux.h"
#include "therm_ops.h"

crumbs_context_t ctx;
crumbs_linux_i2c_t lw;
crumbs_linux_init_controller(&ctx, &lw, "/dev/i2c-1", 0);

crumbs_device_t dev = {
    .ctx      = &ctx,
    .addr     = THERM_ADDR,
    .write_fn = crumbs_linux_i2c_write,
    .read_fn  = crumbs_linux_read,
    .delay_fn = crumbs_linux_delay_us,
    .io       = &lw,
};

/* SET: configure sample rate */
therm_send_set_sample_rate(&dev, 5);

/* GET: read temperatures */
therm_temp_result_t temps;
int rc = therm_get_temp(&dev, &temps);
if (rc == 0) {
    printf("Ch0: %.2f C\n", temps.temp_ch0 / 100.0);
    printf("Ch1: %.2f C\n", temps.temp_ch1 / 100.0);
}

crumbs_linux_close(&lw);
```

---

## Step 9: Use the ops header on Arduino

```cpp
#include "crumbs_arduino.h"
#include "therm_ops.h"

crumbs_context_t ctx;
crumbs_arduino_init_controller(&ctx);

crumbs_device_t dev = {
    .ctx      = &ctx,
    .addr     = THERM_ADDR,
    .write_fn = crumbs_arduino_wire_write,
    .read_fn  = crumbs_arduino_read,
    .delay_fn = crumbs_arduino_delay_us,
    .io       = NULL,
};

therm_temp_result_t temps;
int rc = therm_get_temp(&dev, &temps);
if (rc == 0) {
    Serial.print("Ch0: ");
    Serial.println(temps.temp_ch0 / 100.0);
}
```

The ops header is identical on both platforms. Bundle the platform-specific callbacks into a
`crumbs_device_t` device handle; call sites then reduce to a single `&dev` argument.

---

## Peripheral implementation

The ops header covers the **controller side**. On the **peripheral side** you write the firmware
that receives commands and responds to read requests. CRUMBS provides symmetrical per-opcode
dispatch for both SET and GET operations.

First, declare the device type once after initialisation:

```c
crumbs_set_type_id(&ctx, THERM_TYPE_ID);
```

From then on a frame carrying any other non-zero `type_id` is dropped before any callback or
handler runs (a misaddressed command from another family cannot execute as one of yours).
`type_id 0x00` is a wildcard and always passes; the generated getters' SET_REPLY frames use it.

### SET operations — per-opcode handler table

Register one handler per opcode. The library dispatches automatically:

```c
static void handle_set_sample_rate(crumbs_context_t *ctx, uint8_t opcode,
                                   const uint8_t *data, uint8_t len, void *user)
{
    uint8_t rate;
    if (crumbs_msg_read_u8(data, len, 0, &rate) == 0)
        g_sample_rate = rate;
}

/* In setup(): */
crumbs_register_handler(&ctx, THERM_OP_SET_SAMPLE_RATE, handle_set_sample_rate, NULL);
```

Adding a new SET op = one `crumbs_register_handler` call + one handler function. No switch needed.

### GET operations — per-opcode reply handler table

Register one reply handler per GET opcode with `crumbs_register_reply_handler()`. The library
dispatches automatically when the controller issues a read:

```c
static void handle_get_temp(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, THERM_TYPE_ID, THERM_OP_GET_TEMP);
    crumbs_msg_add_u16(reply, (uint16_t)g_temp_ch0);
    crumbs_msg_add_u16(reply, (uint16_t)g_temp_ch1);
}

static void handle_get_sample_rate(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_msg_init(reply, THERM_TYPE_ID, THERM_OP_GET_SAMPLE_RATE);
    crumbs_msg_add_u8(reply, g_sample_rate);
}

static void handle_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, THERM_TYPE_ID, 1, 0, 0);
}

/* In setup(): */
crumbs_register_reply_handler(&ctx, 0,                    handle_version,         NULL);
crumbs_register_reply_handler(&ctx, THERM_OP_GET_TEMP,    handle_get_temp,        NULL);
crumbs_register_reply_handler(&ctx, THERM_OP_GET_SAMPLE_RATE, handle_get_sample_rate, NULL);
```

Adding a new GET op = one `crumbs_register_reply_handler` call + one handler function. No switch needed.

### GET operations — `on_request` callback (alternative)

The `on_request` callback is an alternative: one function with an explicit switch on
`ctx->requested_opcode`. Use it if you prefer a single dispatch point, or when porting
from older CRUMBS code:

```c
static void on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    switch (ctx->requested_opcode)
    {
    case 0:
        crumbs_build_version_reply(reply, THERM_TYPE_ID, 1, 0, 0);
        break;
    case THERM_OP_GET_TEMP:
        crumbs_msg_init(reply, THERM_TYPE_ID, THERM_OP_GET_TEMP);
        crumbs_msg_add_u16(reply, (uint16_t)g_temp_ch0);
        crumbs_msg_add_u16(reply, (uint16_t)g_temp_ch1);
        break;
    default:
        /* Unknown opcode — return empty reply as safe fallback */
        crumbs_msg_init(reply, THERM_TYPE_ID, ctx->requested_opcode);
        break;
    }
}
crumbs_set_callbacks(&ctx, NULL, on_request, NULL);
```

> **Always include a `default:` case** that calls `crumbs_msg_init(reply, TYPE_ID, ctx->requested_opcode)`.
> This ensures the reply struct is never left in an uninitialised state if an unknown opcode arrives.

When both reply handlers and `on_request` are configured, reply handlers take priority;
`on_request` is called only for opcodes that have no registered reply handler.

### Choosing the right mechanism

| Mechanism                         | Use for                      | Notes                                                                              |
| --------------------------------- | ---------------------------- | ---------------------------------------------------------------------------------- |
| `crumbs_register_handler()`       | SET operations               | One per opcode; preferred for all incoming writes                                  |
| `crumbs_register_reply_handler()` | GET operations               | One per opcode; preferred for all read replies                                     |
| `on_request` callback             | GET operations (alternative) | Single switch; backward-compatible; used as fallback when no reply handler matches |
| `on_message` callback             | Advanced use only            | Fires before handler table for every accepted write (after the type check, never for SET_REPLY); for logging/monitors, not device logic |

`hello_peripheral.ino` uses `on_message` for brevity (one callback, no handler table). For a real
family peripheral, use `crumbs_register_handler()` for each SET opcode and
`crumbs_register_reply_handler()` for each GET opcode — see `examples/families_usage/` for the
complete pattern.

---

## Reference: payload helper functions

| Helper                                    | Description                    |
| ----------------------------------------- | ------------------------------ |
| `crumbs_msg_add_u8(msg, v)`               | Append 1-byte value to payload |
| `crumbs_msg_add_u16(msg, v)`              | Append 2-byte LE value         |
| `crumbs_msg_add_u32(msg, v)`              | Append 4-byte LE value         |
| `crumbs_msg_read_u8(data, len, off, &v)`  | Read 1 byte at offset          |
| `crumbs_msg_read_u16(data, len, off, &v)` | Read 2 bytes LE at offset      |
| `crumbs_msg_read_u32(data, len, off, &v)` | Read 4 bytes LE at offset      |

Maximum payload is `CRUMBS_MAX_PAYLOAD` bytes (27). All multi-byte values are little-endian.

---

## Reference: platform function pointers

| Symbol                | Linux                              | Arduino                     |
| --------------------- | ---------------------------------- | --------------------------- |
| `crumbs_i2c_write_fn` | `crumbs_linux_i2c_write`           | `crumbs_arduino_wire_write` |
| `crumbs_i2c_read_fn`  | `crumbs_linux_read`                | `crumbs_arduino_read`       |
| `crumbs_delay_fn`     | `crumbs_linux_delay_us`            | `crumbs_arduino_delay_us`   |
| `io` context          | `(void *)&lw` (crumbs_linux_i2c_t) | `NULL` (uses global Wire)   |

---

## Checklist

- [ ] Type ID chosen and documented, does not conflict with existing families
- [ ] All opcodes defined with payload format in comments
- [ ] One `*_send_*` function per SET operation
- [ ] One `*_query_*` function per GET operation (sends SET_REPLY)
- [ ] One `*_result_t` struct per GET operation
- [ ] One `*_get_*` function per GET operation (query + delay + `crumbs_controller_read_expect` + parse); hand-written getters use `crumbs_controller_read_expect()` rather than comparing `type_id`/`opcode` themselves
- [ ] All `_get_*` functions return 0 on success, non-zero on error
- [ ] `CRUMBS_DEFINE_GET_OP` / `CRUMBS_DEFINE_SEND_OP` used for 1:1 ops (or equivalent hand-written)
- [ ] Header guard (`#ifndef / #define / #endif`) in place
- [ ] `#ifdef __cplusplus extern "C"` wrapper present for C++ compatibility
- [ ] Works with both Linux and Arduino function pointer combinations
- [ ] Peripheral: `crumbs_set_type_id(&ctx, MY_TYPE_ID)` after init, so frames for other device types are dropped before dispatch
- [ ] Peripheral: one `crumbs_register_handler()` call per SET opcode
- [ ] Peripheral: one `crumbs_register_reply_handler()` call per GET opcode (or `on_request` with `default:` fallback)
- [ ] Peripheral: `on_message` not used for device logic (handler table used instead)
