/**
 * @file led_ops.h
 * @brief LED array family (type 0x01): four LEDs on D4-D7, static or blinking.
 *
 * SET opcodes set states and blink patterns; GET opcodes read them back via
 * SET_REPLY. Every payload layout is declared once below and packed and
 * unpacked by both sides through the generated codec.
 */

#ifndef LED_OPS_H
#define LED_OPS_H

#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Identity ------------------------------------------------------------ */

#define LED_OPS(X)                                                             \
    X(LED_OP_SET_ALL, 0x01)   /* [mask:u8] bits 0-3 = LEDs 0-3, 1 = on */       \
    X(LED_OP_SET_ONE, 0x02)   /* [led_idx:u8][state:u8] */                      \
    X(LED_OP_BLINK, 0x03)     /* [led_idx:u8][enable:u8][period_ms:u16] */      \
    X(LED_OP_GET_STATE, 0x80) /* reply [states:u8] */                          \
    X(LED_OP_GET_BLINK, 0x81) /* reply [enable:u8][period_ms:u16] x 4 LEDs */
CRUMBS_DEFINE_FAMILY(LED, 0x01, LED_OPS)

/* Module version reported by opcode 0x00 (docs/protocol.md). */
#define LED_MODULE_VER_MAJOR 1
#define LED_MODULE_VER_MINOR 0
#define LED_MODULE_VER_PATCH 0

/* ---- Payloads ------------------------------------------------------------ */

#define LED_SET_ALL_FIELDS(X) X(u8, mask) /* bits 0-3 = LEDs 0-3, 1 = on */
CRUMBS_DEFINE_PAYLOAD(led_set_all, 1, LED_SET_ALL_FIELDS)

#define LED_SET_ONE_FIELDS(X) \
    X(u8, led_idx)            /* 0-3 */ \
    X(u8, state)              /* 0 = off, 1 = on */
CRUMBS_DEFINE_PAYLOAD(led_set_one, 2, LED_SET_ONE_FIELDS)

#define LED_BLINK_FIELDS(X) \
    X(u8, led_idx)          /* 0-3 */ \
    X(u8, enable)           /* 0 = steady, 1 = blink */ \
    X(u16, period_ms)       /* full on-off cycle */
CRUMBS_DEFINE_PAYLOAD(led_blink, 4, LED_BLINK_FIELDS)

#define LED_STATE_RESULT_FIELDS(X) X(u8, states) /* bits 0-3 = LEDs 0-3 */
CRUMBS_DEFINE_PAYLOAD(led_state_result, 1, LED_STATE_RESULT_FIELDS)

    /**
     * @brief Reply to LED_OP_GET_BLINK: per-LED blink configuration.
     *
     * Wire: [enable:u8][period_ms:u16] repeated for LEDs 0-3, 12 bytes.
     * Written by hand because the codec has no array fields.
     */
    typedef struct
    {
        uint8_t enable[4];     /**< 0 = steady, 1 = blink. */
        uint16_t period_ms[4]; /**< Full on-off cycle. */
    } led_blink_result_t;

    /** @brief Append a led_blink_result_t to @p msg. */
    static inline int led_blink_result_pack(crumbs_message_t *msg, const led_blink_result_t *v)
    {
        int i;
        if (!msg || !v)
            return -1;
        for (i = 0; i < 4; i++)
        {
            if (crumbs_msg_add_u8(msg, v->enable[i]) != 0 || crumbs_msg_add_u16(msg, v->period_ms[i]) != 0)
                return -1;
        }
        return 0;
    }

    /** @brief Read a led_blink_result_t from the front of @p data. */
    static inline int led_blink_result_unpack(const uint8_t *data, size_t len, led_blink_result_t *v)
    {
        int i;
        if (!data || !v || len < 12u)
            return -1;
        for (i = 0; i < 4; i++)
        {
            v->enable[i] = data[i * 3];
            v->period_ms[i] = (uint16_t)(data[i * 3 + 1] | ((uint16_t)data[i * 3 + 2] << 8));
        }
        return 0;
    }

    /* ---- Controller ---------------------------------------------------------- */

    CRUMBS_DEFINE_SEND_OP(led, set_all, LED_TYPE_ID, LED_OP_SET_ALL,
                          const led_set_all_t *v, led_set_all_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(led, set_one, LED_TYPE_ID, LED_OP_SET_ONE,
                          const led_set_one_t *v, led_set_one_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(led, blink, LED_TYPE_ID, LED_OP_BLINK,
                          const led_blink_t *v, led_blink_pack(&_m, v))
    CRUMBS_DEFINE_GET_OP(led, state, LED_TYPE_ID, LED_OP_GET_STATE,
                         led_state_result_t, led_state_result_unpack)
    CRUMBS_DEFINE_GET_OP(led, blink, LED_TYPE_ID, LED_OP_GET_BLINK,
                         led_blink_result_t, led_blink_result_unpack)

#ifdef __cplusplus
}
#endif

#endif /* LED_OPS_H */
