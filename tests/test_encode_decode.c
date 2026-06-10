/*
 * Lightweight unit test for encode/decode and CRC correctness.
 * Tests variable-length payload encoding with various data sizes.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"

static int test_basic_encode_decode(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0xAA;
    m.opcode = 0x55;
    m.data_len = 5;
    for (size_t i = 0; i < m.data_len; ++i)
        m.data[i] = (uint8_t)(i + 1);

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));

    /* Expected: 4 + data_len = 9 bytes */
    size_t expected_len = 4 + m.data_len;
    if (w != expected_len)
    {
        fprintf(stderr, "encode length mismatch: got %zu expected %zu\n", w, expected_len);
        return 1;
    }

    crumbs_message_t out;
    memset(&out, 0, sizeof(out));

    int rc = crumbs_decode_message(buf, w, &out, &ctx);
    if (rc != 0)
    {
        fprintf(stderr, "decode failed (rc=%d)\n", rc);
        return 1;
    }

    if (out.type_id != m.type_id || out.opcode != m.opcode)
    {
        fprintf(stderr, "decoded header mismatch\n");
        return 1;
    }

    if (out.data_len != m.data_len)
    {
        fprintf(stderr, "decoded data_len mismatch: got %u expected %u\n", out.data_len, m.data_len);
        return 1;
    }

    for (size_t i = 0; i < m.data_len; ++i)
    {
        if (out.data[i] != m.data[i])
        {
            fprintf(stderr, "data mismatch at %zu\n", i);
            return 1;
        }
    }

    if (!crumbs_last_crc_ok(&ctx))
    {
        fprintf(stderr, "CRC flagged bad unexpectedly\n");
        return 1;
    }

    printf("  basic encode/decode: PASS\n");
    return 0;
}

static int test_zero_length_payload(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0x01;
    m.opcode = 0x02;
    m.data_len = 0; /* zero-length payload */

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));

    /* Expected: 4 bytes (header + crc) */
    if (w != 4)
    {
        fprintf(stderr, "zero-length encode: got %zu expected 4\n", w);
        return 1;
    }

    crumbs_message_t out;
    memset(&out, 0, sizeof(out));

    int rc = crumbs_decode_message(buf, w, &out, &ctx);
    if (rc != 0)
    {
        fprintf(stderr, "zero-length decode failed (rc=%d)\n", rc);
        return 1;
    }

    if (out.data_len != 0)
    {
        fprintf(stderr, "zero-length: data_len mismatch\n");
        return 1;
    }

    printf("  zero-length payload: PASS\n");
    return 0;
}

static int test_max_length_payload(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0xFF;
    m.opcode = 0xFE;
    m.data_len = CRUMBS_MAX_PAYLOAD; /* 27 bytes */
    for (size_t i = 0; i < CRUMBS_MAX_PAYLOAD; ++i)
        m.data[i] = (uint8_t)(i ^ 0xAA);

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));

    /* Expected: 4 + 27 = 31 bytes (max frame) */
    if (w != CRUMBS_MESSAGE_MAX_SIZE)
    {
        fprintf(stderr, "max-length encode: got %zu expected %u\n", w, (unsigned)CRUMBS_MESSAGE_MAX_SIZE);
        return 1;
    }

    crumbs_message_t out;
    memset(&out, 0, sizeof(out));

    int rc = crumbs_decode_message(buf, w, &out, &ctx);
    if (rc != 0)
    {
        fprintf(stderr, "max-length decode failed (rc=%d)\n", rc);
        return 1;
    }

    if (out.data_len != CRUMBS_MAX_PAYLOAD)
    {
        fprintf(stderr, "max-length: data_len mismatch\n");
        return 1;
    }

    for (size_t i = 0; i < CRUMBS_MAX_PAYLOAD; ++i)
    {
        if (out.data[i] != m.data[i])
        {
            fprintf(stderr, "max-length: data mismatch at %zu\n", i);
            return 1;
        }
    }

    printf("  max-length payload: PASS\n");
    return 0;
}

static int test_oversized_data_len(void)
{
    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0x01;
    m.opcode = 0x02;
    m.data_len = CRUMBS_MAX_PAYLOAD + 1; /* invalid */

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE + 10];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));

    if (w != 0)
    {
        fprintf(stderr, "oversized data_len should fail encode, got %zu\n", w);
        return 1;
    }

    printf("  oversized data_len rejection: PASS\n");
    return 0;
}

