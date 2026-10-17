/*
 * Writes tests/golden_vectors/vectors.json: what the C core does with a fixed
 * set of inputs, for the Python binding's tests to match byte for byte.
 *
 *   gen_vectors <output.json>
 *
 * CI rebuilds this and fails if the file it writes differs from the one
 * committed, so the vectors cannot drift from the C code.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crumbs.h"
#include "crumbs_crc.h"
#include "crumbs_message.h"

static FILE *out;
static int first;

static void hex(const uint8_t *data, size_t len)
{
    fputc('"', out);
    for (size_t i = 0; i < len; i++)
    {
        fprintf(out, "%02x", data[i]);
    }
    fputc('"', out);
}

static void item(void)
{
    fputs(first ? "\n    " : ",\n    ", out);
    first = 0;
}

/* A fixed pseudo-random sequence, the same on every platform as long as
   each call to it is its own statement. */
static uint32_t lcg_state = 0x2545F491u;

static uint8_t next_byte(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return (uint8_t)(lcg_state >> 24);
}

static void crc_case(const uint8_t *data, size_t len)
{
    item();
    fputs("{\"input\": ", out);
    hex(data, len);
    fprintf(out, ", \"crc\": %u}", crumbs_crc8(data, len));
}

static void encode_case(uint8_t type_id, uint8_t opcode, const uint8_t *data, size_t len)
{
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = type_id;
    msg.opcode = opcode;
    msg.data_len = (uint8_t)len;
    if (len > 0u && len <= CRUMBS_MAX_PAYLOAD)
    {
        memcpy(msg.data, data, len);
    }
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t n = crumbs_encode_message(&msg, frame, sizeof(frame));
    item();
    fprintf(out, "{\"type_id\": %u, \"opcode\": %u, \"data\": ", type_id, opcode);
    hex(data, len);
    fputs(", \"frame\": ", out);
    if (n == 0u)
    {
        fputs("null}", out);
    }
    else
    {
        hex(frame, n);
        fputc('}', out);
    }
}

static void decode_case(const uint8_t *buf, size_t len)
{
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    int rc = crumbs_decode_message(buf, len, &msg, NULL);
    item();
    fputs("{\"buffer\": ", out);
    hex(buf, len);
    fprintf(out, ", \"result\": %d", rc);
    if (rc == 0)
    {
        fprintf(out, ", \"type_id\": %u, \"opcode\": %u, \"data\": ", msg.type_id, msg.opcode);
        hex(msg.data, msg.data_len);
    }
    fputc('}', out);
}

static void frame_length_case(const uint8_t *buf, size_t len)
{
    size_t frame_len = 0u;
    int rc = crumbs_frame_length(buf, len, &frame_len);
    item();
    fputs("{\"buffer\": ", out);
    hex(buf, len);
    if (rc == 0)
    {
        fprintf(out, ", \"frame_length\": %u}", (unsigned)frame_len);
    }
    else
    {
        fputs(", \"frame_length\": null}", out);
    }
}

/* The acknowledgement cases: a peripheral with one handler that applies,
   one that refuses, and none for any other opcode. */
#define ACK_TYPE 0x07
#define ACK_OP_APPLY 0x01
#define ACK_OP_REFUSE 0x02
#define ACK_REASON 0x2A

static void ack_apply(crumbs_context_t *ctx, uint8_t opcode, const uint8_t *data,
                      uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;
}

static void ack_refuse(crumbs_context_t *ctx, uint8_t opcode, const uint8_t *data,
                       uint8_t data_len, void *user_data)
{
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;
    (void)crumbs_refuse(ctx, ACK_REASON);
}

/* Feed buf to the peripheral, then stage GET_ACK and record the reply the
   library builds, beside the state it was built from. */
