# Create a Family

A family is one header that gives a device class its `type_id`, its opcodes
and the layout of every payload, plus the codec both sides use and the
controller-side functions that speak it. Peripheral firmware and controller
programs both include it, so the two sides cannot disagree about the
vocabulary or, for anything that goes through the codec, the bytes. This
walks through a two-channel thermometer, `therm`; every snippet below
compiles as shown against the library at this revision.
`examples/families_usage/lhwit_family/` is a complete four-device family
written the same way.

## 1. The contract: `therm_ops.h`

```c
#ifndef THERM_OPS_H
#define THERM_OPS_H

#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type and opcodes, once. SET opcodes low, GET opcodes from 0x80. */
#define THERM_OPS(X)                                                          \
    X(THERM_OP_SET_SAMPLE_RATE, 0x01) /* [rate:u8] samples per second, 1-10 */ \
    X(THERM_OP_SET_ALARM, 0x02)       /* [ch:u8][limit:i16] */                \
    X(THERM_OP_RESET, 0x03)           /* no payload */                        \
    X(THERM_OP_GET_TEMP, 0x80)        /* reply [ch0:i16][ch1:i16] */          \
    X(THERM_OP_GET_SAMPLE_RATE, 0x81) /* reply [rate:u8] */                   \
    X(THERM_OP_GET_NAMED, 0x82)       /* reply [name:8 bytes][value:i16], section 5 */
CRUMBS_DEFINE_FAMILY(THERM, 0x07, THERM_OPS)

#define THERM_VERSION_MAJOR 1
#define THERM_VERSION_MINOR 0
#define THERM_VERSION_PATCH 0

/* Each payload layout, once: the struct, its wire size, pack and unpack. */
#define THERM_RATE_FIELDS(X) X(u8, rate) /* samples per second, 1-10 */
CRUMBS_DEFINE_PAYLOAD(therm_rate, 1, THERM_RATE_FIELDS)

#define THERM_ALARM_FIELDS(X) \
    X(u8, ch)                 /* 0 or 1 */ \
    X(i16, limit)             /* hundredths of a degree C */
CRUMBS_DEFINE_PAYLOAD(therm_alarm, 3, THERM_ALARM_FIELDS)

#define THERM_TEMP_FIELDS(X) X(i16, ch0) X(i16, ch1) /* hundredths of a degree C */
CRUMBS_DEFINE_PAYLOAD(therm_temp, 4, THERM_TEMP_FIELDS)
```

`CRUMBS_DEFINE_FAMILY` gives you `THERM_TYPE_ID` and the opcode constants and
refuses to compile a type of `0x00` (the wildcard), an opcode above `0xFF` or
equal to `0xFE` (SET_REPLY), or two opcodes with one value. Pick a type unused
by any other family that will share a controller.

`CRUMBS_DEFINE_PAYLOAD(name, bytes, FIELDS)` gives you `name_t`,
`name_wire_size`, `name_pack(msg, &v)` and `name_unpack(data, len, &v)`. Field
types are `u8 u16 u32 i8 i16 i32 float`, in wire order; integers travel
little-endian. The byte count beside the field list is checked against it,
so an edit to either without the other fails the build, and so does a layout
over 27 bytes.

## 2. Controller side: generated wrappers

Three more macros produce the `static inline` functions a controller calls,
over a `crumbs_device_t`. The payload's `pack` and `unpack` slot straight in.

```c
/* therm_send_sample_rate(dev, &v), therm_send_alarm(dev, &v) */
CRUMBS_DEFINE_SEND_OP(therm, sample_rate, THERM_TYPE_ID, THERM_OP_SET_SAMPLE_RATE,
                      const therm_rate_t *v, therm_rate_pack(&_m, v))
CRUMBS_DEFINE_SEND_OP(therm, alarm, THERM_TYPE_ID, THERM_OP_SET_ALARM,
                      const therm_alarm_t *v, therm_alarm_pack(&_m, v))

/* therm_send_reset(dev) */
CRUMBS_DEFINE_SEND_OP_0(therm, reset, THERM_TYPE_ID, THERM_OP_RESET)

/* therm_get_temp(dev, &out): SET_REPLY, 10 ms, read, identity check, unpack */
CRUMBS_DEFINE_GET_OP(therm, temp, THERM_TYPE_ID, THERM_OP_GET_TEMP,
                     therm_temp_t, therm_temp_unpack)
CRUMBS_DEFINE_GET_OP(therm, sample_rate, THERM_TYPE_ID, THERM_OP_GET_SAMPLE_RATE,
                     therm_rate_t, therm_rate_unpack)

#ifdef __cplusplus
}
#endif
#endif /* THERM_OPS_H */
```

