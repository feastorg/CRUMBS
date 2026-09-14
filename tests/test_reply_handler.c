/*
 * Unit tests for crumbs_register_reply_handler() and per-opcode GET dispatch.
 *
 * Tests:
 * - Registration succeeds; null ctx returns -1
 * - Registered handler is dispatched on crumbs_peripheral_build_reply()
 * - Handler fills reply with correct type_id, opcode, and payload
 * - Unregistered opcode falls through to on_request callback
 * - on_request still works when no reply handlers are configured
 * - Reply handler takes priority over on_request for the same opcode
 * - Overwriting a registered opcode replaces the handler
 * - Unregistering a handler restores fall-through to on_request
 * - Table-full returns -1
 */

#include "test_common.h"
#include "crumbs_message_helpers.h"

#include <string.h> /* memcpy */

/* ---- Shared constants ------------------------------------------------- */

#define MY_TYPE_ID  0x55
#define OP_GET_A    0x80
#define OP_GET_B    0x81
#define OP_GET_C    0x82

/* ---- Test call tracking ----------------------------------------------- */

static int g_handler_a_calls = 0;
static int g_handler_b_calls = 0;
static int g_on_request_calls = 0;
static uint8_t g_on_request_last_opcode = 0;

static void reset_call_counts(void)
{
    g_handler_a_calls    = 0;
    g_handler_b_calls    = 0;
    g_on_request_calls   = 0;
    g_on_request_last_opcode = 0;
}

/* ---- Reply handlers --------------------------------------------------- */

static void reply_handler_a(crumbs_context_t *ctx,
                            crumbs_message_t *reply,
                            void *user_data)
{
    (void)ctx;
    (void)user_data;
    g_handler_a_calls++;
    crumbs_msg_init(reply, MY_TYPE_ID, OP_GET_A);
    crumbs_msg_add_u8(reply, 0xAA);
}

static void reply_handler_b(crumbs_context_t *ctx,
                            crumbs_message_t *reply,
                            void *user_data)
{
    (void)ctx;
    (void)user_data;
    g_handler_b_calls++;
    crumbs_msg_init(reply, MY_TYPE_ID, OP_GET_B);
    crumbs_msg_add_u8(reply, 0xBB);
}

/* ---- on_request fallback --------------------------------------------- */

static void fallback_on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    g_on_request_calls++;
    g_on_request_last_opcode = ctx->requested_opcode;
    crumbs_msg_init(reply, MY_TYPE_ID, ctx->requested_opcode);
    crumbs_msg_add_u8(reply, 0xFF);
}

/* ---- Helpers ---------------------------------------------------------- */

/**
 * @brief Set requested_opcode by feeding a SET_REPLY frame to the peripheral.
 */
static int set_reply(crumbs_context_t *ctx, uint8_t target_opcode)
{
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    crumbs_message_t msg;
    crumbs_msg_init(&msg, 0, CRUMBS_CMD_SET_REPLY);
    crumbs_msg_add_u8(&msg, target_opcode);
    size_t len = crumbs_encode_message(&msg, frame, sizeof(frame));
    if (len == 0)
        return -1;
    return crumbs_peripheral_handle_receive(ctx, frame, len);
}

/**
 * @brief Call crumbs_peripheral_build_reply and decode the result.
 * Returns 0 on success; populates *out.
 */
static int build_and_decode(crumbs_context_t *ctx, crumbs_message_t *out)
{
    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = 0;
    int rc = crumbs_peripheral_build_reply(ctx, buf, sizeof(buf), &len);
    if (rc != 0)
        return rc;
    if (len == 0)
        return -99; /* no reply produced */
    return crumbs_decode_message(buf, len, out, NULL);
}

/* ---- Tests ------------------------------------------------------------ */

static int test_register_success(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    int rc = crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);
    if (rc != 0)
    {
        fprintf(stderr, "register_reply_handler returned %d, expected 0\n", rc);
        return 1;
    }

    printf("  register_success: PASS\n");
    return 0;
}

static int test_register_null_ctx(void)
{
    int rc = crumbs_register_reply_handler(NULL, OP_GET_A, reply_handler_a, NULL);
    if (rc != -1)
    {
        fprintf(stderr, "register_reply_handler(NULL) returned %d, expected -1\n", rc);
        return 1;
    }

    printf("  register_null_ctx: PASS\n");
    return 0;
}