static int test_truncated_frame(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    /* Build a valid frame first */
    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0x10;
    m.opcode = 0x20;
    m.data_len = 5;
    for (size_t i = 0; i < 5; ++i)
        m.data[i] = (uint8_t)i;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));

    /* Try to decode with less bytes than needed (truncated) */
    crumbs_message_t out;
    memset(&out, 0, sizeof(out));

    int rc = crumbs_decode_message(buf, w - 2, &out, &ctx);
    if (rc == 0)
    {
        fprintf(stderr, "truncated frame should fail decode\n");
        return 1;
    }

    printf("  truncated frame rejection: PASS\n");
    return 0;
}

static int test_trailing_bytes_rejected(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0x33;
    m.opcode = 0x44;
    m.data_len = 2;
    m.data[0] = 0x55;
    m.data[1] = 0x66;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE + 1u];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));
    if (w == 0u)
    {
        fprintf(stderr, "trailing bytes setup encode failed\n");
        return 1;
    }

    crumbs_message_t out;
    memset(&out, 0, sizeof(out));

    int rc = crumbs_decode_message(buf, w, &out, &ctx);
    if (rc != 0 || !crumbs_last_crc_ok(&ctx))
    {
        fprintf(stderr, "trailing bytes setup decode failed (rc=%d)\n", rc);
        return 1;
    }

    buf[w] = 0x99;
    rc = crumbs_decode_message(buf, w + 1u, &out, &ctx);
    if (rc != -1)
    {
        fprintf(stderr, "decode with trailing bytes should fail with -1, got %d\n", rc);
        return 1;
    }

    if (crumbs_last_crc_ok(&ctx))
    {
        fprintf(stderr, "trailing bytes should clear last_crc_ok\n");
        return 1;
    }

    printf("  trailing bytes rejection: PASS\n");
    return 0;
}

static int test_malformed_data_len_in_frame(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    /*
     * Construct a wire frame manually with a data_len byte that claims
     * more payload bytes than actually present. This tests the decoder's
     * bounds checking against the provided buffer_len.
     *
     * Frame format: [type_id, opcode, data_len, ...payload..., CRC]
     */
    uint8_t malformed[8];
    malformed[0] = 0x01; /* type_id */
    malformed[1] = 0x02; /* opcode */
    malformed[2] = 20;   /* data_len claims 20 bytes of payload */
    malformed[3] = 0xAA; /* Only 1 byte of actual payload */
    malformed[4] = 0x00; /* Fake CRC (doesn't matter, should fail length check first) */

    crumbs_message_t out;
    memset(&out, 0, sizeof(out));

    /* Buffer is only 5 bytes, but data_len claims 20 - should fail */
    int rc = crumbs_decode_message(malformed, 5, &out, &ctx);
    if (rc == 0)
    {
        fprintf(stderr, "malformed data_len should fail decode\n");
        return 1;
    }

    printf("  malformed data_len rejection: PASS\n");
    return 0;
}

static int test_decode_minimum_valid_frame(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    /* Minimum valid frame: 4 bytes (type_id, opcode, data_len=0, CRC) */
    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = 0xAA;
    m.opcode = 0xBB;
    m.data_len = 0;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t w = crumbs_encode_message(&m, buf, sizeof(buf));

    if (w != 4)
    {
        fprintf(stderr, "minimum frame should be 4 bytes, got %zu\n", w);
        return 1;
    }

    /* Now try decoding with exactly 4 bytes */
    crumbs_message_t out;
    int rc = crumbs_decode_message(buf, 4, &out, &ctx);
    if (rc != 0)
    {
        fprintf(stderr, "minimum valid frame decode failed\n");
        return 1;
    }

    if (out.type_id != 0xAA || out.opcode != 0xBB || out.data_len != 0)
    {
        fprintf(stderr, "minimum frame decode mismatch\n");
        return 1;
    }

    printf("  minimum valid frame: PASS\n");
    return 0;
}

static int test_decode_buffer_len_too_short(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t buf[3] = {0x01, 0x02, 0x00}; /* Only 3 bytes, min is 4 */

    crumbs_message_t out;
    int rc = crumbs_decode_message(buf, 3, &out, &ctx);
    if (rc == 0)
    {
        fprintf(stderr, "buffer_len < 4 should fail\n");
        return 1;
    }

    printf("  buffer too short rejection: PASS\n");
    return 0;
}

int main(void)
{
    int failures = 0;

    printf("Running encode/decode tests:\n");

    failures += test_basic_encode_decode();
    failures += test_zero_length_payload();
    failures += test_max_length_payload();
    failures += test_oversized_data_len();
    failures += test_truncated_frame();
    failures += test_trailing_bytes_rejected();
    failures += test_malformed_data_len_in_frame();
    failures += test_decode_minimum_valid_frame();
    failures += test_decode_buffer_len_too_short();

    if (failures == 0)
    {
        printf("OK all encode/decode tests passed\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
}