A `_send_` wrapper returns `-1` for an unbound device or a `NULL` payload,
otherwise the transport's result. A `_get_` wrapper returns `-1` for an
unbound device or a `NULL` result pointer, the send or read code on transport
failure, `CRUMBS_RX_REPLY_MISMATCH` (`-7`) if the reply's type or opcode is
not the one asked for, and otherwise `unpack`'s result: `0`, or `-1` when the
reply is shorter than the layout.

## 3. Peripheral side

The SET handlers unpack; the reply handlers pack. Neither side ever names a
byte offset.

```c
#include "crumbs_arduino.h"
#include "therm_ops.h"

static crumbs_context_t ctx;
static uint8_t rate = 1;
static int16_t temp[2];
static int16_t alarm[2] = {32767, 32767};

static void on_set_rate(crumbs_context_t *c, uint8_t op, const uint8_t *d, uint8_t n, void *u)
{
    therm_rate_t v;
    (void)c; (void)op; (void)u;
    if (therm_rate_unpack(d, n, &v) == 0 && v.rate >= 1 && v.rate <= 10) rate = v.rate;
}

static void on_set_alarm(crumbs_context_t *c, uint8_t op, const uint8_t *d, uint8_t n, void *u)
{
    therm_alarm_t v;
    (void)c; (void)op; (void)u;
    if (therm_alarm_unpack(d, n, &v) == 0 && v.ch < 2) alarm[v.ch] = v.limit;
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
    therm_temp_t v;
    (void)c; (void)u;
    v.ch0 = temp[0];
    v.ch1 = temp[1];
    crumbs_msg_init(r, THERM_TYPE_ID, THERM_OP_GET_TEMP);
    therm_temp_pack(r, &v);
}

static void reply_rate(crumbs_context_t *c, crumbs_message_t *r, void *u)
{
    therm_rate_t v;
    (void)c; (void)u;
    v.rate = rate;
    crumbs_msg_init(r, THERM_TYPE_ID, THERM_OP_GET_SAMPLE_RATE);
    therm_rate_pack(r, &v);
}

void setup(void)
{
    crumbs_arduino_init_peripheral(&ctx, 0x11);
    crumbs_set_type_id(&ctx, THERM_TYPE_ID);          /* drop other families' frames */
    crumbs_register_handler(&ctx, THERM_OP_SET_SAMPLE_RATE, on_set_rate, NULL);
    crumbs_register_handler(&ctx, THERM_OP_SET_ALARM, on_set_alarm, NULL);
    crumbs_register_handler(&ctx, THERM_OP_RESET, on_reset, NULL);
    crumbs_register_reply_handler(&ctx, 0x00, reply_version, NULL);
    crumbs_register_reply_handler(&ctx, THERM_OP_GET_TEMP, reply_temp, NULL);
    crumbs_register_reply_handler(&ctx, THERM_OP_GET_SAMPLE_RATE, reply_rate, NULL);
}

void loop(void) { /* sample temp[] at `rate` Hz; compare with alarm[] */ }
```

One SET handler per opcode; one reply handler per GET opcode; a reply handler
for `0x00` so scanners and version checks work. Each table holds
`CRUMBS_MAX_HANDLERS` entries (16 by default; set it as a build flag for both
library and sketch). Handlers run inside the I²C interrupt on AVR: copy data
out and return. Install handlers and the type after
`crumbs_arduino_init_peripheral()`, which clears them.

`crumbs_set_type_id()` makes the peripheral drop frames addressed to another
non-zero type before any handler runs, which is what lets two families share a
controller's address space without one misreading the other's opcodes.

## 4. Using the family from a controller

Bind a `crumbs_device_t` once per peripheral, then call the wrappers.

Linux:

