/*
 * Unit tests for the per-command handler dispatch system.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"

/* ---- Test infrastructure ---------------------------------------------- */

static int g_handler_call_count = 0;
static uint8_t g_last_opcode = 0;
static uint8_t g_last_data[CRUMBS_MAX_PAYLOAD];
static uint8_t g_last_data_len = 0;
static void *g_last_user_data = NULL;

static void reset_handler_state(void)
{
    g_handler_call_count = 0;
    g_last_opcode = 0;
    memset(g_last_data, 0, sizeof(g_last_data));
    g_last_data_len = 0;
    g_last_user_data = NULL;
}

static void test_handler(crumbs_context_t *ctx,
                         uint8_t opcode,
                         const uint8_t *data,
                         uint8_t data_len,
                         void *user_data)
{
    (void)ctx;
    g_handler_call_count++;
    g_last_opcode = opcode;
    g_last_data_len = data_len;
    if (data && data_len > 0 && data_len <= CRUMBS_MAX_PAYLOAD)
    {
        memcpy(g_last_data, data, data_len);
    }
    g_last_user_data = user_data;
}

/* ---- Tests ------------------------------------------------------------ */

static int test_register_handler_success(void)
{
    crumbs_context_t ctx = {0};  /* Zero-init required */
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    int rc = crumbs_register_handler(&ctx, 0x42, test_handler, (void *)(uintptr_t)0xDEADBEEF);
    if (rc != 0)
    {
        fprintf(stderr, "register_handler returned %d, expected 0\n", rc);
        return 1;
    }

    printf("  register_handler success: PASS\n");
    return 0;
}

static int test_register_handler_null_ctx(void)
{
    int rc = crumbs_register_handler(NULL, 0x00, test_handler, NULL);
    if (rc != -1)
    {
        fprintf(stderr, "register_handler(NULL) returned %d, expected -1\n", rc);
        return 1;
    }

    printf("  register_handler NULL ctx: PASS\n");
    return 0;
}

static int test_unregister_handler(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* Register then unregister */
    crumbs_register_handler(&ctx, 0x55, test_handler, (void *)(uintptr_t)0x1234);
    int rc = crumbs_unregister_handler(&ctx, 0x55);
    if (rc != 0)
    {
        fprintf(stderr, "unregister_handler returned %d, expected 0\n", rc);
        return 1;
    }

    printf("  unregister_handler: PASS\n");
    return 0;
}

static int test_handler_overwrite(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* Register handler for command 0x10 */
    crumbs_register_handler(&ctx, 0x10, test_handler, (void *)(uintptr_t)0x1111);

    /* Overwrite with different user_data */
    crumbs_register_handler(&ctx, 0x10, test_handler, (void *)(uintptr_t)0x2222);

    /* Simulate a message to verify which user_data is used */
    reset_handler_state();

    /* Create a valid encoded frame */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x01;
    msg.opcode = 0x10;
    msg.data_len = 2;
    msg.data[0] = 0xAA;
    msg.data[1] = 0xBB;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    crumbs_peripheral_handle_receive(&ctx, buf, len);

    if (g_last_user_data != (void *)(uintptr_t)0x2222)
    {
        fprintf(stderr, "handler_overwrite: user_data should be 0x2222, got %p\n", g_last_user_data);
        return 1;
    }

    printf("  handler overwrite: PASS\n");
    return 0;
}

static int test_handler_dispatch(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    reset_handler_state();

    /* Register handler for command 0x42 */
    crumbs_register_handler(&ctx, 0x42, test_handler, (void *)(uintptr_t)0xCAFE);

    /* Create and encode a message */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x01;
    msg.opcode = 0x42;
    msg.data_len = 3;
    msg.data[0] = 0x11;
    msg.data[1] = 0x22;
    msg.data[2] = 0x33;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    int rc = crumbs_peripheral_handle_receive(&ctx, buf, len);
    if (rc != 0)
    {
        fprintf(stderr, "handle_receive returned %d\n", rc);
        return 1;
    }

    if (g_handler_call_count != 1)
    {
        fprintf(stderr, "handler should have been called once, got %d\n", g_handler_call_count);
        return 1;
    }

    if (g_last_opcode != 0x42)
    {
        fprintf(stderr, "handler got opcode 0x%02X, expected 0x42\n", g_last_opcode);
        return 1;
    }

    if (g_last_data_len != 3)
    {
        fprintf(stderr, "handler got data_len %u, expected 3\n", g_last_data_len);
        return 1;
    }

    if (g_last_data[0] != 0x11 || g_last_data[1] != 0x22 || g_last_data[2] != 0x33)
    {
        fprintf(stderr, "handler got wrong data bytes\n");
        return 1;
    }

    if (g_last_user_data != (void *)(uintptr_t)0xCAFE)
    {
        fprintf(stderr, "handler got wrong user_data\n");
        return 1;
    }

    printf("  handler dispatch: PASS\n");
    return 0;
}

static int test_no_handler_registered(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    reset_handler_state();

    /* Do NOT register a handler for command 0x99 */

    /* Create and encode a message */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x01;
    msg.opcode = 0x99;
    msg.data_len = 1;
    msg.data[0] = 0xFF;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    int rc = crumbs_peripheral_handle_receive(&ctx, buf, len);
    if (rc != 0)
    {
        fprintf(stderr, "handle_receive returned %d for unhandled command\n", rc);
        return 1;
    }

    /* Handler should NOT have been called */
    if (g_handler_call_count != 0)
    {
        fprintf(stderr, "no handler should have been called, got %d\n", g_handler_call_count);
        return 1;
    }

    printf("  no handler registered: PASS\n");
    return 0;
}

static int g_on_message_called = 0;

