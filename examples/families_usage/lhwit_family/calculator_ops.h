/**
 * @file calculator_ops.h
 * @brief Calculator command definitions (Type ID 0x03)
 *
 * This file defines commands for a simple 32-bit integer calculator peripheral.
 * The calculator performs basic arithmetic (ADD, SUB, MUL, DIV) and maintains
 * a history of the last 12 operations.
 *
 * Pattern: Function-style interface
 * - SET operations (0x01-0x04): Execute calculations, update internal state
 * - GET operations (0x80-0x8D): Retrieve results and history via SET_REPLY
 *
 * Commands:
 * - CALC_OP_ADD:         Add two u32 values
 * - CALC_OP_SUB:         Subtract two u32 values
 * - CALC_OP_MUL:         Multiply two u32 values
 * - CALC_OP_DIV:         Divide two u32 values
 * - CALC_OP_GET_RESULT:  Request last calculation result (via SET_REPLY)
 * - CALC_OP_GET_HIST_META: Request history metadata (count + write_pos)
 * - CALC_OP_GET_HIST_*:  Request specific history entry (entries 0-11)
 */

#ifndef CALCULATOR_OPS_H
#define CALCULATOR_OPS_H

#include "crumbs.h"
#include "crumbs_message_helpers.h"
#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ============================================================================
 * Device Identity
 * ============================================================================ */

/** @brief Type ID for calculator device. */
#define CALC_TYPE_ID 0x03

/* Module protocol version (per versioning.md convention) */
#define CALC_MODULE_VER_MAJOR 1
#define CALC_MODULE_VER_MINOR 0
#define CALC_MODULE_VER_PATCH 0

/* ============================================================================
 * Command Definitions: SET Operations (Execute Commands)
 * ============================================================================ */

/**
 * @brief Add operation - adds two u32 values.
 * Payload: [a:u32][b:u32] (little-endian)
 * Result: Stored in g_last_result, added to history
 */
#define CALC_OP_ADD 0x01

/**
 * @brief Subtract operation - subtracts b from a.
 * Payload: [a:u32][b:u32] (little-endian)
 * Result: Stored in g_last_result (a - b), added to history
 */
#define CALC_OP_SUB 0x02

/**
 * @brief Multiply operation - multiplies two u32 values.
 * Payload: [a:u32][b:u32] (little-endian)
 * Result: Stored in g_last_result (a * b), added to history
 */
#define CALC_OP_MUL 0x03

/**
 * @brief Divide operation - divides a by b (integer division).
 * Payload: [a:u32][b:u32] (little-endian)
 * Result: Stored in g_last_result (a / b), added to history
 * Note: Division by zero sets result to 0xFFFFFFFF
 */
#define CALC_OP_DIV 0x04

/* ============================================================================
 * Command Definitions: GET Operations (Query State via SET_REPLY)
 * ============================================================================ */

/**
 * @brief Request last calculation result.
 * Payload: none
 * Reply: [result:u32] (little-endian)
 */
#define CALC_OP_GET_RESULT 0x80

/**
 * @brief Request history metadata.
 * Payload: none
 * Reply: [count:u8][write_pos:u8]
 *   - count: Number of valid entries (0-12)
 *   - write_pos: Next write slot index (0-11, circular)
 */
#define CALC_OP_GET_HIST_META 0x81

/**
 * @brief Request history entry 0.
 * Payload: none
 * Reply: [op:4 bytes][a:u32][b:u32][result:u32] (16 bytes total)
 *   - op: "ADD\0", "SUB\0", "MUL\0", or "DIV\0"
 *   - a, b, result: Little-endian u32 values
 * Note: Returns empty message if entry doesn't exist yet
 */
#define CALC_OP_GET_HIST_0 0x82

