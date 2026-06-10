/*
 * Unit tests for crumbs_message_helpers.h message building and payload reading helpers.
 *
 * These test the inline helper functions for type-safe payload construction
 * and bounds-checked reading.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "crumbs.h"
#include "crumbs_message_helpers.h"

/* ---- Message Building Tests ------------------------------------------- */

static int test_msg_init(void)
{
    crumbs_message_t msg;
    memset(&msg, 0xFF, sizeof(msg)); /* Fill with garbage first */

    crumbs_msg_init(&msg, 0x42, 0x55);

    if (msg.type_id != 0x42)
    {
        fprintf(stderr, "msg_init: type_id mismatch\n");
        return 1;
    }
    if (msg.opcode != 0x55)
    {
        fprintf(stderr, "msg_init: opcode mismatch\n");
        return 1;
    }
    if (msg.data_len != 0)
    {
        fprintf(stderr, "msg_init: data_len should be 0\n");
        return 1;
    }

    printf("  msg_init: PASS\n");
    return 0;
}

static int test_add_u8(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    int rc = crumbs_msg_add_u8(&msg, 0xAB);
    if (rc != 0)
    {
        fprintf(stderr, "add_u8: returned %d\n", rc);
        return 1;
    }
    if (msg.data_len != 1 || msg.data[0] != 0xAB)
    {
        fprintf(stderr, "add_u8: data mismatch\n");
        return 1;
    }

    printf("  add_u8: PASS\n");
    return 0;
}

static int test_add_u16(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    int rc = crumbs_msg_add_u16(&msg, 0x1234);
    if (rc != 0)
    {
        fprintf(stderr, "add_u16: returned %d\n", rc);
        return 1;
    }
    if (msg.data_len != 2)
    {
        fprintf(stderr, "add_u16: data_len should be 2\n");
        return 1;
    }
    /* Little-endian: LSB first */
    if (msg.data[0] != 0x34 || msg.data[1] != 0x12)
    {
        fprintf(stderr, "add_u16: data mismatch (got 0x%02X 0x%02X)\n",
                msg.data[0], msg.data[1]);
        return 1;
    }

    printf("  add_u16: PASS\n");
    return 0;
}

static int test_add_u32(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    int rc = crumbs_msg_add_u32(&msg, 0xDEADBEEF);
    if (rc != 0)
    {
        fprintf(stderr, "add_u32: returned %d\n", rc);
        return 1;
    }
    if (msg.data_len != 4)
    {
        fprintf(stderr, "add_u32: data_len should be 4\n");
        return 1;
    }
    /* Little-endian */
    if (msg.data[0] != 0xEF || msg.data[1] != 0xBE ||
        msg.data[2] != 0xAD || msg.data[3] != 0xDE)
    {
        fprintf(stderr, "add_u32: data mismatch\n");
        return 1;
    }

    printf("  add_u32: PASS\n");
    return 0;
}

static int test_add_signed(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    /* Add negative values */
    crumbs_msg_add_i8(&msg, -1);       /* 0xFF */
    crumbs_msg_add_i16(&msg, -256);    /* 0xFF00 -> LE: 0x00, 0xFF */
    crumbs_msg_add_i32(&msg, -1);      /* 0xFFFFFFFF */

    if (msg.data_len != 7)
    {
        fprintf(stderr, "add_signed: data_len should be 7, got %u\n", msg.data_len);
        return 1;
    }

    /* Verify i8 */
    if (msg.data[0] != 0xFF)
    {
        fprintf(stderr, "add_signed: i8 mismatch\n");
        return 1;
    }

    /* Verify i16 (-256 = 0xFF00 in little-endian: 0x00, 0xFF) */
    if (msg.data[1] != 0x00 || msg.data[2] != 0xFF)
    {
        fprintf(stderr, "add_signed: i16 mismatch (got 0x%02X 0x%02X)\n",
                msg.data[1], msg.data[2]);
        return 1;
    }

    /* Verify i32 (-1 = 0xFFFFFFFF) */
    if (msg.data[3] != 0xFF || msg.data[4] != 0xFF ||
        msg.data[5] != 0xFF || msg.data[6] != 0xFF)
    {
        fprintf(stderr, "add_signed: i32 mismatch\n");
        return 1;
    }

    printf("  add_signed: PASS\n");
    return 0;
}