static void ack_case(crumbs_context_t *ctx, const uint8_t *buf, size_t len)
{
    int rc = len > 0u ? crumbs_peripheral_handle_receive(ctx, buf, len) : 0;
    uint8_t stage[CRUMBS_MESSAGE_MAX_SIZE];
    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.opcode = CRUMBS_CMD_SET_REPLY;
    m.data_len = 1u;
    m.data[0] = CRUMBS_CMD_GET_ACK;
    size_t n = crumbs_encode_message(&m, stage, sizeof(stage));
    (void)crumbs_peripheral_handle_receive(ctx, stage, n);
    uint8_t reply[CRUMBS_MESSAGE_MAX_SIZE];
    size_t reply_len = 0u;
    (void)crumbs_peripheral_build_reply(ctx, reply, sizeof(reply), &reply_len);
    item();
    fputs("{\"received\": ", out);
    hex(buf, len);
    fprintf(out, ", \"result\": %d, \"reply\": ", rc);
    hex(reply, reply_len);
    fprintf(out, ", \"counter\": %u, \"opcode\": %u, \"status\": %u, \"reason\": %u}",
            ctx->ack_counter, ctx->ack_opcode, ctx->ack_status, ctx->ack_reason);
}

static crumbs_ack_t ack_of(uint8_t counter, uint8_t opcode, uint8_t status, uint8_t reason)
{
    crumbs_ack_t a;
    a.counter = counter;
    a.opcode = opcode;
    a.status = status;
    a.reason = reason;
    return a;
}

static void classify_case(crumbs_ack_t before, crumbs_ack_t after, uint8_t opcode)
{
    item();
    fprintf(out, "{\"before\": [%u, %u, %u, %u], \"after\": [%u, %u, %u, %u], "
                 "\"opcode\": %u, \"result\": %d}",
            before.counter, before.opcode, before.status, before.reason,
            after.counter, after.opcode, after.status, after.reason, opcode,
            crumbs_ack_classify(&before, &after, opcode));
}

/* crumbs_ack_decode() over a GET_ACK reply carrying these payload bytes. */
static void ack_decode_case(const uint8_t *data, size_t len)
{
    crumbs_message_t m;
    crumbs_ack_t a;
    memset(&m, 0, sizeof(m));
    m.type_id = ACK_TYPE;
    m.opcode = CRUMBS_CMD_GET_ACK;
    m.data_len = (uint8_t)len;
    memcpy(m.data, data, len);
    item();
    fputs("{\"data\": ", out);
    hex(data, len);
    fprintf(out, ", \"result\": %d}", crumbs_ack_decode(&m, &a));
}

