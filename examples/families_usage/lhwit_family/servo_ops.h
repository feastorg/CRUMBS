/**
 * @file servo_ops.h
 * @brief Servo family (type 0x02): two servos on D9-D10 with position, speed
 *        limit and sweep.
 *
 * SET opcodes move and configure; GET opcodes read positions and speeds back
 * via SET_REPLY. Every payload layout is declared once below and packed and
 * unpacked by both sides through the generated codec.
 */

#ifndef SERVO_OPS_H
#define SERVO_OPS_H

#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Identity ------------------------------------------------------------ */

#define SERVO_OPS(X)                                                               \
    X(SERVO_OP_SET_POS, 0x01)   /* [servo_idx:u8][position:u8] */                   \
    X(SERVO_OP_SET_SPEED, 0x02) /* [servo_idx:u8][speed:u8] */                      \
    X(SERVO_OP_SWEEP, 0x03)     /* [servo_idx:u8][enable:u8][min:u8][max:u8][step:u8] */ \
    X(SERVO_OP_GET_POS, 0x80)   /* reply [pos0:u8][pos1:u8] */                      \
    X(SERVO_OP_GET_SPEED, 0x81) /* reply [speed0:u8][speed1:u8] */
CRUMBS_DEFINE_FAMILY(SERVO, 0x02, SERVO_OPS)

/* Module version reported by opcode 0x00 (docs/protocol.md). */
#define SERVO_MODULE_VER_MAJOR 1
#define SERVO_MODULE_VER_MINOR 0
#define SERVO_MODULE_VER_PATCH 0

/* ---- Payloads ------------------------------------------------------------ */

#define SERVO_SET_POS_FIELDS(X) \
    X(u8, servo_idx)            /* 0-1 */ \
    X(u8, position)             /* degrees, 0-180 */
CRUMBS_DEFINE_PAYLOAD(servo_set_pos, 2, SERVO_SET_POS_FIELDS)

#define SERVO_SET_SPEED_FIELDS(X) \
    X(u8, servo_idx)              /* 0-1 */ \
    X(u8, speed)                  /* degrees per update; 0 = instant, 1-20 = limited */
CRUMBS_DEFINE_PAYLOAD(servo_set_speed, 2, SERVO_SET_SPEED_FIELDS)

#define SERVO_SWEEP_FIELDS(X) \
    X(u8, servo_idx)          /* 0-1 */ \
    X(u8, enable)             /* 0 = stop, 1 = sweep */ \
    X(u8, min_pos)            /* degrees */ \
    X(u8, max_pos)            /* degrees */ \
    X(u8, step)               /* degrees per update */
CRUMBS_DEFINE_PAYLOAD(servo_sweep, 5, SERVO_SWEEP_FIELDS)

#define SERVO_POS_RESULT_FIELDS(X) X(u8, pos0) X(u8, pos1) /* degrees */
CRUMBS_DEFINE_PAYLOAD(servo_pos_result, 2, SERVO_POS_RESULT_FIELDS)

#define SERVO_SPEED_RESULT_FIELDS(X) X(u8, speed0) X(u8, speed1) /* degrees per update */
CRUMBS_DEFINE_PAYLOAD(servo_speed_result, 2, SERVO_SPEED_RESULT_FIELDS)

    /* ---- Controller ---------------------------------------------------------- */

    CRUMBS_DEFINE_SEND_OP(servo, set_pos, SERVO_TYPE_ID, SERVO_OP_SET_POS,
                          const servo_set_pos_t *v, servo_set_pos_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(servo, set_speed, SERVO_TYPE_ID, SERVO_OP_SET_SPEED,
                          const servo_set_speed_t *v, servo_set_speed_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(servo, sweep, SERVO_TYPE_ID, SERVO_OP_SWEEP,
                          const servo_sweep_t *v, servo_sweep_pack(&_m, v))
    CRUMBS_DEFINE_GET_OP(servo, pos, SERVO_TYPE_ID, SERVO_OP_GET_POS,
                         servo_pos_result_t, servo_pos_result_unpack)
    CRUMBS_DEFINE_GET_OP(servo, speed, SERVO_TYPE_ID, SERVO_OP_GET_SPEED,
                         servo_speed_result_t, servo_speed_result_unpack)

#ifdef __cplusplus
}
#endif

#endif /* SERVO_OPS_H */
