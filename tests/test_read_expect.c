/*
 * Unit tests for crumbs_controller_read_expect() (#35): the controller-side
 * reply identity check, and CRUMBS_DEFINE_GET_OP deferring to it.
 *
 * A reply with a valid CRC and the wrong opcode passed crumbs_controller_read
 * untouched; every shipping getter re-implemented the comparison by hand.
 * These tests pin the check in the core and pin that the macro uses it.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"
#include "crumbs_message_helpers.h"
#include "crumbs_ops.h"
#include "test_common.h"

#define DEV_ADDR 0x10
#define DEV_TYPE 0x21
#define OP_GET_VALUE 0x80

/* What the fake peripheral will answer with on the next read. */
typedef struct
{
    uint8_t reply_type;
    uint8_t reply_opcode;
    uint8_t reply_value;
    int corrupt_crc;
    int write_calls;
    int read_calls;
} fake_dev_t;

static int fake_write(void *user_ctx, uint8_t addr, const uint8_t *data, size_t len)
{
    (void)addr;
    (void)data;
    (void)len;
    ((fake_dev_t *)user_ctx)->write_calls++;
    return 0;
}

static int fake_read(void *user_ctx, uint8_t addr, uint8_t *buf, size_t len, uint32_t timeout_us)
{
    (void)addr;
    (void)timeout_us;
    fake_dev_t *d = (fake_dev_t *)user_ctx;
    d->read_calls++;

    crumbs_message_t m;
    crumbs_msg_init(&m, d->reply_type, d->reply_opcode);
    crumbs_msg_add_u8(&m, d->reply_value);
    size_t n = crumbs_encode_message(&m, buf, len);
    if (n == 0)
        return -1;
    if (d->corrupt_crc)
        buf[n - 1] ^= 0xFF;
    return (int)n;
}

static void fake_delay(uint32_t us)
{
    (void)us;
}

/* A macro-generated getter, as a family header would write it. */
typedef struct
{
    uint8_t value;
} rx_value_t;

static int parse_value(const uint8_t *data, size_t len, rx_value_t *out)
{
    if (!data || len < 1u || !out)
        return -1;
    out->value = data[0];
    return 0;
}

CRUMBS_DEFINE_GET_OP(rx, value, DEV_TYPE, OP_GET_VALUE, rx_value_t, parse_value)

static void setup(crumbs_context_t *ctx, fake_dev_t *d, uint8_t type, uint8_t opcode)
{
    crumbs_init(ctx, CRUMBS_ROLE_CONTROLLER, 0);
    memset(d, 0, sizeof(*d));
    d->reply_type = type;
    d->reply_opcode = opcode;
    d->reply_value = 0x5A;
}

