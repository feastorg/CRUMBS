# Create a Family

A family is one header that gives a device class its `type_id`, its opcodes
and the layout of every payload, plus the controller-side functions that speak
it. Peripheral firmware and controller programs both include it, so the two
sides cannot disagree about the vocabulary. This walks through a two-channel
thermometer, `therm`; every snippet below compiles as shown.
`examples/families_usage/lhwit_family/` is a complete four-device family;
it predates `crumbs_ops.h` and writes its wrappers by hand, in the shape of
[section 5](#5-when-the-macros-do-not-fit).

## 1. The contract: `therm_ops.h`

```c
#ifndef THERM_OPS_H
#define THERM_OPS_H

#include "crumbs.h"
#include "crumbs_message_helpers.h"
#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THERM_TYPE_ID 0x07

/* SET opcodes: controller -> peripheral, no reply. */
#define THERM_OP_SET_SAMPLE_RATE 0x01 /* [rate:u8]  samples per second, 1-10 */
#define THERM_OP_RESET           0x02 /* no payload */

/* GET opcodes: SET_REPLY then read. Replies carry the same opcode. */
#define THERM_OP_GET_TEMP        0x80 /* [ch0:i16][ch1:i16]  hundredths of a degree C */
#define THERM_OP_GET_SAMPLE_RATE 0x81 /* [rate:u8] */

#define THERM_VERSION_MAJOR 1
#define THERM_VERSION_MINOR 0
#define THERM_VERSION_PATCH 0
```

Pick a `type_id` other than `0x00` (the wildcard) and unused by any other
family that will share a controller. Put SET opcodes low and GET opcodes from
`0x80` so a reply's opcode says what it is. State each payload's byte layout
beside its opcode: that comment is the specification, since the library
does not check payloads.

## 2. Controller side: generated wrappers

`crumbs_ops.h` generates the wrappers from three macros. Each takes the family
prefix, an operation name, the type ID and opcode, and produces `static
inline` functions over a `crumbs_device_t`.

```c
/* therm_send_sample_rate(dev, rate) */
CRUMBS_DEFINE_SEND_OP(therm, sample_rate, THERM_TYPE_ID, THERM_OP_SET_SAMPLE_RATE,
                      uint8_t rate, crumbs_msg_add_u8(&_m, rate))

/* therm_send_reset(dev) */
CRUMBS_DEFINE_SEND_OP_0(therm, reset, THERM_TYPE_ID, THERM_OP_RESET)

typedef struct { int16_t ch0; int16_t ch1; } therm_temp_t;

static inline int therm_parse_temp(const uint8_t *data, size_t len, therm_temp_t *out)
{
    if (len < 4) return -1;
    return (crumbs_msg_read_i16(data, (uint8_t)len, 0, &out->ch0) == 0 &&
            crumbs_msg_read_i16(data, (uint8_t)len, 2, &out->ch1) == 0) ? 0 : -1;
}

/* therm_get_temp(dev, &out): SET_REPLY, 10 ms, read, identity check, parse */
CRUMBS_DEFINE_GET_OP(therm, temp, THERM_TYPE_ID, THERM_OP_GET_TEMP,
                     therm_temp_t, therm_parse_temp)

typedef struct { uint8_t rate; } therm_rate_t;

static inline int therm_parse_rate(const uint8_t *data, size_t len, therm_rate_t *out)
{
    return crumbs_msg_read_u8(data, (uint8_t)len, 0, &out->rate);
}

CRUMBS_DEFINE_GET_OP(therm, sample_rate, THERM_TYPE_ID, THERM_OP_GET_SAMPLE_RATE,
                     therm_rate_t, therm_parse_rate)

#ifdef __cplusplus
}
#endif
#endif /* THERM_OPS_H */
```

The parse function's signature is fixed: `int (const uint8_t *data, size_t
len, result_t *out)`, returning 0 on success. A `_get_` wrapper returns `-1`
for an unbound device, the send or read code on transport failure,
`CRUMBS_RX_REPLY_MISMATCH` (`-7`) if the reply's type or opcode is not the one
asked for, and otherwise whatever the parser returned. `SEND_OP` takes exactly
one parameter; an operation with two or more is written by hand (below).

## 3. Peripheral side

```c
#include "crumbs_arduino.h"
#include "therm_ops.h"

static crumbs_context_t ctx;
static uint8_t rate = 1;
static int16_t temp[2];

static void on_set_rate(crumbs_context_t *c, uint8_t op, const uint8_t *d, uint8_t n, void *u)
{
    (void)c; (void)op; (void)u;
    if (n >= 1 && d[0] >= 1 && d[0] <= 10) rate = d[0];
}

static void on_reset(crumbs_context_t *c, uint8_t op, const uint8_t *d, uint8_t n, void *u)
{
    (void)c; (void)op; (void)d; (void)n; (void)u;
    rate = 1;
}

static void reply_version(crumbs_context_t *c, crumbs_message_t *r, void *u)
{
    (void)c; (void)u;
    crumbs_build_version_reply(r, THERM_TYPE_ID, THERM_VERSION_MAJOR,
                               THERM_VERSION_MINOR, THERM_VERSION_PATCH);
}

