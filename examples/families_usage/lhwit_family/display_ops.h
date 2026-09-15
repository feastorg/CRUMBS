/**
 * @file display_ops.h
 * @brief Quad 7-segment display family (type 0x04): a 5641AS on a 74HC595.
 *
 * SET opcodes show a number, raw segments or a brightness; one GET reads the
 * display state back via SET_REPLY. Every payload layout is declared once
 * below and packed and unpacked by both sides through the generated codec.
 */

#ifndef DISPLAY_OPS_H
#define DISPLAY_OPS_H

#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Identity ------------------------------------------------------------ */

#define DISPLAY_OPS(X)                                                              \
    X(DISPLAY_OP_SET_NUMBER, 0x01)     /* [number:u16][decimal_pos:u8] */            \
    X(DISPLAY_OP_SET_SEGMENTS, 0x02)   /* [digit0:u8][digit1:u8][digit2:u8][digit3:u8] */ \
    X(DISPLAY_OP_SET_BRIGHTNESS, 0x03) /* [level:u8] */                              \
    X(DISPLAY_OP_CLEAR, 0x04)          /* no payload */                              \
    X(DISPLAY_OP_GET_VALUE, 0x80)      /* reply [number:u16][decimal_pos:u8][brightness:u8] */
CRUMBS_DEFINE_FAMILY(DISPLAY, 0x04, DISPLAY_OPS)

/* Module version reported by opcode 0x00 (docs/protocol.md). */
#define DISPLAY_MODULE_VER_MAJOR 1
#define DISPLAY_MODULE_VER_MINOR 0
#define DISPLAY_MODULE_VER_PATCH 0

/* ---- Payloads ------------------------------------------------------------ */

#define DISPLAY_SET_NUMBER_FIELDS(X) \
    X(u16, number)                   /* 0-9999 */ \
    X(u8, decimal_pos)               /* 0 = none, 1 = leftmost .. 4 = rightmost */
CRUMBS_DEFINE_PAYLOAD(display_set_number, 3, DISPLAY_SET_NUMBER_FIELDS)

/* One segment byte per digit, bit 0 = a .. bit 6 = g, bit 7 = decimal point. */
#define DISPLAY_SET_SEGMENTS_FIELDS(X) X(u8, digit0) X(u8, digit1) X(u8, digit2) X(u8, digit3)
CRUMBS_DEFINE_PAYLOAD(display_set_segments, 4, DISPLAY_SET_SEGMENTS_FIELDS)

#define DISPLAY_SET_BRIGHTNESS_FIELDS(X) X(u8, level) /* 0-10 */
CRUMBS_DEFINE_PAYLOAD(display_set_brightness, 1, DISPLAY_SET_BRIGHTNESS_FIELDS)

#define DISPLAY_VALUE_RESULT_FIELDS(X) \
    X(u16, number)                     /* 0-9999 */ \
    X(u8, decimal_pos)                 /* 0 = none, 1 = leftmost .. 4 = rightmost */ \
    X(u8, brightness)                  /* 0-10 */
CRUMBS_DEFINE_PAYLOAD(display_value_result, 4, DISPLAY_VALUE_RESULT_FIELDS)

    /* ---- Controller ---------------------------------------------------------- */

    CRUMBS_DEFINE_SEND_OP(display, set_number, DISPLAY_TYPE_ID, DISPLAY_OP_SET_NUMBER,
                          const display_set_number_t *v, display_set_number_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(display, set_segments, DISPLAY_TYPE_ID, DISPLAY_OP_SET_SEGMENTS,
                          const display_set_segments_t *v, display_set_segments_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(display, set_brightness, DISPLAY_TYPE_ID, DISPLAY_OP_SET_BRIGHTNESS,
                          const display_set_brightness_t *v, display_set_brightness_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP_0(display, clear, DISPLAY_TYPE_ID, DISPLAY_OP_CLEAR)
    CRUMBS_DEFINE_GET_OP(display, value, DISPLAY_TYPE_ID, DISPLAY_OP_GET_VALUE,
                         display_value_result_t, display_value_result_unpack)

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_OPS_H */
