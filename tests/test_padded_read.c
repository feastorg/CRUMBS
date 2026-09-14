/*
 * Regression tests for reading CRUMBS frames over transports that return
 * more bytes than the peripheral wrote (issue #15).
 *
 * A Linux I2C controller must request a fixed byte count up front. A CRUMBS
 * peripheral writes only its actual frame and stops driving the bus, so the
 * controller clocks in 0xFF for the remainder and the read returns the full
 * requested count - the buffer size, not the frame size.
 *
 * crumbs_decode_message() enforces an exact-frame-length contract since
 * v0.12.4 (trailing bytes -> -1), so host read paths must trim to the
 * header-declared frame length before decoding. These tests pin that trim
 * (crumbs_frame_length) and the read/scan entry points that use it, using
 * fake read functions that pad like a real Linux bus read.
 */

#include "test_common.h"

/*
 * Captured from a live Slice_RLHT at 0x0A via `i2ctransfer -y 1 r31@0x0a`:
 * a 9-byte version reply (type=0x01 opcode=0x00 data_len=5, CRC 0xE3)
 * followed by 22 bytes of 0xFF bus padding.
 */
static const uint8_t k_bench_reply[CRUMBS_MESSAGE_MAX_SIZE] = {
    0x01, 0x00, 0x05, 0xb4, 0x04, 0x01, 0x00, 0x00, 0xe3,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define BENCH_FRAME_LEN 9u

/* ============================================================================
 * crumbs_frame_length()
 * ========================================================================= */

static int test_frame_length_trims_padded_read(void)
{
    size_t frame_len = 0u;
    int rc = crumbs_frame_length(k_bench_reply, sizeof(k_bench_reply), &frame_len);

    TEST_ASSERT("frame_length_padded", rc == 0, "expected rc=0 for padded read");
    TEST_ASSERT("frame_length_padded", frame_len == BENCH_FRAME_LEN,
                "expected header-declared length (9), not the read length (31)");

    printf("  frame_length trims padded read: PASS\n");
    return 0;
}

static int test_frame_length_exact_frame_is_noop(void)
{
    size_t frame_len = 0u;
    int rc = crumbs_frame_length(k_bench_reply, BENCH_FRAME_LEN, &frame_len);

    TEST_ASSERT("frame_length_exact", rc == 0, "expected rc=0 for exact frame");
    TEST_ASSERT("frame_length_exact", frame_len == BENCH_FRAME_LEN,
                "exact-length input must pass through unchanged");

    printf("  frame_length exact frame no-op: PASS\n");
    return 0;
}

static int test_frame_length_zero_payload(void)
{
    /* data_len = 0 -> frame is header (3) + crc (1) = 4 bytes, then padding. */
    const uint8_t frame[] = {0x01, 0x02, 0x00, 0x55, 0xff, 0xff, 0xff};
    size_t frame_len = 0u;
    int rc = crumbs_frame_length(frame, sizeof(frame), &frame_len);

    TEST_ASSERT("frame_length_zero_payload", rc == 0, "expected rc=0");
    TEST_ASSERT("frame_length_zero_payload", frame_len == 4u, "expected frame_len=4");

    printf("  frame_length zero payload: PASS\n");
    return 0;
}

static int test_frame_length_rejects_silent_device(void)
{
    /* All 0xFF: the device ACKed its address but supplied no data, so the
       data_len byte (0xFF) is out of range. */
    uint8_t garbage[CRUMBS_MESSAGE_MAX_SIZE];
    memset(garbage, 0xff, sizeof(garbage));

    size_t frame_len = 0u;
    int rc = crumbs_frame_length(garbage, sizeof(garbage), &frame_len);

    TEST_ASSERT("frame_length_garbage", rc != 0, "all-0xFF read must be rejected");

    printf("  frame_length rejects silent device: PASS\n");
    return 0;
}

static int test_frame_length_rejects_truncated(void)
{
    /* Header declares 5 payload bytes (frame = 9) but only 6 were read. */
    size_t frame_len = 0u;
    int rc = crumbs_frame_length(k_bench_reply, 6u, &frame_len);

    TEST_ASSERT("frame_length_truncated", rc != 0, "truncated frame must be rejected");

    printf("  frame_length rejects truncated frame: PASS\n");
    return 0;
}

static int test_frame_length_rejects_bad_args(void)
{
    size_t frame_len = 0u;

    TEST_ASSERT("frame_length_args", crumbs_frame_length(NULL, 31u, &frame_len) != 0,
                "NULL buffer must be rejected");
    TEST_ASSERT("frame_length_args", crumbs_frame_length(k_bench_reply, 31u, NULL) != 0,
                "NULL out pointer must be rejected");
    TEST_ASSERT("frame_length_args", crumbs_frame_length(k_bench_reply, 3u, &frame_len) != 0,
                "read shorter than the minimum frame must be rejected");

    printf("  frame_length rejects bad args: PASS\n");
    return 0;
}

/* ============================================================================
 * crumbs_decode_message() contract is unchanged
 * ========================================================================= */

static int test_decode_still_rejects_raw_padded_length(void)
{
    crumbs_context_t ctx;
    crumbs_message_t msg;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0u);

    int rc = crumbs_decode_message(k_bench_reply, sizeof(k_bench_reply), &msg, &ctx);

    TEST_ASSERT("decode_contract", rc == -1,
                "decode must keep rejecting trailing bytes (v0.12.4 contract)");
    TEST_ASSERT("decode_contract", ctx.last_crc_ok == 0u,
                "rejected decode must clear last_crc_ok");

    printf("  decode still rejects raw padded length: PASS\n");
    return 0;
}