static void reply_temp(crumbs_context_t *c, crumbs_message_t *r, void *u)
{
    (void)c; (void)u;
    crumbs_msg_init(r, THERM_TYPE_ID, THERM_OP_GET_TEMP);
    crumbs_msg_add_i16(r, temp[0]);
    crumbs_msg_add_i16(r, temp[1]);
}

static void reply_rate(crumbs_context_t *c, crumbs_message_t *r, void *u)
{
    (void)c; (void)u;
    crumbs_msg_init(r, THERM_TYPE_ID, THERM_OP_GET_SAMPLE_RATE);
    crumbs_msg_add_u8(r, rate);
}

void setup(void)
{
    crumbs_arduino_init_peripheral(&ctx, 0x11);
    crumbs_set_type_id(&ctx, THERM_TYPE_ID);          /* drop other families' frames */
    crumbs_register_handler(&ctx, THERM_OP_SET_SAMPLE_RATE, on_set_rate, NULL);
    crumbs_register_handler(&ctx, THERM_OP_RESET, on_reset, NULL);
    crumbs_register_reply_handler(&ctx, 0x00, reply_version, NULL);
    crumbs_register_reply_handler(&ctx, THERM_OP_GET_TEMP, reply_temp, NULL);
    crumbs_register_reply_handler(&ctx, THERM_OP_GET_SAMPLE_RATE, reply_rate, NULL);
}

void loop(void) { /* sample temp[] at `rate` Hz */ }
```

One SET handler per opcode; one reply handler per GET opcode; a reply handler
for `0x00` so scanners and version checks work. Each table holds
`CRUMBS_MAX_HANDLERS` entries (16 by default; set it as a build flag for both
library and sketch). Handlers run inside the I²C interrupt on AVR: copy data
out and return.

`crumbs_set_type_id()` makes the peripheral drop frames addressed to another
non-zero type before any handler runs, which is what lets two families share a
controller's address space without one misreading the other's opcodes.

## 4. Using the family from a controller

Bind a `crumbs_device_t` once per peripheral, then call the wrappers.

Linux:

```c
#include "crumbs_linux.h"
#include "therm_ops.h"

crumbs_context_t ctx;
crumbs_linux_i2c_t bus;
crumbs_linux_init_controller(&ctx, &bus, "/dev/i2c-1", 0);

crumbs_device_t dev = { &ctx, 0x11, crumbs_linux_i2c_write, crumbs_linux_read,
                        crumbs_linux_delay_us, &bus };

therm_temp_t t;
if (therm_get_temp(&dev, &t) == 0)
    printf("%d.%02d C\n", t.ch0 / 100, abs(t.ch0 % 100));
```

Arduino:

```c
#include "crumbs_arduino.h"
#include "therm_ops.h"

crumbs_context_t ctx;
crumbs_arduino_init_controller(&ctx);
crumbs_device_t dev = { &ctx, 0x11, crumbs_arduino_wire_write, crumbs_arduino_read,
                        crumbs_arduino_delay_us, NULL };
therm_send_sample_rate(&dev, 4);
```

The field order is `ctx, addr, write_fn, read_fn, delay_fn, io`; the last is
the pointer the HAL wants back (`&bus` on Linux, `NULL` or a `TwoWire*` on
Arduino).

## 5. When the macros do not fit

Write the wrapper by hand in the same shape the macro would have produced.
A two-parameter SET:

```c
static inline int therm_send_alarm(const crumbs_device_t *dev, uint8_t ch, int16_t limit)
{
    crumbs_message_t m;
    if (!crumbs_ops_can_send(dev)) return -1;
    crumbs_msg_init(&m, THERM_TYPE_ID, 0x03);
    crumbs_msg_add_u8(&m, ch);
    crumbs_msg_add_i16(&m, limit);
    return crumbs_controller_send(dev->ctx, dev->addr, &m, dev->write_fn, dev->io);
}
```

A parameterised GET puts its argument in the SET_REPLY payload after the
opcode byte — the peripheral reads `data[0]` as the opcode and can read
`data[1..]` itself in `on_message` before the reply is requested — then reads
with `crumbs_controller_read_expect()` exactly as the macro does. The LHWIT
calculator's history uses one opcode per slot instead, which needs no
parameter at all.

## Checklist

- `type_id` is not `0x00` and not another family's; SET opcodes low, GET from
  `0x80`; `0xFE` is never used.
- Every opcode has its payload layout in the header; multi-byte values go
  through `crumbs_msg_add_*` / `crumbs_msg_read_*` (little-endian).
- The peripheral calls `crumbs_set_type_id()` and answers `0x00` with
  `crumbs_build_version_reply()`.
- Every GET has both a reply handler on the peripheral and a `_get_` wrapper
  (or hand-written `read_expect` call) on the controller.
- Payloads stay within 27 bytes; check `crumbs_msg_add_*` return values in
  handlers that build variable-length replies.
