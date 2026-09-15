#ifndef CRUMBS_OPS_H
#define CRUMBS_OPS_H

/**
 * @file crumbs_ops.h
 * @brief Macros that define a family: its identity, its payload codecs and
 *        its controller-side wrappers.
 *
 * Everything here expands to `static inline` functions and enum constants
 * that cost nothing at run time. A family ops header uses them in this
 * order:
 *
 * @code
 *   // 1. Identity: type and opcodes, checked at compile time.
 *   #define THERM_OPS(X)                  \
 *       X(THERM_OP_SET_LIMIT, 0x03)       \
 *       X(THERM_OP_GET_TEMP,  0x80)
 *   CRUMBS_DEFINE_FAMILY(THERM, 0x07, THERM_OPS)
 *
 *   // 2. Payloads: one field list per layout, packed and unpacked by both sides.
 *   #define THERM_LIMIT_FIELDS(X) X(u8, ch) X(i16, limit)
 *   CRUMBS_DEFINE_PAYLOAD(therm_limit, 3, THERM_LIMIT_FIELDS)
 *   #define THERM_TEMP_FIELDS(X) X(i16, ch0) X(i16, ch1)
 *   CRUMBS_DEFINE_PAYLOAD(therm_temp, 4, THERM_TEMP_FIELDS)
 *
 *   // 3. Controller wrappers.
 *   CRUMBS_DEFINE_SEND_OP(therm, limit, THERM_TYPE_ID, THERM_OP_SET_LIMIT,
 *                         const therm_limit_t *v, therm_limit_pack(&_m, v))
 *   CRUMBS_DEFINE_GET_OP(therm, temp, THERM_TYPE_ID, THERM_OP_GET_TEMP,
 *                        therm_temp_t, therm_temp_unpack)
 * @endcode
 *
 * The peripheral calls `therm_limit_unpack()` in its SET handler and
 * `therm_temp_pack()` in its reply handler, so the two sides share one
 * statement of every layout.
 *
 * Not covered: variable-length or array payloads (write those with
 * crumbs_msg_add_bytes() / crumbs_msg_read_bytes()), and parameterised
 * queries ("get history entry N"), which need a SET that stores the
 * parameter on the peripheral before the query.
 *
 * Every macro is used without a trailing semicolon.
 */

#include <string.h> /* memcpy, for float fields */

#include "crumbs.h"
#include "crumbs_message_helpers.h"

/** @cond INTERNAL */
#if defined(__cplusplus)
#define CRUMBS_STATIC_ASSERT_IMPL_ static_assert
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define CRUMBS_STATIC_ASSERT_IMPL_ _Static_assert
#else
#error "crumbs_ops.h needs C11 or C++11"
#endif
/** @endcond */

/**
 * @brief Compile-time assertion, usable at file scope in C11 and C++11.
 *
 * @param cond Constant expression.
 * @param msg  String literal shown when it fails.
 */
#define CRUMBS_STATIC_ASSERT(cond, msg) CRUMBS_STATIC_ASSERT_IMPL_(cond, msg)

/** @cond INTERNAL */
#if defined(__GNUC__) || defined(__clang__)
#define CRUMBS_UNUSED_FN_ __attribute__((unused))
#else
#define CRUMBS_UNUSED_FN_
#endif

/* Field type tokens and their C types, wire widths and codecs. The codecs
   are internal to the generated pack/unpack and do no bounds checking:
   those check the whole payload once. */
#define CRUMBS_FIELD_CTYPE_u8 uint8_t
#define CRUMBS_FIELD_CTYPE_u16 uint16_t
#define CRUMBS_FIELD_CTYPE_u32 uint32_t
#define CRUMBS_FIELD_CTYPE_i8 int8_t
#define CRUMBS_FIELD_CTYPE_i16 int16_t
#define CRUMBS_FIELD_CTYPE_i32 int32_t
#define CRUMBS_FIELD_CTYPE_float float
#define CRUMBS_FIELD_WIRE_u8 1
#define CRUMBS_FIELD_WIRE_u16 2
#define CRUMBS_FIELD_WIRE_u32 4
#define CRUMBS_FIELD_WIRE_i8 1
#define CRUMBS_FIELD_WIRE_i16 2
#define CRUMBS_FIELD_WIRE_i32 4
#define CRUMBS_FIELD_WIRE_float 4

