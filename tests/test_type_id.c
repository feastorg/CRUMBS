/*
 * Unit tests for the peripheral-side type_id check (#37) and the type_id 0x00
 * wildcard (#46).
 *
 * A context that declares a type_id rejects frames carrying a different,
 * non-zero type_id before any callback or handler runs. A context that has
 * not declared one (type_id 0, the default) behaves exactly as before. A
 * frame whose type_id is 0x00 is a wildcard and is never rejected on type:
 * the library's own SET_REPLY frames and the scan probe carry 0x00.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"

#define MY_TYPE 0x21
#define OTHER_TYPE 0x22
#define MY_ADDR 0x10

static int g_on_message_calls;
static int g_handler_calls;

static void on_message(crumbs_context_t *ctx, const crumbs_message_t *msg)
{
    (void)ctx;
    (void)msg;
    g_on_message_calls++;
}

static void handler(crumbs_context_t *ctx, uint8_t opcode, const uint8_t *data,
                    uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;
    g_handler_calls++;
}

static void setup(crumbs_context_t *ctx, uint8_t declared_type)
{
    g_on_message_calls = 0;
    g_handler_calls = 0;
    crumbs_init(ctx, CRUMBS_ROLE_PERIPHERAL, MY_ADDR);
    crumbs_set_callbacks(ctx, on_message, NULL, NULL);
#if CRUMBS_MAX_HANDLERS > 0
    crumbs_register_handler(ctx, 0x42, handler, NULL);
#endif
    if (declared_type != 0u)
        crumbs_set_type_id(ctx, declared_type);
}

/* Encode a frame with the given type_id and opcode and feed it to the
   peripheral; returns handle_receive's rc. */
static int deliver(crumbs_context_t *ctx, uint8_t type_id, uint8_t opcode, uint8_t payload)
{
    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = type_id;
    m.opcode = opcode;
    m.data_len = 1;
    m.data[0] = payload;

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t n = crumbs_encode_message(&m, frame, sizeof(frame));
    if (n == 0)
        return -99;
    return crumbs_peripheral_handle_receive(ctx, frame, n);
}

static int expect(const char *name, int cond, const char *detail)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s: %s\n", name, detail);
        return 1;
    }
    printf("  %s: PASS\n", name);
    return 0;
}

static int test_default_context_accepts_any_type(void)
{
    crumbs_context_t ctx;
    setup(&ctx, 0u);
    int rc = deliver(&ctx, OTHER_TYPE, 0x42, 1);
    return expect("undeclared type accepts any frame", rc == 0 && g_on_message_calls == 1,
                  "frame should dispatch when ctx->type_id is 0");
}

static int test_init_zeroes_type_id(void)
{
    crumbs_context_t ctx;
    memset(&ctx, 0xA5, sizeof(ctx)); /* poison, then init must clear it */
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, MY_ADDR);
    return expect("crumbs_init zeroes type_id", ctx.type_id == 0u,
                  "type_id must default to 0 (not declared)");
}

static int test_matching_type_dispatches(void)
{
    crumbs_context_t ctx;
    setup(&ctx, MY_TYPE);
    int rc = deliver(&ctx, MY_TYPE, 0x42, 1);
    int handlers_ok = 1;
#if CRUMBS_MAX_HANDLERS > 0
    handlers_ok = (g_handler_calls == 1);
#endif
    return expect("matching type dispatches", rc == 0 && g_on_message_calls == 1 && handlers_ok,
                  "frame with the declared type must reach on_message and the handler");
}

static int test_mismatching_type_is_rejected(void)
{
    crumbs_context_t ctx;
    setup(&ctx, MY_TYPE);
    int rc = deliver(&ctx, OTHER_TYPE, 0x42, 1);
    return expect("mismatching type rejected",
                  rc == CRUMBS_RX_TYPE_MISMATCH && g_on_message_calls == 0 && g_handler_calls == 0,
                  "frame with another type must return CRUMBS_RX_TYPE_MISMATCH and reach nothing");
}

static int test_wildcard_type_dispatches(void)
{
    crumbs_context_t ctx;
    setup(&ctx, MY_TYPE);
    int rc = deliver(&ctx, 0x00, 0x42, 1);
    return expect("type_id 0x00 wildcard dispatches", rc == 0 && g_on_message_calls == 1,
                  "a 0x00 frame must not be rejected on type");
}

static int test_set_reply_wildcard_honoured(void)
{
    /* What the library's own getters send: type_id 0, opcode 0xFE. */
    crumbs_context_t ctx;
    setup(&ctx, MY_TYPE);
    int rc = deliver(&ctx, 0x00, CRUMBS_CMD_SET_REPLY, 0x80);
    return expect("SET_REPLY with wildcard type honoured",
                  rc == 0 && ctx.requested_opcode == 0x80 && g_on_message_calls == 0,
                  "SET_REPLY carrying type 0 must set requested_opcode on a typed target");
}

static int test_set_reply_wrong_type_ignored(void)
{
    crumbs_context_t ctx;
    setup(&ctx, MY_TYPE);
    int rc = deliver(&ctx, OTHER_TYPE, CRUMBS_CMD_SET_REPLY, 0x80);
    return expect("SET_REPLY with wrong type ignored",
                  rc == CRUMBS_RX_TYPE_MISMATCH && ctx.requested_opcode == 0u,
                  "SET_REPLY addressed to another type must not change requested_opcode");
}

static int test_mismatch_is_not_a_crc_error(void)
{
    crumbs_context_t ctx;
    setup(&ctx, MY_TYPE);
    (void)deliver(&ctx, OTHER_TYPE, 0x42, 1);
    return expect("mismatch leaves CRC stats alone",
                  ctx.crc_error_count == 0u && ctx.last_crc_ok == 1u,
                  "a type mismatch is a valid frame; it must not count as a CRC error");
}

int main(void)
{
    int failures = 0;
    printf("Running type_id tests:\n");
    failures += test_default_context_accepts_any_type();
    failures += test_init_zeroes_type_id();
    failures += test_matching_type_dispatches();
    failures += test_mismatching_type_is_rejected();
    failures += test_wildcard_type_dispatches();
    failures += test_set_reply_wildcard_honoured();
    failures += test_set_reply_wrong_type_ignored();
    failures += test_mismatch_is_not_a_crc_error();
    if (failures)
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
    printf("OK all type_id tests passed\n");
    return 0;
}
