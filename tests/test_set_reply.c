/*
 * Unit tests for the SET_REPLY (0xFE) mechanism.
 *
 * Tests the core behavior:
 * - Initial requested_opcode is 0
 * - SET_REPLY stores target opcode in ctx->requested_opcode
 * - Multiple SET_REPLY overwrites previous value
 * - SET_REPLY is NOT dispatched to user handlers or on_message
 * - SET_REPLY with empty payload is ignored (no crash)
 */

#include "test_common.h"
#include "crumbs_message_helpers.h"

/* ---- Test infrastructure ---------------------------------------------- */

static int g_on_message_call_count = 0;
static uint8_t g_last_msg_opcode = 0;

static int g_handler_call_count = 0;
static uint8_t g_handler_last_opcode = 0;

static void reset_test_state(void)
{
    g_on_message_call_count = 0;
    g_last_msg_opcode = 0;
    g_handler_call_count = 0;
    g_handler_last_opcode = 0;
}

static void test_on_message(crumbs_context_t *ctx, const crumbs_message_t *msg)
{
    (void)ctx;
    g_on_message_call_count++;
    g_last_msg_opcode = msg->opcode;
}

static void test_handler(crumbs_context_t *ctx,
                         uint8_t opcode,
                         const uint8_t *data,
                         uint8_t data_len,
                         void *user_data)
{
    (void)ctx;
    (void)data;
    (void)data_len;
    (void)user_data;
    g_handler_call_count++;
    g_handler_last_opcode = opcode;
}

/**
 * @brief Build a SET_REPLY frame for testing.
 *
 * Wire format: [type_id][0xFE][data_len][target_opcode][CRC8]
 */
static size_t build_set_reply_frame(uint8_t type_id,
                                    uint8_t target_opcode,
                                    uint8_t *buffer,
                                    size_t buffer_len)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, type_id, CRUMBS_CMD_SET_REPLY);
    crumbs_msg_add_u8(&msg, target_opcode);
    return crumbs_encode_message(&msg, buffer, buffer_len);
}

/**
 * @brief Build a SET_REPLY frame with no payload (edge case).
 */
static size_t build_set_reply_empty(uint8_t type_id,
                                    uint8_t *buffer,
                                    size_t buffer_len)
{
    crumbs_message_t msg;
    crumbs_msg_init(&msg, type_id, CRUMBS_CMD_SET_REPLY);
    /* No payload - testing edge case */
    return crumbs_encode_message(&msg, buffer, buffer_len);
}

/* ---- Tests ------------------------------------------------------------ */

/**
 * Test: Initial requested_opcode is 0 after crumbs_init.
 */
static int test_initial_requested_opcode_is_zero(void)
{
    const char *test_name = "initial_requested_opcode";
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0,
                   "requested_opcode should be 0 after init");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/**
 * Test: SET_REPLY stores target opcode in ctx->requested_opcode.
 */
static int test_set_reply_stores_opcode(void)
{
    const char *test_name = "set_reply_stores_opcode";
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = build_set_reply_frame(0x01, 0x42, frame, sizeof(frame));
    TEST_ASSERT(test_name, len > 0, "failed to build SET_REPLY frame");

    int rc = crumbs_peripheral_handle_receive(&ctx, frame, len);
    TEST_ASSERT_EQ(test_name, rc, 0, "handle_receive should return 0");
    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0x42,
                   "requested_opcode should be 0x42");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/**
 * Test: Multiple SET_REPLY commands overwrite previous value.
 */
static int test_set_reply_overwrites(void)
{
    const char *test_name = "set_reply_overwrites";
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];

    /* First SET_REPLY: target 0x10 */
    size_t len = build_set_reply_frame(0x01, 0x10, frame, sizeof(frame));
    crumbs_peripheral_handle_receive(&ctx, frame, len);
    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0x10,
                   "first SET_REPLY should set 0x10");

    /* Second SET_REPLY: target 0x20 */
    len = build_set_reply_frame(0x01, 0x20, frame, sizeof(frame));
    crumbs_peripheral_handle_receive(&ctx, frame, len);
    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0x20,
                   "second SET_REPLY should overwrite to 0x20");

    /* Third SET_REPLY: target 0x00 (back to default) */
    len = build_set_reply_frame(0x01, 0x00, frame, sizeof(frame));
    crumbs_peripheral_handle_receive(&ctx, frame, len);
    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0x00,
                   "third SET_REPLY should reset to 0x00");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/**
 * Test: SET_REPLY is NOT dispatched to on_message callback.
 */