#define CALC_OP_GET_HIST_1 0x83  /**< @brief Request history entry 1 */
#define CALC_OP_GET_HIST_2 0x84  /**< @brief Request history entry 2 */
#define CALC_OP_GET_HIST_3 0x85  /**< @brief Request history entry 3 */
#define CALC_OP_GET_HIST_4 0x86  /**< @brief Request history entry 4 */
#define CALC_OP_GET_HIST_5 0x87  /**< @brief Request history entry 5 */
#define CALC_OP_GET_HIST_6 0x88  /**< @brief Request history entry 6 */
#define CALC_OP_GET_HIST_7 0x89  /**< @brief Request history entry 7 */
#define CALC_OP_GET_HIST_8 0x8A  /**< @brief Request history entry 8 */
#define CALC_OP_GET_HIST_9 0x8B  /**< @brief Request history entry 9 */
#define CALC_OP_GET_HIST_10 0x8C /**< @brief Request history entry 10 */
#define CALC_OP_GET_HIST_11 0x8D /**< @brief Request history entry 11 */

    /* ============================================================================
     * Controller Side: Command Senders
     * ============================================================================ */

    /**
     * @brief Send ADD command to calculator peripheral.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param a    First operand (u32).
     * @param b    Second operand (u32).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_send_add(const crumbs_device_t *dev, uint32_t a, uint32_t b)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, CALC_TYPE_ID, CALC_OP_ADD);
        crumbs_msg_add_u32(&msg, a);
        crumbs_msg_add_u32(&msg, b);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Send SUB command to calculator peripheral.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param a    Minuend (u32).
     * @param b    Subtrahend (u32).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_send_sub(const crumbs_device_t *dev, uint32_t a, uint32_t b)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, CALC_TYPE_ID, CALC_OP_SUB);
        crumbs_msg_add_u32(&msg, a);
        crumbs_msg_add_u32(&msg, b);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Send MUL command to calculator peripheral.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param a    First factor (u32).
     * @param b    Second factor (u32).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_send_mul(const crumbs_device_t *dev, uint32_t a, uint32_t b)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, CALC_TYPE_ID, CALC_OP_MUL);
        crumbs_msg_add_u32(&msg, a);
        crumbs_msg_add_u32(&msg, b);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Send DIV command to calculator peripheral.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param a    Dividend (u32).
     * @param b    Divisor (u32).
     * @return 0 on success, non-zero on error.
     * @note Division by zero results in 0xFFFFFFFF.
     */
    static inline int calc_send_div(const crumbs_device_t *dev, uint32_t a, uint32_t b)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, CALC_TYPE_ID, CALC_OP_DIV);
        crumbs_msg_add_u32(&msg, a);
        crumbs_msg_add_u32(&msg, b);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query last calculation result (peripheral will respond on next I2C read).
     *
     * @internal Used by calc_get_result(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_query_result(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, CALC_OP_GET_RESULT);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query history metadata (peripheral will respond on next I2C read).
     *
     * @internal Used by calc_get_hist_meta(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_query_hist_meta(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, CALC_OP_GET_HIST_META);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query specific history entry (peripheral will respond on next I2C read).
     *
     * @internal Used by calc_get_hist_entry(); prefer that function for combined query+read.
     *
     * @param dev       Bound device handle (see crumbs_device_t).
     * @param entry_idx Entry index (0-11).
     * @return 0 on success, -1 if invalid index, non-zero on I2C error.
     */
    static inline int calc_query_hist_entry(const crumbs_device_t *dev, uint8_t entry_idx)
    {
        crumbs_message_t msg;

        if (!crumbs_ops_can_send(dev) || entry_idx > 11)
        {
            return -1; /* Invalid device or index */
        }

        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, CALC_OP_GET_HIST_0 + entry_idx);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /* ============================================================================
     * Controller Side: Combined Query + Read (Receiver API)
     * ============================================================================ */

    /**
     * @brief Result struct for CALC_OP_GET_RESULT.
     */
    typedef struct
    {
        uint32_t result; /**< Last calculation result. */
    } calc_result_t;

    /**
     * @brief Result struct for CALC_OP_GET_HIST_META.
     */
    typedef struct
    {
        uint8_t count;     /**< Number of valid history entries (0-12). */
        uint8_t write_pos; /**< Next write slot index (0-11, circular). */
    } calc_hist_meta_t;

    /**
     * @brief Result struct for one history entry (CALC_OP_GET_HIST_0 .. _11).
     *
     * Wire format: [op:4 bytes][a:u32][b:u32][result:u32] (16 bytes total).
     * An empty entry is signalled by the peripheral sending a zero-length payload;
     * calc_get_hist_entry() returns -1 in that case.
     */
    typedef struct
    {
        char     op[5];   /**< Operation string: "ADD\0", "SUB\0", "MUL\0", or "DIV\0". */
        uint32_t a;       /**< First operand. */
        uint32_t b;       /**< Second operand. */
        uint32_t result;  /**< Computed result. */
    } calc_hist_entry_t;

    /**
     * @brief Combined SET_REPLY query + read + parse for last calculation result.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_get_result(const crumbs_device_t *dev, calc_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = calc_query_result(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != CALC_TYPE_ID || reply.opcode != CALC_OP_GET_RESULT)
            return -1;
        return crumbs_msg_read_u32(reply.data, reply.data_len, 0, &out->result);
    }

    /**
     * @brief Combined SET_REPLY query + read + parse for history metadata.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int calc_get_hist_meta(const crumbs_device_t *dev, calc_hist_meta_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = calc_query_hist_meta(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != CALC_TYPE_ID || reply.opcode != CALC_OP_GET_HIST_META)
            return -1;
        rc = crumbs_msg_read_u8(reply.data, reply.data_len, 0, &out->count);
        if (rc != 0)
            return rc;
        return crumbs_msg_read_u8(reply.data, reply.data_len, 1, &out->write_pos);
    }

    /**
     * @brief Combined SET_REPLY query + read + parse for a single history entry.
     *
     * Returns -1 if entry_idx > 11 or the peripheral returns an empty payload
     * (indicates the slot has not been written yet).
     *
     * @param dev       Bound device handle (see crumbs_device_t).
     * @param entry_idx History slot index (0-11).
     * @param out       Output struct (must not be NULL).
     * @return 0 on success, -1 if entry is empty or invalid, non-zero on I2C error.
     */
    static inline int calc_get_hist_entry(const crumbs_device_t *dev,
                                          uint8_t entry_idx,
                                          calc_hist_entry_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev) || entry_idx > 11)
            return -1;
        rc = calc_query_hist_entry(dev, entry_idx);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != CALC_TYPE_ID || reply.opcode != (uint8_t)(CALC_OP_GET_HIST_0 + entry_idx))
            return -1;
        if (reply.data_len < 16)
            return -1; /* Entry is empty or malformed */
        /* Parse: [op:4][a:u32][b:u32][result:u32] */
        out->op[0] = (char)reply.data[0];
        out->op[1] = (char)reply.data[1];
        out->op[2] = (char)reply.data[2];
        out->op[3] = (char)reply.data[3];
        out->op[4] = '\0';
        rc = crumbs_msg_read_u32(reply.data, reply.data_len, 4, &out->a);
        if (rc != 0)
            return rc;
        rc = crumbs_msg_read_u32(reply.data, reply.data_len, 8, &out->b);
        if (rc != 0)
            return rc;
        return crumbs_msg_read_u32(reply.data, reply.data_len, 12, &out->result);
    }

#ifdef __cplusplus
}
#endif

#endif /* CALCULATOR_OPS_H */
