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
    fputs("\n  ]\n}\n", out);

    if (fclose(out) != 0)
    {
        perror(argv[1]);
        return 1;
    }
    return 0;
}