static int test_handler_dispatched(void)
{
    const char *test_name = "handler_dispatched";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);

    set_reply(&ctx, OP_GET_A);

    crumbs_message_t reply;
    int rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0, "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_handler_a_calls, 1,  "handler_a should be called once");
    TEST_ASSERT_EQ(test_name, g_on_request_calls, 0, "on_request should not be called");
    TEST_ASSERT_EQ(test_name, reply.type_id, MY_TYPE_ID, "type_id mismatch");
    TEST_ASSERT_EQ(test_name, reply.opcode, OP_GET_A,    "opcode mismatch");
    TEST_ASSERT_EQ(test_name, reply.data_len, 1,         "data_len should be 1");
    TEST_ASSERT_EQ(test_name, reply.data[0], 0xAA,       "payload byte mismatch");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_handler_correct_opcode_selected(void)
{
    const char *test_name = "handler_correct_opcode_selected";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);
    crumbs_register_reply_handler(&ctx, OP_GET_B, reply_handler_b, NULL);

    /* Request opcode B — handler_b should fire, not handler_a */
    set_reply(&ctx, OP_GET_B);

    crumbs_message_t reply;
    int rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0, "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_handler_a_calls, 0, "handler_a should NOT be called");
    TEST_ASSERT_EQ(test_name, g_handler_b_calls, 1, "handler_b should be called once");
    TEST_ASSERT_EQ(test_name, reply.opcode, OP_GET_B, "opcode should be OP_GET_B");
    TEST_ASSERT_EQ(test_name, reply.data[0], 0xBB,    "payload should be 0xBB");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_fallthrough_to_on_request(void)
{
    const char *test_name = "fallthrough_to_on_request";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);
    crumbs_set_callbacks(&ctx, NULL, fallback_on_request, NULL);

    /* Request opcode C — no reply handler registered for it; should fall to on_request */
    set_reply(&ctx, OP_GET_C);

    crumbs_message_t reply;
    int rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0,        "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_handler_a_calls, 0, "handler_a should NOT be called");
    TEST_ASSERT_EQ(test_name, g_on_request_calls, 1, "on_request should be called once");
    TEST_ASSERT_EQ(test_name, g_on_request_last_opcode, OP_GET_C, "on_request opcode mismatch");
    TEST_ASSERT_EQ(test_name, reply.opcode, OP_GET_C, "reply opcode mismatch");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_on_request_only(void)
{
    const char *test_name = "on_request_only";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    /* No reply handlers registered — only on_request */
    crumbs_set_callbacks(&ctx, NULL, fallback_on_request, NULL);

    set_reply(&ctx, OP_GET_A);

    crumbs_message_t reply;
    int rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0, "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_on_request_calls, 1, "on_request should be called");
    TEST_ASSERT_EQ(test_name, reply.opcode, OP_GET_A, "opcode mismatch");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_handler_priority_over_on_request(void)
{
    const char *test_name = "handler_priority_over_on_request";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);
    crumbs_set_callbacks(&ctx, NULL, fallback_on_request, NULL);

    /* Request OP_GET_A — handler takes priority; on_request must NOT fire */
    set_reply(&ctx, OP_GET_A);

    crumbs_message_t reply;
    int rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0,              "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_handler_a_calls, 1,  "handler_a should be called");
    TEST_ASSERT_EQ(test_name, g_on_request_calls, 0, "on_request should NOT be called");
    TEST_ASSERT_EQ(test_name, reply.data[0], 0xAA,   "handler_a payload expected");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_overwrite(void)
{
    const char *test_name = "overwrite";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);
    /* Overwrite with handler_b for the same opcode */
    int rc = crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_b, NULL);
    TEST_ASSERT_EQ(test_name, rc, 0, "overwrite registration should succeed");

    set_reply(&ctx, OP_GET_A);

    crumbs_message_t reply;
    rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0,              "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_handler_a_calls, 0, "original handler_a should NOT fire");
    TEST_ASSERT_EQ(test_name, g_handler_b_calls, 1, "replacement handler_b should fire");
    TEST_ASSERT_EQ(test_name, reply.data[0], 0xBB,  "handler_b payload expected");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_unregister(void)
{
    const char *test_name = "unregister";
    reset_call_counts();

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_a, NULL);
    crumbs_set_callbacks(&ctx, NULL, fallback_on_request, NULL);

    /* Unregister by passing NULL fn */
    int rc = crumbs_register_reply_handler(&ctx, OP_GET_A, NULL, NULL);
    TEST_ASSERT_EQ(test_name, rc, 0, "unregister should succeed");

    set_reply(&ctx, OP_GET_A);

    crumbs_message_t reply;
    rc = build_and_decode(&ctx, &reply);
    TEST_ASSERT_EQ(test_name, rc, 0,              "build_and_decode should succeed");
    TEST_ASSERT_EQ(test_name, g_handler_a_calls, 0,  "handler_a should NOT fire after unregister");
    TEST_ASSERT_EQ(test_name, g_on_request_calls, 1, "on_request should fire after unregister");

    printf("  %s: PASS\n", test_name);
    return 0;
}

