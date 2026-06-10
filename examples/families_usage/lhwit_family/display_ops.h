/**
 * @file display_ops.h
 * @brief Quad 7-segment display command definitions (Type ID 0x04)
 *
 * This file defines commands for controlling a 4-digit 7-segment display
 * peripheral (5641AS or compatible). The peripheral can display numbers,
 * custom patterns, and individual segments.
 *
 * Pattern: Display control interface
 * - SET operations (0x01-0x04): Display numbers, patterns, and segment control
 * - GET operations (0x80): Query current displayed value via SET_REPLY
 *
 * Commands:
 * - DISPLAY_OP_SET_NUMBER:    Display a number (0-9999) with optional decimal point
 * - DISPLAY_OP_SET_SEGMENTS:  Set custom segment patterns for all 4 digits
 * - DISPLAY_OP_SET_BRIGHTNESS: Control display brightness (if supported)
 * - DISPLAY_OP_CLEAR:         Clear the display
 * - DISPLAY_OP_GET_VALUE:     Request current displayed number (via SET_REPLY)
 */

#ifndef DISPLAY_OPS_H
#define DISPLAY_OPS_H

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

/** @brief Type ID for quad 7-segment display device. */
#define DISPLAY_TYPE_ID 0x04

/* Module protocol version (per versioning.md convention) */
#define DISPLAY_MODULE_VER_MAJOR 1
#define DISPLAY_MODULE_VER_MINOR 0
#define DISPLAY_MODULE_VER_PATCH 0

/* ============================================================================
 * Command Definitions: SET Operations (Control Display)
 * ============================================================================ */

/**
 * @brief Display a number with optional decimal point.
 * Payload: [number:u16][decimal_pos:u8]
 *   - number: Value to display (0-9999)
 *   - decimal_pos: Decimal point position (0=none, 1=digit1, 2=digit2, 3=digit3, 4=digit4)
 *     Position 1 is leftmost digit, 4 is rightmost digit
 *
 * Examples:
 *   number=1234, decimal_pos=0 -> "1234" (no decimal)
 *   number=1234, decimal_pos=3 -> "123.4" (decimal on digit 3)
 *   number=1234, decimal_pos=2 -> "12.34" (decimal on digit 2)
 *   number=42, decimal_pos=0   -> "  42" (right-aligned, leading spaces)
 */
#define DISPLAY_OP_SET_NUMBER 0x01

/**
 * @brief Set custom segment patterns for all 4 digits.
 * Payload: [digit0:u8][digit1:u8][digit2:u8][digit3:u8]
 *   - Each byte is a segment pattern (bit=1 means segment on)
 *   - Bit mapping (bit 7->0): [a][b][c][d][e][f][g][dot]
 *     where a-g are the 7 segments, dot is decimal point
 *
 * Example segment pattern:
 *       a (bit 7)
 *  f(2)   b(6)
 *       g (bit 1)
 *  e(3)   c(5)
 *       d (bit 4)  dot(bit 0)
 */
#define DISPLAY_OP_SET_SEGMENTS 0x02

/**
 * @brief Set display brightness.
 * Payload: [level:u8]
 *   - level: Brightness level (0=off, 1-10=dimmest to brightest)
 *
 * Note: Actual implementation depends on hardware (PWM, etc.)
 */
#define DISPLAY_OP_SET_BRIGHTNESS 0x03

/**
 * @brief Clear the display (all segments off).
 * Payload: none
 */
#define DISPLAY_OP_CLEAR 0x04

/* ============================================================================
 * Command Definitions: GET Operations (Query State via SET_REPLY)
 * ============================================================================ */

/**
 * @brief Request current displayed number.
 * Payload: none
 * Reply: [number:u16][decimal_pos:u8][brightness:u8]
 *   - number: Currently displayed value
 *   - decimal_pos: Which digit has decimal point (0=none, 1=leftmost, 4=rightmost)
 *   - brightness: Current brightness level (0-10)
 */