static inline uint8_t crumbs_field_read_u8_(const uint8_t *p) { return p[0]; }
static inline uint16_t crumbs_field_read_u16_(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t crumbs_field_read_u32_(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int8_t crumbs_field_read_i8_(const uint8_t *p) { return (int8_t)p[0]; }
static inline int16_t crumbs_field_read_i16_(const uint8_t *p) { return (int16_t)crumbs_field_read_u16_(p); }
static inline int32_t crumbs_field_read_i32_(const uint8_t *p) { return (int32_t)crumbs_field_read_u32_(p); }
static inline float crumbs_field_read_float_(const uint8_t *p) /* native order, as crumbs_msg_add_float() */
{
    float f;
    memcpy(&f, p, sizeof f);
    return f;
}
static inline void crumbs_field_write_u8_(uint8_t *p, uint8_t v) { p[0] = v; }
static inline void crumbs_field_write_u16_(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}
static inline void crumbs_field_write_u32_(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)(v >> 24);
}
static inline void crumbs_field_write_i8_(uint8_t *p, int8_t v) { p[0] = (uint8_t)v; }
static inline void crumbs_field_write_i16_(uint8_t *p, int16_t v) { crumbs_field_write_u16_(p, (uint16_t)v); }
static inline void crumbs_field_write_i32_(uint8_t *p, int32_t v) { crumbs_field_write_u32_(p, (uint32_t)v); }
static inline void crumbs_field_write_float_(uint8_t *p, float v) { memcpy(p, &v, sizeof v); }

#define CRUMBS_FIELD_MEMBER_(t, n) CRUMBS_FIELD_CTYPE_##t n;
#define CRUMBS_FIELD_SIZE_(t, n) +CRUMBS_FIELD_WIRE_##t
#define CRUMBS_FIELD_PACK_(t, n) \
    crumbs_field_write_##t##_(_p, v->n);  \
    _p += CRUMBS_FIELD_WIRE_##t;
#define CRUMBS_FIELD_UNPACK_(t, n) \
    v->n = crumbs_field_read_##t##_(_p);   \
    _p += CRUMBS_FIELD_WIRE_##t;

#define CRUMBS_OP_ENUM_(n, v) n = (v),
#define CRUMBS_OP_BAD_(n, v) +((v) < 0 || (v) > 0xFF || (v) == CRUMBS_CMD_SET_REPLY)
#define CRUMBS_OP_CASE_(n, v) \
    case n:                   \
        break;
/** @endcond */

/**
 * @brief Define a payload layout once: its struct, wire size, pack and unpack.
 *
 * @p FIELDS is an X-macro list of `X(type, name)` entries, in wire order,
 * where `type` is one of `u8 u16 u32 i8 i16 i32 float`. Integers travel
 * little-endian, floats as their 4 native bytes, as crumbs_msg_add_*() send
 * them. Expands to:
 *
 * - `typedef struct { ... } name_t;` with one member per field. Its layout
 *   is the compiler's, not the wire's: never copy a `name_t` onto the bus.
 * - `enum { name_wire_size = ... };` the payload length in bytes.
 * - `int name_pack(crumbs_message_t *msg, const name_t *v)`: appends the
 *   fields after `msg->data_len`. `-1` for `NULL` arguments or if the
 *   payload would exceed `CRUMBS_MAX_PAYLOAD`.
 * - `int name_unpack(const uint8_t *data, size_t len, name_t *v)`: reads
 *   the fields from the front of `data`. `-1` for `NULL` arguments or
 *   `len < name_wire_size`; longer buffers are accepted (trailing bytes are
 *   how a layout is extended compatibly). The signature is what
 *   CRUMBS_DEFINE_GET_OP() takes as `parse_fn`.
 *
 * Two compile-time checks: the field list must sum to @p wire_bytes (write
 * the number the layout comment states, so an edit to either is caught),
 * and @p wire_bytes must fit `CRUMBS_MAX_PAYLOAD`. An empty list is not a
 * payload: a payload-less SET uses CRUMBS_DEFINE_SEND_OP_0().
 *
 * @param name       Token: the struct is `name_t`, the functions `name_pack`
 *                   and `name_unpack`.
 * @param wire_bytes The layout's length in bytes, as an integer constant.
 * @param FIELDS     The field-list macro.
 */
#define CRUMBS_DEFINE_PAYLOAD(name, wire_bytes, FIELDS)                                    \
    typedef struct                                                                         \
    {                                                                                      \
        FIELDS(CRUMBS_FIELD_MEMBER_)                                                       \
    } name##_t;                                                                            \
    enum                                                                                   \
    {                                                                                      \
        name##_wire_size = 0 FIELDS(CRUMBS_FIELD_SIZE_)                                    \
    };                                                                                     \
    CRUMBS_STATIC_ASSERT(name##_wire_size == (wire_bytes),                                 \
                         #name ": the field list does not sum to the declared wire size"); \
    CRUMBS_STATIC_ASSERT((wire_bytes) > 0 && (wire_bytes) <= (int)CRUMBS_MAX_PAYLOAD,      \
                         #name ": payload exceeds CRUMBS_MAX_PAYLOAD");                    \
    CRUMBS_UNUSED_FN_ static inline int name##_pack(crumbs_message_t *msg, const name##_t *v) \
    {                                                                                      \
        uint8_t *_p;                                                                       \
        if (!msg || !v || (size_t)msg->data_len + (size_t)name##_wire_size > CRUMBS_MAX_PAYLOAD) \
            return -1;                                                                     \
        _p = msg->data + msg->data_len;                                                    \
        FIELDS(CRUMBS_FIELD_PACK_)                                                         \
        msg->data_len = (uint8_t)(msg->data_len + name##_wire_size);                       \
        return 0;                                                                          \
    }                                                                                      \
    CRUMBS_UNUSED_FN_ static inline int name##_unpack(const uint8_t *data, size_t len, name##_t *v) \
    {                                                                                      \
        const uint8_t *_p = data;                                                          \
        if (!data || !v || len < (size_t)name##_wire_size)                                 \
            return -1;                                                                     \
        FIELDS(CRUMBS_FIELD_UNPACK_)                                                       \
        return 0;                                                                          \
    }

/**
 * @brief Declare a family's type and opcodes once, checked at compile time.
 *
 * @p OPS is an X-macro list of `X(NAME, value)` entries. Expands to
 * `enum { PREFIX_TYPE_ID = type_id };` and one enum constant per opcode,
 * plus three checks that fail the build: the type must be `0x01`-`0xFF`
 * (`0x00` is the wildcard); every opcode must be `0x00`-`0xFF` and not
 * `0xFE` (SET_REPLY); and no two opcodes may share a value, which would make
 * one handler registration silently replace another. `0x00` is allowed: by
 * convention it is the version query (docs/protocol.md).
 *
 * The distinctness check is an unused `static inline` function whose
 * `switch` lists every opcode as a `case`; a duplicate is a duplicate case
 * label, reported by the compiler as such.
 *
 * @param PREFIX        Uppercase token; the type constant is `PREFIX_TYPE_ID`.
 * @param type_id_value The family's type, an integer constant.
 * @param OPS           The opcode-list macro.
 */
#define CRUMBS_DEFINE_FAMILY(PREFIX, type_id_value, OPS)                                   \
    enum                                                                                   \
    {                                                                                      \
        PREFIX##_TYPE_ID = (type_id_value)                                                 \
    };                                                                                     \
    CRUMBS_STATIC_ASSERT((type_id_value) >= 1 && (type_id_value) <= 0xFF,                  \
                         #PREFIX ": type_id must be 0x01-0xFF (0x00 is the wildcard)");    \
    enum                                                                                   \
    {                                                                                      \
        OPS(CRUMBS_OP_ENUM_) PREFIX##_opcode_list_end_                                     \
    };                                                                                     \
    enum                                                                                   \
    {                                                                                      \
        PREFIX##_bad_opcodes_ = 0 OPS(CRUMBS_OP_BAD_)                                      \
    };                                                                                     \
    CRUMBS_STATIC_ASSERT(PREFIX##_bad_opcodes_ == 0,                                       \
                         #PREFIX ": every opcode must be 0x00-0xFF and not 0xFE");         \
    CRUMBS_UNUSED_FN_ static inline void PREFIX##_opcodes_are_distinct_(uint8_t op)        \
    {                                                                                      \
        switch (op)                                                                        \
        {                                                                                  \
            OPS(CRUMBS_OP_CASE_)                                                           \
        default:                                                                           \
            break;                                                                         \
        }                                                                                  \
    }

/** @brief True if @p dev is bound to a context and has a write callback. */
static inline int crumbs_ops_can_send(const crumbs_device_t *dev)
{
    return dev && dev->ctx && dev->write_fn;
}

/** @brief True if @p dev can send and also has read and delay callbacks. */
static inline int crumbs_ops_can_get(const crumbs_device_t *dev)
{
    return crumbs_ops_can_send(dev) && dev->read_fn && dev->delay_fn;
}

/**
 * @brief Define `family_query_name(dev)` and `family_get_name(dev, out)` for one GET opcode.
 *
 * `family_query_name()` sends the SET_REPLY probe and is internal.
 * `family_get_name()` queries, waits `CRUMBS_DEFAULT_QUERY_DELAY_US`, reads the
 * reply with crumbs_controller_read_expect() and parses it. It returns -1 for
 * a NULL `out` or an unbound device, the send/read code on transport failure,
 * `CRUMBS_RX_REPLY_MISMATCH` if the reply carries another type or opcode, and
 * otherwise whatever @p parse_fn returns.
 *
 * Covers the 1:1 opcode-to-result fetch only; a parameterised query needs a
 * hand-written `_query_` that packs its argument into the probe payload.
 *
 * @param family        Token prefix, e.g. `therm`.
 * @param name          Operation name token, e.g. `temperature`.
 * @param type_id_value Peripheral type ID constant.
 * @param opcode_value  Opcode constant for this GET.
 * @param result_t      Typedef name of the result struct.
 * @param parse_fn      `int parse_fn(const uint8_t *data, size_t len, result_t *out)`.
 */
#define CRUMBS_DEFINE_GET_OP(family, name, type_id_value, opcode_value, result_t, parse_fn) \
    /** @internal Used by family##_get_##name(); prefer that for              */        \
    /** combined query+read.                                                  */        \
    static inline int family##_query_##name(const crumbs_device_t *dev)                \
    {                                                                                   \
        crumbs_message_t _m;                                                            \
        if (!crumbs_ops_can_send(dev)) return -1;                                      \
        crumbs_msg_init(&_m, 0, CRUMBS_CMD_SET_REPLY);                                 \
        crumbs_msg_add_u8(&_m, (uint8_t)(opcode_value));                               \
        return crumbs_controller_send(dev->ctx, dev->addr, &_m,                        \
                                      dev->write_fn, dev->io);                         \
    }                                                                                   \
    static inline int family##_get_##name(const crumbs_device_t *dev, result_t *out)   \
    {                                                                                   \
        crumbs_message_t _r;                                                            \
        int _rc;                                                                        \
        if (!out || !crumbs_ops_can_get(dev)) return -1;                               \
        _rc = family##_query_##name(dev);                                               \
        if (_rc != 0) return _rc;                                                       \
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);                                   \
        _rc = crumbs_controller_read_expect(dev->ctx, dev->addr,                       \
                                            (uint8_t)(type_id_value),                  \
                                            (uint8_t)(opcode_value), &_r,              \
                                            dev->read_fn, dev->io);                    \
        if (_rc != 0) return _rc;                                                       \
        return parse_fn(_r.data, _r.data_len, out);                                    \
    }

/**
 * @brief Define `family_send_name(dev, param)` for one single-parameter SET opcode.
 *
 * Returns -1 for an unbound device, otherwise the crumbs_controller_send() result.
 * Operations with two or more parameters are written as ordinary static inline
 * functions; the lhwit_family ops headers show the pattern.
 *
 * @param family        Token prefix.
 * @param name          Operation name token.
 * @param type_id_value Peripheral type ID constant.
 * @param opcode_value  Opcode constant.
 * @param param_decl    Single typed parameter, e.g. `uint8_t mask`.
 * @param pack_stmt     Expression (no trailing semicolon) packing the parameter into
 *                      the local message `_m`, e.g. `crumbs_msg_add_u8(&_m, mask)`.
 */
#define CRUMBS_DEFINE_SEND_OP(family, name, type_id_value, opcode_value, param_decl, pack_stmt) \
    static inline int family##_send_##name(const crumbs_device_t *dev, param_decl)    \
    {                                                                                  \
        crumbs_message_t _m;                                                           \
        if (!crumbs_ops_can_send(dev)) return -1;                                     \
        crumbs_msg_init(&_m, (uint8_t)(type_id_value), (uint8_t)(opcode_value));      \
        pack_stmt;                                                                     \
        return crumbs_controller_send(dev->ctx, dev->addr, &_m,                       \
                                      dev->write_fn, dev->io);                        \
    }

/**
 * @brief Define `family_send_name(dev)` for one payload-less SET opcode.
 *
 * Returns -1 for an unbound device, otherwise the crumbs_controller_send() result.
 *
 * @param family        Token prefix.
 * @param name          Operation name token.
 * @param type_id_value Peripheral type ID constant.
 * @param opcode_value  Opcode constant.
 */
#define CRUMBS_DEFINE_SEND_OP_0(family, name, type_id_value, opcode_value)            \
    static inline int family##_send_##name(const crumbs_device_t *dev)                \
    {                                                                                  \
        crumbs_message_t _m;                                                           \
        if (!crumbs_ops_can_send(dev)) return -1;                                     \
        crumbs_msg_init(&_m, (uint8_t)(type_id_value), (uint8_t)(opcode_value));      \
        return crumbs_controller_send(dev->ctx, dev->addr, &_m,                       \
                                      dev->write_fn, dev->io);                        \
    }

#endif /* CRUMBS_OPS_H */
