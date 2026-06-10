/**
 * @file servo_ops.h
 * @brief Servo control command definitions (Type ID 0x02)
 *
 * This file defines commands for controlling a 2-servo peripheral.
 * The peripheral controls servo motors (D9-D10) with position control,
 * speed limiting, and sweep patterns.
 *
 * Pattern: Position-control interface
 * - SET operations (0x01-0x03): Control servo positions and sweep modes
 * - GET operations (0x80-0x81): Query current positions and settings
 *
 * Commands:
 * - SERVO_OP_SET_POS:    Set servo position immediately
 * - SERVO_OP_SET_SPEED:  Set servo movement speed limit
 * - SERVO_OP_SWEEP:      Configure sweep pattern
 * - SERVO_OP_GET_POS:    Request current positions (via SET_REPLY)
 * - SERVO_OP_GET_SPEED:  Request speed limits (via SET_REPLY)
 */

#ifndef SERVO_OPS_H
#define SERVO_OPS_H

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

/** @brief Type ID for servo controller device. */
#define SERVO_TYPE_ID 0x02

/* Module protocol version (per versioning.md convention) */
#define SERVO_MODULE_VER_MAJOR 1
#define SERVO_MODULE_VER_MINOR 0
#define SERVO_MODULE_VER_PATCH 0

/* ============================================================================
 * Command Definitions: SET Operations (Control Servos)
 * ============================================================================ */

/**
 * @brief Set servo position immediately.
 * Payload: [servo_idx:u8][position:u8]
 *   - servo_idx: Servo index (0-1 for D9-D10)
 *   - position: Target position (0-180 degrees)
 */
#define SERVO_OP_SET_POS 0x01

/**
 * @brief Set servo movement speed limit.
 * Payload: [servo_idx:u8][speed:u8]
 *   - servo_idx: Servo index (0-1)
 *   - speed: Degrees per update (0=instant, 1-20=limited speed)
 */
#define SERVO_OP_SET_SPEED 0x02

/**
 * @brief Configure servo sweep pattern.
 * Payload: [servo_idx:u8][enable:u8][min_pos:u8][max_pos:u8][step:u8]
 *   - servo_idx: Servo index (0-1)
 *   - enable: 0=disable sweep, 1=enable sweep
 *   - min_pos: Minimum position (0-180 degrees)
 *   - max_pos: Maximum position (0-180 degrees)
 *   - step: Degrees to move per sweep update
 */
#define SERVO_OP_SWEEP 0x03

/* ============================================================================
 * Command Definitions: GET Operations (Query State via SET_REPLY)
 * ============================================================================ */

/**
 * @brief Request current servo positions.
 * Payload: none
 * Reply: [pos0:u8][pos1:u8] (current positions in degrees)
 */
#define SERVO_OP_GET_POS 0x80

/**
 * @brief Request servo speed limits.
 * Payload: none
 * Reply: [speed0:u8][speed1:u8] (speed limits in degrees/update)
 */
#define SERVO_OP_GET_SPEED 0x81

    /* ============================================================================
     * Controller Side: Command Senders
     * ============================================================================ */

    /**
     * @brief Set servo position.
     *
     * @param dev       Bound device handle (see crumbs_device_t).
     * @param servo_idx Servo index (0-1).
     * @param position  Target position (0-180 degrees).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_send_set_pos(const crumbs_device_t *dev,
                                         uint8_t servo_idx,
                                         uint8_t position)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, SERVO_TYPE_ID, SERVO_OP_SET_POS);
        crumbs_msg_add_u8(&msg, servo_idx);
        crumbs_msg_add_u8(&msg, position);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Set servo movement speed limit.
     *
     * @param dev       Bound device handle (see crumbs_device_t).
     * @param servo_idx Servo index (0-1).
     * @param speed     Speed limit (0=instant, 1-20=limited).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_send_set_speed(const crumbs_device_t *dev,
                                           uint8_t servo_idx,
                                           uint8_t speed)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, SERVO_TYPE_ID, SERVO_OP_SET_SPEED);
        crumbs_msg_add_u8(&msg, servo_idx);
        crumbs_msg_add_u8(&msg, speed);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Configure servo sweep pattern.
     *
     * @param dev       Bound device handle (see crumbs_device_t).
     * @param servo_idx Servo index (0-1).
     * @param enable    Enable sweep (0=OFF, 1=ON).
     * @param min_pos   Minimum position (0-180 degrees).
     * @param max_pos   Maximum position (0-180 degrees).
     * @param step      Step size (degrees per update).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_send_sweep(const crumbs_device_t *dev,
                                       uint8_t servo_idx,
                                       uint8_t enable,
                                       uint8_t min_pos,
                                       uint8_t max_pos,
                                       uint8_t step)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, SERVO_TYPE_ID, SERVO_OP_SWEEP);
        crumbs_msg_add_u8(&msg, servo_idx);
        crumbs_msg_add_u8(&msg, enable);
        crumbs_msg_add_u8(&msg, min_pos);
        crumbs_msg_add_u8(&msg, max_pos);
        crumbs_msg_add_u8(&msg, step);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query current servo positions (peripheral will respond on next I2C read).
     *
     * @internal Used by servo_get_pos(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_query_pos(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, SERVO_OP_GET_POS);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Query servo speed limits (peripheral will respond on next I2C read).
     *
     * @internal Used by servo_get_speed(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_query_speed(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, SERVO_OP_GET_SPEED);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /* ============================================================================
     * Controller Side: Combined Query + Read (Receiver API)
     * ============================================================================ */

    /**
     * @brief Result struct for SERVO_OP_GET_POS.
     *
     * Current position of each servo in degrees (0-180).
     */
    typedef struct
    {
        uint8_t pos[2]; /**< Current positions in degrees, indexed by servo (0-1). */
    } servo_pos_result_t;

    /**
     * @brief Result struct for SERVO_OP_GET_SPEED.
     *
     * Speed limit of each servo in degrees/update (0=instant).
     */
    typedef struct
    {
        uint8_t speed[2]; /**< Speed limits, indexed by servo (0-1). */
    } servo_speed_result_t;

    /**
     * @brief Combined SET_REPLY query + read + parse for servo positions.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_get_pos(const crumbs_device_t *dev, servo_pos_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = servo_query_pos(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != SERVO_TYPE_ID || reply.opcode != SERVO_OP_GET_POS)
            return -1;
        rc = crumbs_msg_read_u8(reply.data, reply.data_len, 0, &out->pos[0]);
        if (rc != 0)
            return rc;
        return crumbs_msg_read_u8(reply.data, reply.data_len, 1, &out->pos[1]);
    }

    /**
     * @brief Combined SET_REPLY query + read + parse for servo speed limits.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int servo_get_speed(const crumbs_device_t *dev, servo_speed_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = servo_query_speed(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != SERVO_TYPE_ID || reply.opcode != SERVO_OP_GET_SPEED)
            return -1;
        rc = crumbs_msg_read_u8(reply.data, reply.data_len, 0, &out->speed[0]);
        if (rc != 0)
            return rc;
        return crumbs_msg_read_u8(reply.data, reply.data_len, 1, &out->speed[1]);
    }

#ifdef __cplusplus
}
#endif

#endif /* SERVO_OPS_H */