#define DISPLAY_OP_GET_VALUE 0x80

    /* ============================================================================
     * Low-Level Message Builders
     *
     * These functions construct a crumbs_message_t in-place for callers that need
     * to inspect or modify a message before sending it manually with
     * crumbs_controller_send(). For standard fire-and-forget usage prefer the
     * display_send_*() wrappers in the "Controller Side: Command Senders" section
     * below, which match the pattern used by all other lhwit_family ops headers.
     * ============================================================================ */

    /**
     * @brief Build a SET_NUMBER command message.
     *
     * @param msg         Pointer to message structure to initialize
     * @param number      Number to display (0-9999)
     * @param decimal_pos Decimal point on which digit (0=none, 1=leftmost, 4=rightmost)
     * @return 0 on success, negative on error
     */
    static inline int display_build_set_number(crumbs_message_t *msg, uint16_t number, uint8_t decimal_pos)
    {
        if (!msg)
            return -1;

        crumbs_msg_init(msg, DISPLAY_TYPE_ID, DISPLAY_OP_SET_NUMBER);
        crumbs_msg_add_u16(msg, number);
        crumbs_msg_add_u8(msg, decimal_pos);

        return 0;
    }

    /**
     * @brief Build a SET_SEGMENTS command message.
     *
     * @param msg      Pointer to message structure to initialize
     * @param segments Array of 4 segment patterns (one per digit)
     * @return 0 on success, negative on error
     */
    static inline int display_build_set_segments(crumbs_message_t *msg, const uint8_t segments[4])
    {
        if (!msg || !segments)
            return -1;

        crumbs_msg_init(msg, DISPLAY_TYPE_ID, DISPLAY_OP_SET_SEGMENTS);
        for (int i = 0; i < 4; i++)
        {
            crumbs_msg_add_u8(msg, segments[i]);
        }

        return 0;
    }

    /**
     * @brief Build a SET_BRIGHTNESS command message.
     *
     * @param msg   Pointer to message structure to initialize
     * @param level Brightness level (0-10)
     * @return 0 on success, negative on error
     */
    static inline int display_build_set_brightness(crumbs_message_t *msg, uint8_t level)
    {
        if (!msg)
            return -1;

        crumbs_msg_init(msg, DISPLAY_TYPE_ID, DISPLAY_OP_SET_BRIGHTNESS);
        crumbs_msg_add_u8(msg, level);

        return 0;
    }

    /**
     * @brief Build a CLEAR command message.
     *
     * @param msg Pointer to message structure to initialize
     * @return 0 on success, negative on error
     */
    static inline int display_build_clear(crumbs_message_t *msg)
    {
        if (!msg)
            return -1;

        crumbs_msg_init(msg, DISPLAY_TYPE_ID, DISPLAY_OP_CLEAR);

        return 0;
    }

    /**
     * @brief Build a GET_VALUE query message (for SET_REPLY pattern).
     *
     * @param msg Pointer to message structure to initialize
     * @return 0 on success, negative on error
     */
    static inline int display_build_get_value(crumbs_message_t *msg)
    {
        if (!msg)
            return -1;

        crumbs_msg_init(msg, DISPLAY_TYPE_ID, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(msg, DISPLAY_OP_GET_VALUE);

        return 0;
    }

    /**
     * @brief Parse GET_VALUE reply payload.
     *
     * @param data       Reply payload data
     * @param data_len   Reply payload length
     * @param number     Output: displayed number (may be NULL)
     * @param decimal    Output: decimal position (may be NULL)
     * @param brightness Output: brightness level (may be NULL)
     * @return 0 on success, negative on error
     */
    static inline int display_parse_get_value(const uint8_t *data, uint8_t data_len,
                                              uint16_t *number, uint8_t *decimal, uint8_t *brightness)
    {
        if (!data || data_len < 4)
            return -1;

        if (number)
            *number = data[0] | ((uint16_t)data[1] << 8);
        if (decimal)
            *decimal = data[2];
        if (brightness)
            *brightness = data[3];

        return 0;
    }

    /* ============================================================================
     * Controller Side: Command Senders
     *
     * Direct-send wrappers matching the led_send_*() / servo_send_*() pattern.
     * Each function builds and sends a message in a single call.
     * ============================================================================ */

    /**
     * @brief Display a number with optional decimal point.
     *
     * @param dev         Bound device handle (see crumbs_device_t).
     * @param number      Number to display (0-9999).
     * @param decimal_pos Decimal point digit (0=none, 1=leftmost, 4=rightmost).
     * @return 0 on success, non-zero on error.
     */
    static inline int display_send_set_number(const crumbs_device_t *dev,
                                              uint16_t number,
                                              uint8_t decimal_pos)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, DISPLAY_TYPE_ID, DISPLAY_OP_SET_NUMBER);
        crumbs_msg_add_u16(&msg, number);
        crumbs_msg_add_u8(&msg, decimal_pos);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Set custom segment patterns for all 4 digits.
     *
     * @param dev      Bound device handle (see crumbs_device_t).
     * @param segments Array of 4 segment patterns (digit 0-3).
     * @return 0 on success, non-zero on error.
     */
    static inline int display_send_set_segments(const crumbs_device_t *dev,
                                                const uint8_t segments[4])
    {
        crumbs_message_t msg;
        int i;
        if (!crumbs_ops_can_send(dev) || !segments)
            return -1;
        crumbs_msg_init(&msg, DISPLAY_TYPE_ID, DISPLAY_OP_SET_SEGMENTS);
        for (i = 0; i < 4; i++)
            crumbs_msg_add_u8(&msg, segments[i]);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Set display brightness.
     *
     * @param dev   Bound device handle (see crumbs_device_t).
     * @param level Brightness level (0=off, 1-10=dimmest to brightest).
     * @return 0 on success, non-zero on error.
     */
    static inline int display_send_set_brightness(const crumbs_device_t *dev, uint8_t level)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, DISPLAY_TYPE_ID, DISPLAY_OP_SET_BRIGHTNESS);
        crumbs_msg_add_u8(&msg, level);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Clear the display (all segments off).
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int display_send_clear(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, DISPLAY_TYPE_ID, DISPLAY_OP_CLEAR);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /* ============================================================================
     * Controller Side: Query Sender + Combined Query + Read (Receiver API)
     * ============================================================================ */

    /**
     * @brief Send a SET_REPLY query for current display value (peripheral will respond on next I2C read).
     *
     * @internal Used by display_get_value(); prefer that function for combined query+read.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @return 0 on success, non-zero on error.
     */
    static inline int display_query_value(const crumbs_device_t *dev)
    {
        crumbs_message_t msg;
        if (!crumbs_ops_can_send(dev))
            return -1;
        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, DISPLAY_OP_GET_VALUE);
        return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
    }

    /**
     * @brief Result struct for DISPLAY_OP_GET_VALUE.
     */
    typedef struct
    {
        uint16_t number;     /**< Currently displayed number (0-9999). */
        uint8_t decimal_pos; /**< Decimal position (0=none, 1=leftmost, 4=rightmost). */
        uint8_t brightness;  /**< Brightness level (0-10). */
    } display_value_result_t;

    /**
     * @brief Combined SET_REPLY query + read + parse for current display value.
     *
     * @param dev  Bound device handle (see crumbs_device_t).
     * @param out  Output struct (must not be NULL).
     * @return 0 on success, non-zero on error.
     */
    static inline int display_get_value(const crumbs_device_t *dev, display_value_result_t *out)
    {
        crumbs_message_t reply;
        int rc;
        if (!out || !crumbs_ops_can_get(dev))
            return -1;
        rc = display_query_value(dev);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read(dev->ctx, dev->addr, &reply, dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        if (reply.type_id != DISPLAY_TYPE_ID || reply.opcode != DISPLAY_OP_GET_VALUE)
            return -1;
        /* Reply: [number:u16][decimal_pos:u8][brightness:u8] */
        return display_parse_get_value(reply.data, reply.data_len,
                                       &out->number, &out->decimal_pos, &out->brightness);
    }

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_OPS_H */