static int test_add_float(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    float val = 3.14159f;
    int rc = crumbs_msg_add_float(&msg, val);
    if (rc != 0)
    {
        fprintf(stderr, "add_float: returned %d\n", rc);
        return 1;
    }
    if (msg.data_len != sizeof(float))
    {
        fprintf(stderr, "add_float: data_len mismatch\n");
        return 1;
    }

    /* Verify by reading back */
    float readback;
    memcpy(&readback, msg.data, sizeof(float));
    if (fabsf(readback - val) > 0.0001f)
    {
        fprintf(stderr, "add_float: value mismatch\n");
        return 1;
    }

    printf("  add_float: PASS\n");
    return 0;
}

static int test_add_bytes(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    int rc = crumbs_msg_add_bytes(&msg, data, sizeof(data));
    if (rc != 0)
    {
        fprintf(stderr, "add_bytes: returned %d\n", rc);
        return 1;
    }
    if (msg.data_len != sizeof(data))
    {
        fprintf(stderr, "add_bytes: data_len mismatch\n");
        return 1;
    }
    if (memcmp(msg.data, data, sizeof(data)) != 0)
    {
        fprintf(stderr, "add_bytes: data mismatch\n");
        return 1;
    }

    printf("  add_bytes: PASS\n");
    return 0;
}

static int test_add_invalid_args(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);
    uint8_t data[] = {0xAA};

    /* Should be a safe no-op. */
    crumbs_msg_init(NULL, 0x01, 0x02);

    if (crumbs_msg_add_u8(NULL, 0x11) != -1)
    {
        fprintf(stderr, "add_invalid: add_u8 NULL msg should fail\n");
        return 1;
    }
    if (crumbs_msg_add_u16(NULL, 0x1122) != -1)
    {
        fprintf(stderr, "add_invalid: add_u16 NULL msg should fail\n");
        return 1;
    }
    if (crumbs_msg_add_u32(NULL, 0x11223344) != -1)
    {
        fprintf(stderr, "add_invalid: add_u32 NULL msg should fail\n");
        return 1;
    }
    if (crumbs_msg_add_i8(NULL, -1) != -1 ||
        crumbs_msg_add_i16(NULL, -1) != -1 ||
        crumbs_msg_add_i32(NULL, -1) != -1)
    {
        fprintf(stderr, "add_invalid: signed add NULL msg should fail\n");
        return 1;
    }
    if (crumbs_msg_add_float(NULL, 1.0f) != -1)
    {
        fprintf(stderr, "add_invalid: add_float NULL msg should fail\n");
        return 1;
    }
    if (crumbs_msg_add_bytes(NULL, data, sizeof(data)) != -1)
    {
        fprintf(stderr, "add_invalid: add_bytes NULL msg should fail\n");
        return 1;
    }
    if (crumbs_msg_add_bytes(&msg, NULL, 1) != -1)
    {
        fprintf(stderr, "add_invalid: add_bytes NULL data with len should fail\n");
        return 1;
    }
    if (crumbs_msg_add_bytes(&msg, NULL, 0) != 0 || msg.data_len != 0)
    {
        fprintf(stderr, "add_invalid: add_bytes NULL data len 0 should be no-op success\n");
        return 1;
    }

    printf("  add invalid args: PASS\n");
    return 0;
}

static int test_add_overflow(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    /* Fill to capacity */
    for (int i = 0; i < CRUMBS_MAX_PAYLOAD; ++i)
    {
        int rc = crumbs_msg_add_u8(&msg, (uint8_t)i);
        if (rc != 0)
        {
            fprintf(stderr, "add_overflow: early failure at %d\n", i);
            return 1;
        }
    }

    /* Next add should fail */
    int rc = crumbs_msg_add_u8(&msg, 0xFF);
    if (rc != -1)
    {
        fprintf(stderr, "add_overflow: should return -1 on overflow\n");
        return 1;
    }

    /* data_len should still be at max */
    if (msg.data_len != CRUMBS_MAX_PAYLOAD)
    {
        fprintf(stderr, "add_overflow: data_len corrupted\n");
        return 1;
    }

    printf("  add_overflow u8: PASS\n");
    return 0;
}