/* ============================================================================
 * crumbs_controller_read() with a padding transport
 * ========================================================================= */

/* Fake read that behaves like Linux i2c-dev: fills the entire requested
   buffer, frame first, 0xFF padding after, and returns the requested count. */
static int fake_padded_read(void *user_ctx, uint8_t addr, uint8_t *buffer,
                            size_t len, uint32_t timeout_us)
{
    (void)user_ctx;
    (void)addr;
    (void)timeout_us;

    size_t n = len < sizeof(k_bench_reply) ? len : sizeof(k_bench_reply);
    memcpy(buffer, k_bench_reply, n);
    return (int)len;
}

static int fake_silent_read(void *user_ctx, uint8_t addr, uint8_t *buffer,
                            size_t len, uint32_t timeout_us)
{
    (void)user_ctx;
    (void)addr;
    (void)timeout_us;

    memset(buffer, 0xff, len);
    return (int)len;
}

static int test_controller_read_decodes_padded_reply(void)
{
    crumbs_context_t ctx;
    crumbs_message_t msg;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0u);
    memset(&msg, 0, sizeof(msg));

    int rc = crumbs_controller_read(&ctx, 0x0A, &msg, fake_padded_read, NULL);

    TEST_ASSERT("controller_read_padded", rc == 0,
                "controller_read must decode a padded Linux-style read");
    TEST_ASSERT("controller_read_padded", msg.type_id == 0x01, "wrong type_id");
    TEST_ASSERT("controller_read_padded", msg.opcode == 0x00, "wrong opcode");
    TEST_ASSERT("controller_read_padded", msg.data_len == 5u, "wrong data_len");
    TEST_ASSERT("controller_read_padded", msg.crc8 == 0xE3, "wrong crc8");
    TEST_ASSERT("controller_read_padded", ctx.last_crc_ok == 1u,
                "successful decode must set last_crc_ok");

    printf("  controller_read decodes padded reply: PASS\n");
    return 0;
}

static int test_controller_read_rejects_silent_device(void)
{
    crumbs_context_t ctx;
    crumbs_message_t msg;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0u);

    int rc = crumbs_controller_read(&ctx, 0x0A, &msg, fake_silent_read, NULL);

    TEST_ASSERT("controller_read_silent", rc != 0,
                "an all-0xFF read must not decode");
    TEST_ASSERT("controller_read_silent", ctx.last_crc_ok == 0u,
                "failed read must clear last_crc_ok");

    printf("  controller_read rejects silent device: PASS\n");
    return 0;
}

/* ============================================================================
 * Scanner with a padding transport
 * ========================================================================= */

#define PADDED_DEV 0x0A

static int fake_padded_scan_read(void *user_ctx, uint8_t addr, uint8_t *buffer,
                                 size_t len, uint32_t timeout_us)
{
    (void)user_ctx;
    (void)timeout_us;

    if (addr != PADDED_DEV)
        return -1; /* no device: address NAK */

    return fake_padded_read(NULL, addr, buffer, len, 0u);
}

static int test_scan_finds_device_behind_padded_reads(void)
{
    uint8_t found[4];
    uint8_t types[4];

    int n = crumbs_controller_scan_for_crumbs_with_types(
        NULL, 0x08, 0x10, 1 /* strict */, NULL, fake_padded_scan_read, NULL,
        found, types, 4u, 0u);

    TEST_ASSERT("scan_padded", n == 1, "expected exactly one device found");
    TEST_ASSERT("scan_padded", found[0] == PADDED_DEV, "wrong address found");
    TEST_ASSERT("scan_padded", types[0] == 0x01, "wrong type_id reported");

    printf("  scan finds device behind padded reads: PASS\n");
    return 0;
}

int main(void)
{
    int failures = 0;

    printf("Running padded-read tests:\n");

    failures += test_frame_length_trims_padded_read();
    failures += test_frame_length_exact_frame_is_noop();
    failures += test_frame_length_zero_payload();
    failures += test_frame_length_rejects_silent_device();
    failures += test_frame_length_rejects_truncated();
    failures += test_frame_length_rejects_bad_args();
    failures += test_decode_still_rejects_raw_padded_length();
    failures += test_controller_read_decodes_padded_reply();
    failures += test_controller_read_rejects_silent_device();
    failures += test_scan_finds_device_behind_padded_reads();

    if (failures == 0)
    {
        printf("OK all padded-read tests passed\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
}