static void test_on_message(crumbs_context_t *ctx, const crumbs_message_t *msg)
{
    (void)ctx;
    (void)msg;
    g_on_message_called++;
}

static int test_handler_with_on_message(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);
    crumbs_set_callbacks(&ctx, test_on_message, NULL, NULL);

    reset_handler_state();
    g_on_message_called = 0;

    /* Register handler for command 0x77 */
    crumbs_register_handler(&ctx, 0x77, test_handler, NULL);

    /* Create and encode a message */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x01;
    msg.opcode = 0x77;
    msg.data_len = 0;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    crumbs_peripheral_handle_receive(&ctx, buf, len);

    /* Both on_message and handler should be called */
    if (g_on_message_called != 1)
    {
        fprintf(stderr, "on_message should have been called once, got %d\n", g_on_message_called);
        return 1;
    }

    if (g_handler_call_count != 1)
    {
        fprintf(stderr, "handler should have been called once, got %d\n", g_handler_call_count);
        return 1;
    }

    printf("  handler with on_message: PASS\n");
    return 0;
}

static int test_handler_zero_data(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    reset_handler_state();

    /* Register handler for command 0x00 */
    crumbs_register_handler(&ctx, 0x00, test_handler, (void *)(uintptr_t)0xABCD);

    /* Create message with zero-length payload */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x05;
    msg.opcode = 0x00;
    msg.data_len = 0;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    crumbs_peripheral_handle_receive(&ctx, buf, len);

    if (g_handler_call_count != 1)
    {
        fprintf(stderr, "handler should have been called once for zero-data, got %d\n", g_handler_call_count);
        return 1;
    }

    if (g_last_data_len != 0)
    {
        fprintf(stderr, "handler should receive data_len 0, got %u\n", g_last_data_len);
        return 1;
    }

    printf("  handler zero data: PASS\n");
    return 0;
}

static int test_handler_max_data(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    reset_handler_state();

    /* Register handler for command 0xFF */
    crumbs_register_handler(&ctx, 0xFF, test_handler, NULL);

    /* Create message with max-length payload */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x0A;
    msg.opcode = 0xFF;
    msg.data_len = CRUMBS_MAX_PAYLOAD;
    for (uint8_t i = 0; i < CRUMBS_MAX_PAYLOAD; ++i)
    {
        msg.data[i] = i;
    }

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    crumbs_peripheral_handle_receive(&ctx, buf, len);

    if (g_handler_call_count != 1)
    {
        fprintf(stderr, "handler should have been called once for max-data, got %d\n", g_handler_call_count);
        return 1;
    }

    if (g_last_data_len != CRUMBS_MAX_PAYLOAD)
    {
        fprintf(stderr, "handler should receive data_len %u, got %u\n", CRUMBS_MAX_PAYLOAD, g_last_data_len);
        return 1;
    }

    for (uint8_t i = 0; i < CRUMBS_MAX_PAYLOAD; ++i)
    {
        if (g_last_data[i] != i)
        {
            fprintf(stderr, "handler max-data mismatch at index %u\n", i);
            return 1;
        }
    }

    printf("  handler max data: PASS\n");
    return 0;
}

static int test_handler_table_full(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* Fill the handler table to capacity */
    for (uint8_t i = 0; i < CRUMBS_MAX_HANDLERS; ++i)
    {
        int rc = crumbs_register_handler(&ctx, i, test_handler, NULL);
        if (rc != 0)
        {
            fprintf(stderr, "handler_table_full: registration %u failed early\n", i);
            return 1;
        }
    }

    /* Next registration should fail (table full) */
    int rc = crumbs_register_handler(&ctx, 0xFF, test_handler, NULL);
    if (rc != -1)
    {
        fprintf(stderr, "handler_table_full: should return -1 when full\n");
        return 1;
    }

    printf("  handler table full: PASS\n");
    return 0;
}

static int test_unregister_nonexistent(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    /* Unregister a command that was never registered */
    int rc = crumbs_unregister_handler(&ctx, 0x99);

    /* Should succeed gracefully (no-op) */
    if (rc != 0)
    {
        fprintf(stderr, "unregister_nonexistent: should return 0\n");
        return 1;
    }

    printf("  unregister nonexistent: PASS\n");
    return 0;
}

static int test_handler_wrong_command_not_called(void)
{
    crumbs_context_t ctx = {0};
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, 0x10);

    reset_handler_state();

    /* Register handler for command 0x42 ONLY */
    crumbs_register_handler(&ctx, 0x42, test_handler, NULL);

    /* Send a message with a DIFFERENT command (0x99) */
    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type_id = 0x01;
    msg.opcode = 0x99; /* Not registered */
    msg.data_len = 0;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t len = crumbs_encode_message(&msg, buf, sizeof(buf));

    crumbs_peripheral_handle_receive(&ctx, buf, len);

    /* Handler should NOT have been called */
    if (g_handler_call_count != 0)
    {
        fprintf(stderr, "handler_wrong_command: handler should not be called for unregistered command\n");
        return 1;
    }

    printf("  handler wrong command not called: PASS\n");
    return 0;
}

/* ---- Main ------------------------------------------------------------- */

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

    printf("Running handler tests:\n");

    failures += test_register_handler_success();
    failures += test_register_handler_null_ctx();
    failures += test_unregister_handler();
    failures += test_unregister_nonexistent();
    failures += test_handler_table_full();
    failures += test_handler_overwrite();
    failures += test_handler_dispatch();
    failures += test_no_handler_registered();
    failures += test_handler_wrong_command_not_called();
    failures += test_handler_with_on_message();
    failures += test_handler_zero_data();
    failures += test_handler_max_data();

    if (failures == 0)
    {
        printf("OK all handler tests passed\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
#endif /* CRUMBS_MAX_HANDLERS == 0 */
}