static int test_add_u16_overflow(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    /* Fill to capacity - 1 (26 bytes) */
    for (int i = 0; i < CRUMBS_MAX_PAYLOAD - 1; ++i)
    {
        crumbs_msg_add_u8(&msg, (uint8_t)i);
    }

    /* Adding u16 (2 bytes) should fail - only 1 byte left */
    int rc = crumbs_msg_add_u16(&msg, 0x1234);
    if (rc != -1)
    {
        fprintf(stderr, "add_u16_overflow: should fail when 1 byte left\n");
        return 1;
    }

    printf("  add_u16 overflow: PASS\n");
    return 0;
}

static int test_add_u32_overflow(void)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x02);

    /* Fill to capacity - 3 (24 bytes) */
    for (int i = 0; i < CRUMBS_MAX_PAYLOAD - 3; ++i)
    {
        crumbs_msg_add_u8(&msg, (uint8_t)i);
    }

    /* Adding u32 (4 bytes) should fail - only 3 bytes left */
    int rc = crumbs_msg_add_u32(&msg, 0x12345678);
    if (rc != -1)
    {
        fprintf(stderr, "add_u32_overflow: should fail when 3 bytes left\n");
        return 1;
    }

    printf("  add_u32 overflow: PASS\n");
    return 0;
}

/* ---- Payload Reading Tests -------------------------------------------- */

static int test_read_u8(void)
{
    uint8_t payload[] = {0xAB, 0xCD, 0xEF};
    uint8_t val;

    int rc = crumbs_msg_read_u8(payload, sizeof(payload), 0, &val);
    if (rc != 0 || val != 0xAB)
    {
        fprintf(stderr, "read_u8: offset 0 failed\n");
        return 1;
    }

    rc = crumbs_msg_read_u8(payload, sizeof(payload), 2, &val);
    if (rc != 0 || val != 0xEF)
    {
        fprintf(stderr, "read_u8: offset 2 failed\n");
        return 1;
    }

    /* Out of bounds */
    rc = crumbs_msg_read_u8(payload, sizeof(payload), 3, &val);
    if (rc != -1)
    {
        fprintf(stderr, "read_u8: should fail out of bounds\n");
        return 1;
    }

    printf("  read_u8: PASS\n");
    return 0;
}

static int test_read_u16(void)
{
    /* Little-endian: 0x34, 0x12 = 0x1234 */
    uint8_t payload[] = {0x34, 0x12, 0x78, 0x56};
    uint16_t val;

    int rc = crumbs_msg_read_u16(payload, sizeof(payload), 0, &val);
    if (rc != 0 || val != 0x1234)
    {
        fprintf(stderr, "read_u16: offset 0 failed (got 0x%04X)\n", val);
        return 1;
    }

    rc = crumbs_msg_read_u16(payload, sizeof(payload), 2, &val);
    if (rc != 0 || val != 0x5678)
    {
        fprintf(stderr, "read_u16: offset 2 failed\n");
        return 1;
    }

    /* Out of bounds: offset 3 + 2 bytes = 5 > 4 */
    rc = crumbs_msg_read_u16(payload, sizeof(payload), 3, &val);
    if (rc != -1)
    {
        fprintf(stderr, "read_u16: should fail out of bounds\n");
        return 1;
    }

    printf("  read_u16: PASS\n");
    return 0;
}

static int test_read_u32(void)
{
    /* Little-endian: 0xEF, 0xBE, 0xAD, 0xDE = 0xDEADBEEF */
    uint8_t payload[] = {0xEF, 0xBE, 0xAD, 0xDE, 0x00};
    uint32_t val;

    int rc = crumbs_msg_read_u32(payload, sizeof(payload), 0, &val);
    if (rc != 0 || val != 0xDEADBEEF)
    {
        fprintf(stderr, "read_u32: offset 0 failed (got 0x%08X)\n", val);
        return 1;
    }

    /* Out of bounds */
    rc = crumbs_msg_read_u32(payload, sizeof(payload), 2, &val);
    if (rc != -1)
    {
        fprintf(stderr, "read_u32: should fail out of bounds\n");
        return 1;
    }

    printf("  read_u32: PASS\n");
    return 0;
}

