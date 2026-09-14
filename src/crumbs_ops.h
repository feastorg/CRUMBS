#ifndef CRUMBS_OPS_H
#define CRUMBS_OPS_H

/**
 * @file crumbs_ops.h
 * @brief Helper macros for defining family ops-header functions.
 *
 * These macros generate the boilerplate query/get/send wrapper functions
 * that every CRUMBS family ops header requires. They expand to the same
 * calls a hand-written wrapper would make and carry no runtime overhead.
 *
 * Usage — in your family's ops header (e.g. therm_ops.h):
 *
 * @code
 *   // After defining result structs and parse functions:
 *   CRUMBS_DEFINE_GET_OP(therm, temperature,
 *                        THERM_TYPE_ID, THERM_OP_GET_TEMP,
 *                        therm_temp_result_t, therm_parse_temperature)
 *
 *   CRUMBS_DEFINE_SEND_OP(therm, set_interval,
 *                         THERM_TYPE_ID, THERM_OP_SET_INTERVAL,
 *                         uint16_t interval_ms,
 *                         crumbs_msg_add_u16(&_m, interval_ms))
 *
 *   CRUMBS_DEFINE_SEND_OP_0(therm, reset, THERM_TYPE_ID, THERM_OP_RESET)
 * @endcode
 *
 * Macro limitations:
 *   - CRUMBS_DEFINE_SEND_OP supports only single-parameter SETs cleanly.
 *     Operations with 2+ parameters should be written as normal inline
 *     functions (see any lhwit_family ops header for reference).
 *   - CRUMBS_DEFINE_GET_OP covers the standard 1:1 opcode->result fetch only.
 *     Parameterized queries (e.g. "get history entry N") require a custom
 *     _query_* that packs the index into the message payload.
 *
 * Requires: crumbs.h (includes crumbs_i2c.h for crumbs_device_t),
 *           crumbs_message_helpers.h (for crumbs_msg_init, crumbs_msg_add_*)
 */

#include "crumbs.h"
#include "crumbs_message_helpers.h"

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
