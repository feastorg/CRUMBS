/**
 * @file calculator_ops.h
 * @brief Calculator family (type 0x03): 32-bit integer arithmetic with a
 *        12-entry history.
 *
 * SET opcodes run an operation on two operands; GET opcodes read the last
 * result, the history metadata and one history entry each via SET_REPLY.
 * Every payload layout is declared once below and packed and unpacked by
 * both sides. The history entry carries a 4-byte operation name, which the
 * generated codec has no field type for, so its codec is written by hand.
 */

#ifndef CALCULATOR_OPS_H
#define CALCULATOR_OPS_H

#include "crumbs_ops.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Identity ------------------------------------------------------------ */

/* Every SET carries [a:u32][b:u32]; every GET carries nothing. */
#define CALC_OPS(X)                                                                  \
    X(CALC_OP_ADD, 0x01)           /* result = a + b */                               \
    X(CALC_OP_SUB, 0x02)           /* result = a - b */                               \
    X(CALC_OP_MUL, 0x03)           /* result = a * b */                               \
    X(CALC_OP_DIV, 0x04)           /* result = a / b; 0xFFFFFFFF when b is 0 */        \
    X(CALC_OP_GET_RESULT, 0x80)    /* reply [result:u32] */                           \
    X(CALC_OP_GET_HIST_META, 0x81) /* reply [count:u8][write_pos:u8] */              \
    X(CALC_OP_GET_HIST_0, 0x82)    /* reply calc_hist_entry_t, or empty if unused */ \
    X(CALC_OP_GET_HIST_1, 0x83)                                                       \
    X(CALC_OP_GET_HIST_2, 0x84)                                                       \
    X(CALC_OP_GET_HIST_3, 0x85)                                                       \
    X(CALC_OP_GET_HIST_4, 0x86)                                                       \
    X(CALC_OP_GET_HIST_5, 0x87)                                                       \
    X(CALC_OP_GET_HIST_6, 0x88)                                                       \
    X(CALC_OP_GET_HIST_7, 0x89)                                                       \
    X(CALC_OP_GET_HIST_8, 0x8A)                                                       \
    X(CALC_OP_GET_HIST_9, 0x8B)                                                       \
    X(CALC_OP_GET_HIST_10, 0x8C)                                                      \
    X(CALC_OP_GET_HIST_11, 0x8D)
CRUMBS_DEFINE_FAMILY(CALC, 0x03, CALC_OPS)

#define CALC_HISTORY_SIZE 12

/* Module version reported by opcode 0x00 (docs/protocol.md). */
#define CALC_MODULE_VER_MAJOR 1
#define CALC_MODULE_VER_MINOR 0
#define CALC_MODULE_VER_PATCH 0

/* ---- Payloads ------------------------------------------------------------ */

#define CALC_OPERANDS_FIELDS(X) X(u32, a) X(u32, b)
CRUMBS_DEFINE_PAYLOAD(calc_operands, 8, CALC_OPERANDS_FIELDS)

#define CALC_RESULT_FIELDS(X) X(u32, result)
CRUMBS_DEFINE_PAYLOAD(calc_result, 4, CALC_RESULT_FIELDS)

#define CALC_HIST_META_FIELDS(X) \
    X(u8, count)                 /* valid entries, 0-12 */ \
    X(u8, write_pos)             /* next slot, 0-11, circular */