static int test_read_signed(void)
{
    /* i8: -1 = 0xFF, i16: -256 = 0xFF00 (LE: 0x00, 0xFF), i32: -1 */
    uint8_t payload[] = {0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    int8_t i8;
    int16_t i16;
    int32_t i32;

    int rc = crumbs_msg_read_i8(payload, sizeof(payload), 0, &i8);
    if (rc != 0 || i8 != -1)
    {
        fprintf(stderr, "read_i8: got %d\n", i8);
        return 1;
    }

    rc = crumbs_msg_read_i16(payload, sizeof(payload), 1, &i16);
    if (rc != 0 || i16 != -256)
    {
        fprintf(stderr, "read_i16: got %d\n", i16);
        return 1;
    }

    rc = crumbs_msg_read_i32(payload, sizeof(payload), 3, &i32);
    if (rc != 0 || i32 != -1)
    {
        fprintf(stderr, "read_i32: got %d\n", i32);
        return 1;
    }

    printf("  read_signed: PASS\n");
    return 0;
}

static int test_read_float(void)
{
    float original = 3.14159f;
    uint8_t payload[sizeof(float)];
    memcpy(payload, &original, sizeof(float));

    float val;
    int rc = crumbs_msg_read_float(payload, sizeof(payload), 0, &val);
    if (rc != 0 || fabsf(val - original) > 0.0001f)
    {
        fprintf(stderr, "read_float: mismatch\n");
        return 1;
    }

    /* Out of bounds */
    rc = crumbs_msg_read_float(payload, sizeof(payload), 1, &val);
    if (rc != -1)
    {
        fprintf(stderr, "read_float: should fail out of bounds\n");
        return 1;
    }

    printf("  read_float: PASS\n");
    return 0;
}

static int test_read_bytes(void)
{
    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t out[3];

    int rc = crumbs_msg_read_bytes(payload, sizeof(payload), 1, out, 3);
    if (rc != 0)
    {
        fprintf(stderr, "read_bytes: failed\n");
        return 1;
    }
    if (out[0] != 0x22 || out[1] != 0x33 || out[2] != 0x44)
    {
        fprintf(stderr, "read_bytes: data mismatch\n");
        return 1;
    }

    /* Out of bounds */
    rc = crumbs_msg_read_bytes(payload, sizeof(payload), 3, out, 3);
    if (rc != -1)
    {
        fprintf(stderr, "read_bytes: should fail out of bounds\n");
        return 1;
    }

    printf("  read_bytes: PASS\n");
    return 0;
}

static int test_read_invalid_args(void)
{
    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    float f;
    uint8_t out[2];

    if (crumbs_msg_read_u8(NULL, sizeof(payload), 0, &u8) != -1 ||
        crumbs_msg_read_u8(payload, sizeof(payload), 0, NULL) != -1)
    {
        fprintf(stderr, "read_invalid: read_u8 invalid args should fail\n");
        return 1;
    }
    if (crumbs_msg_read_u16(NULL, sizeof(payload), 0, &u16) != -1 ||
        crumbs_msg_read_u16(payload, sizeof(payload), 0, NULL) != -1)
    {
        fprintf(stderr, "read_invalid: read_u16 invalid args should fail\n");
        return 1;
    }
    if (crumbs_msg_read_u32(NULL, sizeof(payload), 0, &u32) != -1 ||
        crumbs_msg_read_u32(payload, sizeof(payload), 0, NULL) != -1)
    {
        fprintf(stderr, "read_invalid: read_u32 invalid args should fail\n");
        return 1;
    }
    if (crumbs_msg_read_i8(NULL, sizeof(payload), 0, &i8) != -1 ||
        crumbs_msg_read_i8(payload, sizeof(payload), 0, NULL) != -1 ||
        crumbs_msg_read_i16(NULL, sizeof(payload), 0, &i16) != -1 ||
        crumbs_msg_read_i16(payload, sizeof(payload), 0, NULL) != -1 ||
        crumbs_msg_read_i32(NULL, sizeof(payload), 0, &i32) != -1 ||
        crumbs_msg_read_i32(payload, sizeof(payload), 0, NULL) != -1)
    {
        fprintf(stderr, "read_invalid: signed read invalid args should fail\n");
        return 1;
    }
    if (crumbs_msg_read_float(NULL, sizeof(payload), 0, &f) != -1 ||
        crumbs_msg_read_float(payload, sizeof(payload), 0, NULL) != -1)
    {
        fprintf(stderr, "read_invalid: read_float invalid args should fail\n");
        return 1;
    }
    if (crumbs_msg_read_bytes(NULL, sizeof(payload), 0, out, sizeof(out)) != -1 ||
        crumbs_msg_read_bytes(payload, sizeof(payload), 0, NULL, sizeof(out)) != -1)
    {
        fprintf(stderr, "read_invalid: read_bytes invalid args should fail\n");
        return 1;
    }
    if (crumbs_msg_read_bytes(NULL, 0, 0, NULL, 0) != 0)
    {
        fprintf(stderr, "read_invalid: read_bytes NULL len/count 0 should be no-op success\n");
        return 1;
    }

    printf("  read invalid args: PASS\n");
    return 0;
}

static int test_roundtrip(void)
{
    /* Build a message, encode it, decode it, read it back */
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x10, 0x20);
    crumbs_msg_add_u8(&msg, 0x42);
    crumbs_msg_add_u16(&msg, 0x1234);
    crumbs_msg_add_u32(&msg, 0xDEADBEEF);

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, frame, sizeof(frame));
    if (len == 0)
    {
        fprintf(stderr, "roundtrip: encode failed\n");
        return 1;
    }

    crumbs_message_t decoded;
    int rc = crumbs_decode_message(frame, len, &decoded, NULL);
    if (rc != 0)
    {
        fprintf(stderr, "roundtrip: decode failed\n");
        return 1;
    }

    /* Read values back */
    uint8_t v8;
    uint16_t v16;
    uint32_t v32;

    rc = crumbs_msg_read_u8(decoded.data, decoded.data_len, 0, &v8);
    if (rc != 0 || v8 != 0x42)
    {
        fprintf(stderr, "roundtrip: u8 mismatch\n");
        return 1;
    }

    rc = crumbs_msg_read_u16(decoded.data, decoded.data_len, 1, &v16);
    if (rc != 0 || v16 != 0x1234)
    {
        fprintf(stderr, "roundtrip: u16 mismatch\n");
        return 1;
    }

    rc = crumbs_msg_read_u32(decoded.data, decoded.data_len, 3, &v32);
    if (rc != 0 || v32 != 0xDEADBEEF)
    {
        fprintf(stderr, "roundtrip: u32 mismatch\n");
        return 1;
    }

    printf("  roundtrip: PASS\n");
    return 0;
}

/* ---- Main ------------------------------------------------------------- */

int main(void)
{
    int failures = 0;

    printf("Running message helper tests:\n");
    printf("  Building:\n");

    failures += test_msg_init();
    failures += test_add_u8();
    failures += test_add_u16();
    failures += test_add_u32();
    failures += test_add_signed();
    failures += test_add_float();
    failures += test_add_bytes();
    failures += test_add_invalid_args();
    failures += test_add_overflow();
    failures += test_add_u16_overflow();
    failures += test_add_u32_overflow();

    printf("  Reading:\n");

    failures += test_read_u8();
    failures += test_read_u16();
    failures += test_read_u32();
    failures += test_read_signed();
    failures += test_read_float();
    failures += test_read_bytes();
    failures += test_read_invalid_args();

    printf("  Integration:\n");

    failures += test_roundtrip();

    if (failures == 0)
    {
        printf("OK all message helper tests passed\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
}
