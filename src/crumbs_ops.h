#ifndef CRUMBS_OPS_H
#define CRUMBS_OPS_H

/**
 * @file crumbs_ops.h
 * @brief Helper macros for defining family ops-header functions.
 *
 * These macros generate the boilerplate query/get/send wrapper functions
 * that every CRUMBS family ops header requires. They produce identical
 * code to hand-written equivalents and carry no runtime overhead.
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

static inline int crumbs_ops_can_send(const crumbs_device_t *dev)
{
    return dev && dev->ctx && dev->write_fn;
}

static inline int crumbs_ops_can_get(const crumbs_device_t *dev)
{
    return crumbs_ops_can_send(dev) && dev->read_fn && dev->delay_fn;
}

/* -----------------------------------------------------------------------
 * CRUMBS_DEFINE_GET_OP
 *
 * Generates:
 *   family_query_name(dev)           — @internal, sends SET_REPLY probe
 *   family_get_name(dev, result_t*)  — public, full query+delay+read+parse
 *
 * Parameters:
 *   family    Token prefix, e.g. therm
 *   name      Operation name token, e.g. temperature
 *   type_id   Peripheral type ID constant
 *   opcode    Op constant for this GET, e.g. THERM_OP_GET_TEMP
 *   result_t  Typedef name of the result struct, e.g. therm_temp_result_t
 *   parse_fn  int parse_fn(const uint8_t *data, size_t len, result_t *out)
 * ----------------------------------------------------------------------- */
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
        _rc = crumbs_controller_read(dev->ctx, dev->addr, &_r,                         \
                                     dev->read_fn, dev->io);                           \
        if (_rc != 0) return _rc;                                                       \
        if (_r.type_id != (uint8_t)(type_id_value) ||                                  \
            _r.opcode  != (uint8_t)(opcode_value))  return -1;                         \
        return parse_fn(_r.data, _r.data_len, out);                                    \
    }

/* -----------------------------------------------------------------------
 * CRUMBS_DEFINE_SEND_OP
 *
 * Generates:
 *   family_send_name(dev, param)  — public, single-parameter SET
 *
 * pack_stmt is a single expression (no trailing semicolon) that packs one
 * argument into the local crumbs_message_t _m, e.g.:
 *   crumbs_msg_add_u8(&_m, mask)
 *   crumbs_msg_add_u16(&_m, interval_ms)
 *   crumbs_msg_add_u32(&_m, value)
 *
 * For operations with 2+ parameters, write a normal static inline function
 * (the lhwit_family ops headers are reference implementations for these).
 *
 * Parameters:
 *   family      Token prefix
 *   name        Operation name token
 *   type_id     Peripheral type ID
 *   opcode      Op constant
 *   param_decl  Single typed parameter, e.g. uint8_t mask
 *   pack_stmt   Expression packing param into _m (no semicolon)
 * ----------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------
 * CRUMBS_DEFINE_SEND_OP_0
 *
 * Generates:
 *   family_send_name(dev)  — public, zero-parameter SET
 *
 * Use for operations that carry no payload, e.g. display_send_clear.
 * ----------------------------------------------------------------------- */
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