static int test_matching_reply_accepted(void)
{
    const char *t = "matching reply accepted";
    crumbs_context_t ctx;
    fake_dev_t d;
    crumbs_message_t out;
    setup(&ctx, &d, DEV_TYPE, OP_GET_VALUE);
    int rc = crumbs_controller_read_expect(&ctx, DEV_ADDR, DEV_TYPE, OP_GET_VALUE, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, 0, "expected 0 for a matching reply");
    TEST_ASSERT_EQ(t, out.data[0], 0x5A, "payload must be decoded into out_msg");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_wrong_opcode_rejected(void)
{
    const char *t = "wrong opcode rejected";
    crumbs_context_t ctx;
    fake_dev_t d;
    crumbs_message_t out;
    setup(&ctx, &d, DEV_TYPE, 0x81); /* a different, valid opcode */
    int rc = crumbs_controller_read_expect(&ctx, DEV_ADDR, DEV_TYPE, OP_GET_VALUE, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, CRUMBS_RX_REPLY_MISMATCH, "valid frame, wrong opcode must be CRUMBS_RX_REPLY_MISMATCH");
    TEST_ASSERT_EQ(t, out.opcode, 0x81, "out_msg must still hold what was received");
    TEST_ASSERT_EQ(t, ctx.crc_error_count, 0u, "identity mismatch is not a CRC error");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_wrong_type_rejected(void)
{
    const char *t = "wrong type rejected";
    crumbs_context_t ctx;
    fake_dev_t d;
    crumbs_message_t out;
    setup(&ctx, &d, 0x22, OP_GET_VALUE);
    int rc = crumbs_controller_read_expect(&ctx, DEV_ADDR, DEV_TYPE, OP_GET_VALUE, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, CRUMBS_RX_REPLY_MISMATCH, "valid frame, wrong type must be CRUMBS_RX_REPLY_MISMATCH");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_any_type_skips_type_check(void)
{
    const char *t = "CRUMBS_TYPE_ID_ANY skips the type half";
    crumbs_context_t ctx;
    fake_dev_t d;
    crumbs_message_t out;
    setup(&ctx, &d, 0x22, OP_GET_VALUE);
    int rc = crumbs_controller_read_expect(&ctx, DEV_ADDR, CRUMBS_TYPE_ID_ANY, OP_GET_VALUE, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, 0, "expect_type_id 0 must accept any type");
    rc = crumbs_controller_read_expect(&ctx, DEV_ADDR, CRUMBS_TYPE_ID_ANY, 0x81, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, CRUMBS_RX_REPLY_MISMATCH, "opcode is still checked when type is ANY");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_decode_errors_pass_through(void)
{
    const char *t = "decode errors pass through";
    crumbs_context_t ctx;
    fake_dev_t d;
    crumbs_message_t out;
    setup(&ctx, &d, DEV_TYPE, OP_GET_VALUE);
    d.corrupt_crc = 1;
    int rc = crumbs_controller_read_expect(&ctx, DEV_ADDR, DEV_TYPE, OP_GET_VALUE, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, -2, "CRC failure must keep crumbs_decode_message's -2");
    rc = crumbs_controller_read_expect(NULL, DEV_ADDR, DEV_TYPE, OP_GET_VALUE, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, -1, "NULL ctx must be -1 like crumbs_controller_read");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_plain_read_is_unchanged(void)
{
    const char *t = "crumbs_controller_read still accepts any identity";
    crumbs_context_t ctx;
    fake_dev_t d;
    crumbs_message_t out;
    setup(&ctx, &d, 0x22, 0x81);
    int rc = crumbs_controller_read(&ctx, DEV_ADDR, &out, fake_read, &d);
    TEST_ASSERT_EQ(t, rc, 0, "the unchecked read must not start rejecting");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_get_op_macro_defers_to_core(void)
{
    const char *t = "CRUMBS_DEFINE_GET_OP uses the core check";
    crumbs_context_t ctx;
    fake_dev_t d;
    rx_value_t v;
    setup(&ctx, &d, DEV_TYPE, OP_GET_VALUE);
    crumbs_device_t dev = {&ctx, DEV_ADDR, fake_write, fake_read, fake_delay, &d};

    int rc = rx_get_value(&dev, &v);
    TEST_ASSERT_EQ(t, rc, 0, "matching reply must parse");
    TEST_ASSERT_EQ(t, v.value, 0x5A, "parsed value");
    TEST_ASSERT_EQ(t, d.write_calls, 1, "one SET_REPLY write");

    d.reply_opcode = 0x81;
    rc = rx_get_value(&dev, &v);
    TEST_ASSERT_EQ(t, rc, CRUMBS_RX_REPLY_MISMATCH, "macro must surface the core's mismatch code, not a private -1");

    d.reply_opcode = OP_GET_VALUE;
    d.reply_type = 0x22;
    rc = rx_get_value(&dev, &v);
    TEST_ASSERT_EQ(t, rc, CRUMBS_RX_REPLY_MISMATCH, "wrong type through the macro");
    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("Running read_expect tests:\n");
    failures += test_matching_reply_accepted();
    failures += test_wrong_opcode_rejected();
    failures += test_wrong_type_rejected();
    failures += test_any_type_skips_type_check();
    failures += test_decode_errors_pass_through();
    failures += test_plain_read_is_unchanged();
    failures += test_get_op_macro_defers_to_core();
    if (failures)
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
    printf("OK all read_expect tests passed\n");
    return 0;
}