static size_t encode_into(uint8_t *frame, uint8_t type_id, uint8_t opcode,
                          const uint8_t *data, size_t len)
{
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = type_id;
    msg.opcode = opcode;
    msg.data_len = (uint8_t)len;
    memcpy(msg.data, data, len);
    return crumbs_encode_message(&msg, frame, CRUMBS_MESSAGE_MAX_SIZE);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <output.json>\n", argv[0]);
        return 2;
    }
    out = fopen(argv[1], "w");
    if (!out)
    {
        perror(argv[1]);
        return 1;
    }

    uint8_t buf[64];
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];

    fprintf(out, "{\n  \"crumbs_version\": \"%s\",\n", CRUMBS_VERSION_STRING);

    /* CRC-8 */
    fputs("  \"crc8\": [", out);
    first = 1;
    crc_case((const uint8_t *)"", 0);
    crc_case((const uint8_t *)"123456789", 9);
    memset(buf, 0x00, sizeof(buf));
    crc_case(buf, 1);
    crc_case(buf, 31);
    memset(buf, 0xFF, sizeof(buf));
    crc_case(buf, 1);
    crc_case(buf, 31);
    for (size_t len = 1; len <= 40; len++)
    {
        for (size_t i = 0; i < len; i++)
        {
            buf[i] = next_byte();
        }
        crc_case(buf, len);
    }
    fputs("\n  ],\n", out);

    /* Encoding */
    fputs("  \"encode\": [", out);
    first = 1;
    encode_case(0x00, 0x00, NULL, 0);
    uint8_t one = 0x80;
    encode_case(0x00, CRUMBS_CMD_SET_REPLY, &one, 1);
    for (size_t i = 0; i < CRUMBS_MAX_PAYLOAD; i++)
    {
        buf[i] = (uint8_t)i;
    }
    encode_case(0x01, 0x10, buf, CRUMBS_MAX_PAYLOAD);
    encode_case(0xFF, 0xFF, buf, 5);
    for (int n = 0; n < 60; n++)
    {
        uint8_t type_id = next_byte();
        uint8_t opcode = next_byte();
        size_t len = next_byte() % (CRUMBS_MAX_PAYLOAD + 1u);
        for (size_t i = 0; i < len; i++)
        {
            buf[i] = next_byte();
        }
        encode_case(type_id, opcode, buf, len);
    }
    fputs("\n  ],\n", out);

    /* Decoding, accepted and refused */
    fputs("  \"decode\": [", out);
    first = 1;
    memset(buf, 0x00, sizeof(buf));
    decode_case(buf, 0);
    decode_case(buf, 3);
    decode_case(buf, 4); /* the empty frame 00 00 00 00 */
    memset(buf, 0xFF, sizeof(buf));
    decode_case(buf, CRUMBS_MESSAGE_MAX_SIZE); /* a silent device */
    /* One payload byte over the maximum, with a CRC that matches: refused
       for its length alone. */
    buf[0] = 0x01;
    buf[1] = 0x02;
    buf[2] = (uint8_t)(CRUMBS_MAX_PAYLOAD + 1u);
    for (size_t i = 0; i < CRUMBS_MAX_PAYLOAD + 1u; i++)
    {
        buf[3 + i] = (uint8_t)i;
    }
    buf[3 + CRUMBS_MAX_PAYLOAD + 1u] = crumbs_crc8(buf, 3 + CRUMBS_MAX_PAYLOAD + 1u);
    decode_case(buf, 3 + CRUMBS_MAX_PAYLOAD + 2u);
    for (int n = 0; n < 80; n++)
    {
        uint8_t data[CRUMBS_MAX_PAYLOAD];
        size_t len = next_byte() % (CRUMBS_MAX_PAYLOAD + 1u);
        for (size_t i = 0; i < len; i++)
        {
            data[i] = next_byte();
        }
        /* One draw per statement: C leaves the order of a call's argument
           evaluations unspecified, and compilers differ. */
        uint8_t type_id = next_byte();
        uint8_t opcode = next_byte();
        size_t fl = encode_into(frame, type_id, opcode, data, len);
        memcpy(buf, frame, fl);
        switch (n % 8)
        {
        case 0: /* as sent */
        case 1:
            decode_case(buf, fl);
            break;
        case 2: /* corrupt one byte */
        {
            size_t at = next_byte() % fl;
            unsigned bit = next_byte() % 8u;
            buf[at] ^= (uint8_t)(1u << bit);
        }
            decode_case(buf, fl);
            break;
        case 3: /* one byte short */
            decode_case(buf, fl - 1u);
            break;
        case 4: /* one byte over */
            buf[fl] = 0xFF;
            decode_case(buf, fl + 1u);
            break;
        case 5: /* a payload length beyond the maximum */
            buf[2] = (uint8_t)(CRUMBS_MAX_PAYLOAD + 1u + next_byte() % 200u);
            decode_case(buf, fl);
            break;
        case 6: /* wrong CRC byte only */
            buf[fl - 1u] ^= 0x01;
            decode_case(buf, fl);
            break;
        default: /* bus padding left on */
            memset(buf + fl, 0xFF, CRUMBS_MESSAGE_MAX_SIZE - fl);
            decode_case(buf, CRUMBS_MESSAGE_MAX_SIZE);
            break;
        }
    }
    fputs("\n  ],\n", out);

    /* The length a received header declares */
    fputs("  \"frame_length\": [", out);
    first = 1;
    memset(buf, 0x00, sizeof(buf));
    frame_length_case(buf, 0);
    frame_length_case(buf, 3);
    frame_length_case(buf, 4);
    memset(buf, 0xFF, sizeof(buf));
    frame_length_case(buf, CRUMBS_MESSAGE_MAX_SIZE);
    for (int n = 0; n < 30; n++)
    {
        uint8_t data[CRUMBS_MAX_PAYLOAD];
        size_t len = next_byte() % (CRUMBS_MAX_PAYLOAD + 1u);
        for (size_t i = 0; i < len; i++)
        {
            data[i] = next_byte();
        }
        /* One draw per statement: C leaves the order of a call's argument
           evaluations unspecified, and compilers differ. */
        uint8_t type_id = next_byte();
        uint8_t opcode = next_byte();
        size_t fl = encode_into(frame, type_id, opcode, data, len);
        memset(buf, 0xFF, sizeof(buf));
        memcpy(buf, frame, fl);
        /* a padded fixed-size read, the exact frame, and one byte short */
        frame_length_case(buf, n % 3 == 0 ? CRUMBS_MESSAGE_MAX_SIZE : (n % 3 == 1 ? fl : fl - 1u));
    }
    fputs("\n  ],\n", out);

    /* The acknowledgement a peripheral reports after each received buffer */
    fputs("  \"ack\": [", out);
    first = 1;
    {
        static crumbs_context_t ack_ctx;
        crumbs_init(&ack_ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
        crumbs_set_type_id(&ack_ctx, ACK_TYPE);
        (void)crumbs_register_handler(&ack_ctx, ACK_OP_APPLY, ack_apply, NULL);
        (void)crumbs_register_handler(&ack_ctx, ACK_OP_REFUSE, ack_refuse, NULL);
        uint8_t data[1] = {0x55};

        ack_case(&ack_ctx, NULL, 0); /* nothing received yet */
        size_t fl = encode_into(frame, ACK_TYPE, ACK_OP_APPLY, data, 1);
        ack_case(&ack_ctx, frame, fl);
        fl = encode_into(frame, ACK_TYPE, ACK_OP_REFUSE, data, 1);
        ack_case(&ack_ctx, frame, fl);
        fl = encode_into(frame, ACK_TYPE, 0x03, data, 1); /* no handler */
        ack_case(&ack_ctx, frame, fl);
        fl = encode_into(frame, 0x09, ACK_OP_APPLY, data, 1); /* another type */
        ack_case(&ack_ctx, frame, fl);
        fl = encode_into(frame, 0x09, CRUMBS_CMD_SET_REPLY, data, 1); /* never counted */
        ack_case(&ack_ctx, frame, fl);
        fl = encode_into(frame, ACK_TYPE, ACK_OP_APPLY, data, 1);
        frame[fl - 1u] ^= 0x01; /* corrupt: lost */
        ack_case(&ack_ctx, frame, fl);
        fl = encode_into(frame, ACK_TYPE, ACK_OP_APPLY, data, 1);
        memcpy(buf, frame, fl);
        memcpy(buf + fl, frame, fl); /* a retry merged into the failed write */
        ack_case(&ack_ctx, buf, 2u * fl);
        for (int n = 0; n < 24; n++)
        {
            static const uint8_t types[3] = {CRUMBS_TYPE_ID_ANY, ACK_TYPE, 0x09};
            uint8_t type_id = types[next_byte() % 3u];
            uint8_t opcode = (uint8_t)(1u + next_byte() % 4u);
            fl = encode_into(frame, type_id, opcode, data, 1);
            ack_case(&ack_ctx, frame, fl);
        }
    }
    fputs("\n  ],\n", out);

    /* Which GET_ACK payloads are an acknowledgement */
    fputs("  \"ack_decode\": [", out);
    first = 1;
    {
        static const uint8_t cases[][6] = {
            {9, ACK_OP_REFUSE, CRUMBS_ACK_REFUSED, ACK_REASON, 0, 0},
            {0, 0, CRUMBS_ACK_NONE, 0, 0, 0},
            {1, 0, CRUMBS_ACK_NONE, 0, 0, 0},
            {0, 1, CRUMBS_ACK_NONE, 0, 0, 0},
            {0, 0, CRUMBS_ACK_NONE, 1, 0, 0},
            {0x10, 0x11, 0x12, 0x13, 0x14, 0x15}, /* an echoing old peripheral */
            {3, 3, CRUMBS_ACK_REJECTED, CRUMBS_ACK_REJECT_NO_HANDLER, 0xEE, 0},
            {3, 3, 0x04, 0, 0, 0},
        };
        static const uint8_t lens[] = {4, 4, 4, 4, 4, 6, 5, 4};
        for (size_t i = 0; i < sizeof(lens); i++)
        {
            ack_decode_case(cases[i], lens[i]);
        }
        ack_decode_case(cases[0], 3);
        ack_decode_case(cases[0], 0);
    }
    fputs("\n  ],\n", out);

    /* Classifying a SET by the acknowledgements read before and after it */
    fputs("  \"ack_classify\": [", out);
    first = 1;
    {
        const crumbs_ack_t none = ack_of(0, 0, CRUMBS_ACK_NONE, 0);
        const crumbs_ack_t five = ack_of(5, ACK_OP_REFUSE, CRUMBS_ACK_APPLIED, 0);
        classify_case(five, five, ACK_OP_APPLY);
        classify_case(five, ack_of(6, ACK_OP_APPLY, CRUMBS_ACK_APPLIED, 0), ACK_OP_APPLY);
        classify_case(five, ack_of(6, ACK_OP_APPLY, CRUMBS_ACK_REFUSED, ACK_REASON), ACK_OP_APPLY);
        classify_case(five, ack_of(6, ACK_OP_APPLY, CRUMBS_ACK_REJECTED, 1), ACK_OP_APPLY);
        classify_case(five, ack_of(6, ACK_OP_REFUSE, CRUMBS_ACK_APPLIED, 0), ACK_OP_APPLY);
        classify_case(five, ack_of(5, ACK_OP_APPLY, CRUMBS_ACK_APPLIED, 0), ACK_OP_APPLY);
        classify_case(five, none, ACK_OP_APPLY);                                  /* reset */
        classify_case(none, none, ACK_OP_APPLY);                                  /* fresh */
        classify_case(ack_of(0, ACK_OP_REFUSE, CRUMBS_ACK_APPLIED, 0), none, ACK_OP_APPLY); /* wrapped */
        classify_case(five, ack_of(6, ACK_OP_APPLY, CRUMBS_ACK_NONE, 0), ACK_OP_APPLY);
        classify_case(five, ack_of(6, ACK_OP_APPLY, 0x04, 0), ACK_OP_APPLY);
        classify_case(ack_of(255, ACK_OP_REFUSE, CRUMBS_ACK_APPLIED, 0),
                      ack_of(0, ACK_OP_APPLY, CRUMBS_ACK_APPLIED, 0), ACK_OP_APPLY);
        for (int n = 0; n < 60; n++)
        {
            /* One draw per statement, as above. */
            uint8_t counter = next_byte();
            uint8_t before_op = (uint8_t)(1u + next_byte() % 3u);
            uint8_t before_status = (uint8_t)(1u + next_byte() % 3u);
            uint8_t step = (uint8_t)(next_byte() % 3u);
            uint8_t after_op = (uint8_t)(1u + next_byte() % 3u);
            uint8_t after_status = (uint8_t)(next_byte() % 5u);
            uint8_t opcode = (uint8_t)(1u + next_byte() % 3u);
            crumbs_ack_t before = ack_of(counter, before_op, before_status, 0);
            crumbs_ack_t after = after_status == CRUMBS_ACK_NONE && n % 2
                                     ? none
                                     : ack_of((uint8_t)(counter + step), after_op, after_status, 0);
            classify_case(before, after, opcode);
        }
    }
    fputs("\n  ]\n}\n", out);

    if (fclose(out) != 0)
    {
        perror(argv[1]);
        return 1;
    }
    return 0;
}