```c
#include <stdio.h>
#include <stdlib.h>
#include "crumbs_linux.h"
#include "therm_ops.h"

int main(void)
{
    crumbs_context_t ctx;
    crumbs_linux_i2c_t bus;
    if (crumbs_linux_init_controller(&ctx, &bus, "/dev/i2c-1", 0) != 0) return 1;

    crumbs_device_t dev = { &ctx, 0x11, crumbs_linux_i2c_write, crumbs_linux_read,
                            crumbs_linux_delay_us, &bus };

    therm_alarm_t alarm = { 0, 8500 }; /* channel 0 alarms at 85.00 C */
    therm_send_alarm(&dev, &alarm);

    therm_temp_t t;
    if (therm_get_temp(&dev, &t) == 0)
        printf("%d.%02d C\n", t.ch0 / 100, abs(t.ch0 % 100));
    crumbs_linux_close(&bus);
    return 0;
}
```

Arduino:

```c
#include "crumbs_arduino.h"
#include "therm_ops.h"

static crumbs_context_t ctx;
static crumbs_device_t dev = { &ctx, 0x11, crumbs_arduino_wire_write, crumbs_arduino_read,
                               crumbs_arduino_delay_us, NULL };

void setup(void)
{
    therm_rate_t four = { 4 };
    crumbs_arduino_init_controller(&ctx);
    therm_send_sample_rate(&dev, &four);
}

void loop(void) {}
```

The field order is `ctx, addr, write_fn, read_fn, delay_fn, io`; the last is
the pointer the HAL wants back (`&bus` on Linux, `NULL` or a `TwoWire*` on
Arduino).

## 5. When the macros do not fit

`CRUMBS_DEFINE_PAYLOAD` has no array or variable-length fields. Write such a
codec by hand in the same shape, so both sides still share it, and keep the
generated wrappers: a GET whose reply is a NUL-padded 8-byte name and a
reading looks like this.

```c
#include "therm_ops.h"

typedef struct { char name[9]; int16_t value; } therm_named_t;
enum { therm_named_wire_size = 10 };

static inline int therm_named_pack(crumbs_message_t *msg, const therm_named_t *v)
{
    uint8_t name[8] = {0};
    int i;
    if (!msg || !v) return -1;
    for (i = 0; i < 8 && v->name[i] != '\0'; i++) name[i] = (uint8_t)v->name[i];
    return (crumbs_msg_add_bytes(msg, name, 8) == 0 && crumbs_msg_add_i16(msg, v->value) == 0) ? 0 : -1;
}

static inline int therm_named_unpack(const uint8_t *data, size_t len, therm_named_t *v)
{
    int i;
    if (!data || !v || len < therm_named_wire_size) return -1;
    for (i = 0; i < 8; i++) v->name[i] = (char)data[i];
    v->name[8] = '\0';
    return crumbs_msg_read_i16(data, (uint8_t)len, 8, &v->value);
}

/* therm_get_named(dev, &out), exactly as for a generated payload */
CRUMBS_DEFINE_GET_OP(therm, named, THERM_TYPE_ID, THERM_OP_GET_NAMED, therm_named_t, therm_named_unpack)
```

The peripheral's reply handler for `THERM_OP_GET_NAMED` calls
`therm_named_pack()` the way `reply_temp` calls `therm_temp_pack()`.

The LHWIT LED family's blink table (four `enable`/`period` pairs) and the
calculator's history entry (a 4-byte operation name) are written this way.

A GET with a parameter cannot smuggle it in the SET_REPLY frame: the library
intercepts `0xFE` and stores only `data[0]`, and neither `on_message` nor any
handler sees that frame. Send the parameter first as an ordinary SET that the
peripheral stores, then SET_REPLY and read with
`crumbs_controller_read_expect()` as the macro does; or give each value its
own GET opcode, as the LHWIT calculator's twelve history slots do
(`calc_get_hist_entry()` in `calculator_ops.h` is that wrapper, written out).

## Checklist

- The type and every opcode are in the `CRUMBS_DEFINE_FAMILY` list; nothing
  else defines an opcode.
- Every payload has one `CRUMBS_DEFINE_PAYLOAD` (or one hand-written
  `pack`/`unpack` pair), and both sides call it: SET handlers unpack, reply
  handlers pack, wrappers take the struct. No handler indexes `data[]`.
- The peripheral calls `crumbs_set_type_id()` and answers `0x00` with
  `crumbs_build_version_reply()`.
- Every GET has both a reply handler on the peripheral and a `_get_` wrapper
  on the controller.
- The byte count beside each field list is the layout's length; the build
  tells you when it is not.