static int test_no_reply_configured(void)
{
    const char *test_name = "no_reply_configured";

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    /* Neither reply handlers nor on_request */

    set_reply(&ctx, OP_GET_A);

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = 0;
    int rc = crumbs_peripheral_build_reply(&ctx, buf, sizeof(buf), &len);
    TEST_ASSERT_EQ(test_name, rc, 0,   "build_reply should succeed (returns 0, empty)");
    TEST_ASSERT_EQ(test_name, (int)len, 0, "len should be 0 (no reply)");

    printf("  %s: PASS\n", test_name);
    return 0;
}

/* File-scope state for test_user_data_forwarded */
static void *g_captured_ud = NULL;
static int   g_ud_handler_calls = 0;

static void reply_handler_captures_ud(crumbs_context_t *ctx,
                                      crumbs_message_t *reply,
                                      void *user_data)
{
    (void)ctx;
    g_ud_handler_calls++;
    g_captured_ud = user_data;
    crumbs_msg_init(reply, MY_TYPE_ID, OP_GET_A);
}

static int test_user_data_forwarded(void)
{
    const char *test_name = "user_data_forwarded";
    g_ud_handler_calls = 0;
    g_captured_ud = NULL;

    void *expected_ptr = (void *)(uintptr_t)0xCAFEBABEu;

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_register_reply_handler(&ctx, OP_GET_A, reply_handler_captures_ud, expected_ptr);

    set_reply(&ctx, OP_GET_A);

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = 0;
    int rc = crumbs_peripheral_build_reply(&ctx, buf, sizeof(buf), &len);
    TEST_ASSERT_EQ(test_name, rc, 0,                "build_reply should succeed");
    TEST_ASSERT_EQ(test_name, g_ud_handler_calls, 1, "handler should be called");
    if (g_captured_ud != expected_ptr)
    {
        fprintf(stderr, "  %s: FAIL — user_data not forwarded (got %p, want %p)\n",
                test_name, g_captured_ud, expected_ptr);
        return 1;
    }

    printf("  %s: PASS\n", test_name);
    return 0;
}

#if CRUMBS_MAX_HANDLERS > 0
static int test_table_full(void)
{
    const char *test_name = "table_full";

    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* Fill the table */
    int i;
    for (i = 0; i < CRUMBS_MAX_HANDLERS; i++)
    {
        int rc = crumbs_register_reply_handler(&ctx, (uint8_t)i, reply_handler_a, NULL);
        if (rc != 0)
        {
            fprintf(stderr, "  %s: FAIL — registration %d returned %d\n", test_name, i, rc);
            return 1;
        }
    }

    /* One more should fail */
    int rc = crumbs_register_reply_handler(&ctx, 0xFF, reply_handler_a, NULL);
    TEST_ASSERT_EQ(test_name, rc, -1, "over-limit registration should return -1");

    printf("  %s: PASS\n", test_name);
    return 0;
}
#endif /* CRUMBS_MAX_HANDLERS > 0 */

/* ---- main ------------------------------------------------------------- */

int main(void)
{
#if CRUMBS_MAX_HANDLERS == 0
    /* The handler table is compiled out; nothing here can run. Exit with the
       CTest skip code (see SKIP_RETURN_CODE in CMakeLists.txt) so the
       configuration reports "skipped", not a vacuous pass. */
    printf("SKIP: CRUMBS_MAX_HANDLERS == 0, handler table compiled out\n");
    return 77;
#else
    int failures = 0;

    printf("Running reply handler tests:\n");

    failures += test_register_success();
    failures += test_register_null_ctx();
    failures += test_handler_dispatched();
    failures += test_handler_correct_opcode_selected();
    failures += test_fallthrough_to_on_request();
    failures += test_on_request_only();
    failures += test_handler_priority_over_on_request();
    failures += test_overwrite();
    failures += test_unregister();
    failures += test_no_reply_configured();
    failures += test_user_data_forwarded();
#if CRUMBS_MAX_HANDLERS > 0
    failures += test_table_full();
#endif

    if (failures == 0)
    {
        printf("OK all reply handler tests passed\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
#endif /* CRUMBS_MAX_HANDLERS == 0 */
}
