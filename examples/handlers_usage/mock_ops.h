/**
 * @file mock_ops.h
 * @brief Mock device command definitions for handler pattern demonstration
 *
 * This file demonstrates the pattern for defining device commands.
 * Copy and modify this template for your own device types.
 *
 * This header defines a mock device (Type ID 0x10) for demonstrating
 * the CRUMBS handler pattern without hardware complexity.
 *
 * Commands:
 * - MOCK_OP_ECHO:         Echo data back (store for later retrieval)
 * - MOCK_OP_SET_HEARTBEAT: Set LED heartbeat period
 * - MOCK_OP_TOGGLE:       Toggle heartbeat enable/disable
 * - MOCK_OP_GET_ECHO:     Request stored echo data (via SET_REPLY)
 * - MOCK_OP_GET_STATUS:   Request state and heartbeat period (via SET_REPLY)
 * - MOCK_OP_GET_INFO:     Request device info (via SET_REPLY)
 */

#ifndef MOCK_OPS_H
#define MOCK_OPS_H

#include "crumbs.h"
#include "crumbs_message_helpers.h"
#include "crumbs_ops.h"

#include <string.h> /* memcpy */

#ifdef __cplusplus
extern "C"
{
#endif

/* ============================================================================
 * Device Identity
 * ============================================================================ */

/** @brief Type ID for mock demonstration device. */
#define MOCK_TYPE_ID 0x10

/* ============================================================================
 * Command Definitions
 * ============================================================================ */

/**
 * @brief Echo operation - stores data for later retrieval.
 * Payload (SET): [data bytes...]
 * Reply (GET via SET_REPLY): [echoed data bytes...]
 */
#define MOCK_OP_ECHO 0x01

/**
 * @brief Set heartbeat operation - sets LED heartbeat period.
 * Payload: [period_ms:u16] (little-endian, milliseconds between pulses)
 */
#define MOCK_OP_SET_HEARTBEAT 0x02

/**
 * @brief Toggle operation - enables/disables heartbeat.
 * Payload: none
 */
#define MOCK_OP_TOGGLE 0x03

/**
 * @brief Request stored echo data.
 * Payload: none (reply contains previously stored echo data)
 */
#define MOCK_OP_GET_ECHO 0x80

/**
 * @brief Request current status (state + heartbeat period).
 * Payload: none (reply contains [state:u8, period_ms:u16])
 */
#define MOCK_OP_GET_STATUS 0x81

/**
 * @brief Request device info (version string).
 * Payload: none (reply contains device info string)
 * Note: Also returned as default (opcode 0) reply.
 */
#define MOCK_OP_GET_INFO 0x82

    /* ============================================================================
     * Controller Side: Command Senders
     *
     * These functions build and send commands to a mock peripheral.
     * ============================================================================ */

    /**
     * @brief Send echo data to peripheral (stores for later retrieval).
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param data Data bytes to echo.
     * @param len  Number of data bytes (0-27).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_send_echo(const crumbs_device_t *dev,
                                     const uint8_t *data,
                                     uint8_t len)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev) || (len > 0 && !data))
            return -1;
        crumbs_msg_init(&msg, MOCK_TYPE_ID, MOCK_OP_ECHO);
        for (uint8_t i = 0; i < len; i++)
        {
            crumbs_msg_add_u8(&msg, data[i]);
        }
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Set LED heartbeat period.
     *
     * @param dev       Bound device handle (see crumbs_device_t).
     * @param period_ms Heartbeat period in milliseconds (0 = off, 1-65535 = pulse period).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_send_heartbeat(const crumbs_device_t *dev, uint16_t period_ms)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, MOCK_TYPE_ID, MOCK_OP_SET_HEARTBEAT);
        crumbs_msg_add_u16(&msg, period_ms);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Send toggle command to peripheral (flips state bit).
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_send_toggle(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, MOCK_TYPE_ID, MOCK_OP_TOGGLE);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query stored echo data (peripheral will respond on next I2C read).
     *
     * @internal Used by mock_get_echo(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_query_echo(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, MOCK_TYPE_ID, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, MOCK_OP_GET_ECHO);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query current status (state + heartbeat period).
     *
     * @internal Used by mock_get_status(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_query_status(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, MOCK_TYPE_ID, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, MOCK_OP_GET_STATUS);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query device info (peripheral will respond on next I2C read).
     *
     * @internal Used by mock_get_info(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_query_info(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, MOCK_TYPE_ID, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, MOCK_OP_GET_INFO);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /* ============================================================================
     * Controller Side: Combined Query + Read (Receiver API)
     * ============================================================================ */

    /**
     * @brief Result struct for MOCK_OP_GET_ECHO.
     *
     * Contains the echo data stored by the previous MOCK_OP_ECHO command.
     * Length is in bytes; data is the raw echo payload (up to CRUMBS_MAX_PAYLOAD).
     */
    typedef struct
    {
        uint8_t data[CRUMBS_MAX_PAYLOAD]; /**< Echoed payload bytes. */
        uint8_t len;                       /**< Number of valid bytes in data. */
    } mock_echo_result_t;

    /**
     * @brief Result struct for MOCK_OP_GET_STATUS.
     */
    typedef struct
    {
        uint8_t  state;     /**< Current state (heartbeat enable flag). */
        uint16_t period_ms; /**< Heartbeat period in milliseconds. */
    } mock_status_result_t;

    /**
     * @brief Result struct for MOCK_OP_GET_INFO.
     */
    typedef struct
    {
        char    info[CRUMBS_MAX_PAYLOAD + 1]; /**< Null-terminated device info string. */
        uint8_t len;                          /**< Length of info string (without null terminator). */
    } mock_info_result_t;

    /**
     * @brief Combined SET_REPLY query + read + parse for echo data.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_get_echo(const crumbs_device_t *dev, mock_echo_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = mock_query_echo(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != MOCK_TYPE_ID || reply.opcode != MOCK_OP_GET_ECHO)
            return -1;
        out->len = reply.data_len;
        if (reply.data_len > 0)
            memcpy(out->data, reply.data, reply.data_len);
        return 0;
    }

    /**
     * @brief Combined SET_REPLY query + read + parse for device status.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_get_status(const crumbs_device_t *dev, mock_status_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = mock_query_status(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != MOCK_TYPE_ID || reply.opcode != MOCK_OP_GET_STATUS)
            return -1;
        rc = crumbs_msg_read_u8(reply.data, reply.data_len, 0, &out->state);
        if (rc != 0)
            return rc;
        return crumbs_msg_read_u16(reply.data, reply.data_len, 1, &out->period_ms);
    }

    /**
     * @brief Combined SET_REPLY query + read + parse for device info string.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int mock_get_info(const crumbs_device_t *dev, mock_info_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = mock_query_info(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != MOCK_TYPE_ID || reply.opcode != MOCK_OP_GET_INFO)
            return -1;
        out->len = reply.data_len;
        if (reply.data_len > 0)
            memcpy(out->info, reply.data, reply.data_len);
        out->info[out->len] = '\0';
        return 0;
    }

#ifdef __cplusplus
}
#endif

#endif /* MOCK_OPS_H */