static int test_set_reply_not_dispatched_to_on_message(void)
{
    const char *test_name = "set_reply_no_on_message";
    reset_test_state();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_set_callbacks(&ctx, test_on_message, NULL, NULL);

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = build_set_reply_frame(0x01, 0x42, frame, sizeof(frame));

    crumbs_peripheral_handle_receive(&ctx, frame, len);

    TEST_ASSERT_EQ(test_name, g_on_message_call_count, 0,
                   "on_message should NOT be called for SET_REPLY");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/**
 * Test: SET_REPLY is NOT dispatched to registered handlers.
 */
#if CRUMBS_MAX_HANDLERS > 0
static int test_set_reply_not_dispatched_to_handlers(void)
{
    const char *test_name = "set_reply_no_handler";
    reset_test_state();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* Register a handler for 0xFE (should NOT be called) */
    crumbs_register_handler(&ctx, CRUMBS_CMD_SET_REPLY, test_handler, NULL);

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = build_set_reply_frame(0x01, 0x42, frame, sizeof(frame));

    crumbs_peripheral_handle_receive(&ctx, frame, len);

    TEST_ASSERT_EQ(test_name, g_handler_call_count, 0,
                   "handler for 0xFE should NOT be called");
    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0x42,
                   "requested_opcode should still be set");

    printf("  %s: PASS\n", test_name);
    return 0;
}
#endif /* CRUMBS_MAX_HANDLERS > 0 */

/**
 * Test: SET_REPLY with empty payload is gracefully ignored.
 */
static int test_set_reply_empty_payload(void)
{
    const char *test_name = "set_reply_empty_payload";
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* First set a known value */
    ctx.requested_opcode = 0x55;

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = build_set_reply_empty(0x01, frame, sizeof(frame));
    TEST_ASSERT(test_name, len > 0, "failed to build empty SET_REPLY frame");

    int rc = crumbs_peripheral_handle_receive(&ctx, frame, len);
    TEST_ASSERT_EQ(test_name, rc, 0, "handle_receive should return 0");

    /* requested_opcode should remain unchanged */
    TEST_ASSERT_EQ(test_name, ctx.requested_opcode, 0x55,
                   "empty SET_REPLY should not change requested_opcode");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/**
 * Test: Normal messages (non-SET_REPLY) still dispatch correctly.
 */
#if CRUMBS_MAX_HANDLERS > 0
static int test_normal_message_still_dispatches(void)
{
    const char *test_name = "normal_msg_dispatches";
    reset_test_state();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_set_callbacks(&ctx, test_on_message, NULL, NULL);
    crumbs_register_handler(&ctx, 0x42, test_handler, NULL);

    /* Send a normal message (not SET_REPLY) */
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0x01, 0x42);
    crumbs_msg_add_u8(&msg, 0xAB);

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, frame, sizeof(frame));

    crumbs_peripheral_handle_receive(&ctx, frame, len);

    TEST_ASSERT_EQ(test_name, g_on_message_call_count, 1,
                   "on_message should be called for normal message");
    TEST_ASSERT_EQ(test_name, g_last_msg_opcode, 0x42,
                   "on_message should receive opcode 0x42");
    TEST_ASSERT_EQ(test_name, g_handler_call_count, 1,
                   "handler should be called for normal message");
    TEST_ASSERT_EQ(test_name, g_handler_last_opcode, 0x42,
                   "handler should receive opcode 0x42");

    printf("  %s: PASS\n", test_name);
    return 0;
}
#endif /* CRUMBS_MAX_HANDLERS > 0 */

/**
 * Test: SET_REPLY constant is 0xFE.
 */
static int test_set_reply_constant(void)
{
    const char *test_name = "set_reply_constant";

    TEST_ASSERT_EQ(test_name, CRUMBS_CMD_SET_REPLY, 0xFE,
                   "CRUMBS_CMD_SET_REPLY should be 0xFE");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/* ---- Main ------------------------------------------------------------- */

int main(void)
{
    int failures = 0;

    printf("=== test_set_reply ===\n");

    failures += test_set_reply_constant();
    failures += test_initial_requested_opcode_is_zero();
    failures += test_set_reply_stores_opcode();
    failures += test_set_reply_overwrites();
    failures += test_set_reply_not_dispatched_to_on_message();
#if CRUMBS_MAX_HANDLERS > 0
    failures += test_set_reply_not_dispatched_to_handlers();
#else
    printf("  set_reply_no_handler: SKIP (handler table compiled out)\n");
#endif
    failures += test_set_reply_empty_payload();
#if CRUMBS_MAX_HANDLERS > 0
    failures += test_normal_message_still_dispatches();
#else
    printf("  normal_msg_dispatches: SKIP (handler table compiled out)\n");
#endif

    printf("\n");
    if (failures == 0)
    {
        printf("All SET_REPLY tests passed!\n");
    }
    else
    {
        printf("FAILED: %d test(s)\n", failures);
    }

    return failures;
}