CRUMBS_DEFINE_PAYLOAD(calc_hist_meta, 2, CALC_HIST_META_FIELDS)

    /**
     * @brief One history entry: [op:4 bytes][a:u32][b:u32][result:u32], 16 bytes.
     *
     * `op` is "ADD", "SUB", "MUL" or "DIV", NUL-padded on the wire and
     * NUL-terminated here.
     */
    typedef struct
    {
        char op[5];
        uint32_t a;
        uint32_t b;
        uint32_t result;
    } calc_hist_entry_t;

    enum
    {
        calc_hist_entry_wire_size = 16
    };

    /** @brief Append a calc_hist_entry_t to @p msg. */
    static inline int calc_hist_entry_pack(crumbs_message_t *msg, const calc_hist_entry_t *v)
    {
        uint8_t op[4] = {0, 0, 0, 0};
        int i;
        if (!msg || !v)
            return -1;
        for (i = 0; i < 4 && v->op[i] != '\0'; i++)
            op[i] = (uint8_t)v->op[i];
        if (crumbs_msg_add_bytes(msg, op, 4) != 0 || crumbs_msg_add_u32(msg, v->a) != 0 ||
            crumbs_msg_add_u32(msg, v->b) != 0 || crumbs_msg_add_u32(msg, v->result) != 0)
            return -1;
        return 0;
    }

    /** @brief Read a calc_hist_entry_t from the front of @p data; -1 if shorter than 16. */
    static inline int calc_hist_entry_unpack(const uint8_t *data, size_t len, calc_hist_entry_t *v)
    {
        int i;
        if (!data || !v || len < (size_t)calc_hist_entry_wire_size)
            return -1;
        for (i = 0; i < 4; i++)
            v->op[i] = (char)data[i];
        v->op[4] = '\0';
        if (crumbs_msg_read_u32(data, (uint8_t)len, 4, &v->a) != 0 ||
            crumbs_msg_read_u32(data, (uint8_t)len, 8, &v->b) != 0 ||
            crumbs_msg_read_u32(data, (uint8_t)len, 12, &v->result) != 0)
            return -1;
        return 0;
    }

    /* ---- Controller ---------------------------------------------------------- */

    CRUMBS_DEFINE_SEND_OP(calc, add, CALC_TYPE_ID, CALC_OP_ADD,
                          const calc_operands_t *v, calc_operands_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(calc, sub, CALC_TYPE_ID, CALC_OP_SUB,
                          const calc_operands_t *v, calc_operands_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(calc, mul, CALC_TYPE_ID, CALC_OP_MUL,
                          const calc_operands_t *v, calc_operands_pack(&_m, v))
    CRUMBS_DEFINE_SEND_OP(calc, div, CALC_TYPE_ID, CALC_OP_DIV,
                          const calc_operands_t *v, calc_operands_pack(&_m, v))
    CRUMBS_DEFINE_GET_OP(calc, result, CALC_TYPE_ID, CALC_OP_GET_RESULT,
                         calc_result_t, calc_result_unpack)
    CRUMBS_DEFINE_GET_OP(calc, hist_meta, CALC_TYPE_ID, CALC_OP_GET_HIST_META,
                         calc_hist_meta_t, calc_hist_meta_unpack)

    /**
     * @brief Query, wait and read history entry @p entry_idx (0-11).
     *
     * A parameterised GET: the opcode is CALC_OP_GET_HIST_0 + entry_idx, so
     * CRUMBS_DEFINE_GET_OP does not fit and the query is written out.
     * Returns -1 for a bad index, an unbound device, a NULL @p out or an
     * unused entry (the peripheral replies with no payload); otherwise the
     * send/read code, CRUMBS_RX_REPLY_MISMATCH, or 0 with @p out filled.
     */
    static inline int calc_get_hist_entry(const crumbs_device_t *dev, uint8_t entry_idx,
                                          calc_hist_entry_t *out)
    {
        crumbs_message_t msg;
        int rc;
        uint8_t opcode = (uint8_t)(CALC_OP_GET_HIST_0 + entry_idx);
        if (!out || !crumbs_ops_can_get(dev) || entry_idx >= CALC_HISTORY_SIZE)
            return -1;
        crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
        crumbs_msg_add_u8(&msg, opcode);
        rc = crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
        if (rc != 0)
            return rc;
        dev->delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US);
        rc = crumbs_controller_read_expect(dev->ctx, dev->addr, CALC_TYPE_ID, opcode, &msg,
                                          dev->read_fn, dev->io);
        if (rc != 0)
            return rc;
        return calc_hist_entry_unpack(msg.data, msg.data_len, out);
    }

#ifdef __cplusplus
}
#endif

#endif /* CALCULATOR_OPS_H */
