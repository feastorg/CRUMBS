/*
 * Unit tests for the SET acknowledgement (#98): the peripheral's command
 * counter and last-command status, the reserved GET_ACK query the library
 * answers, crumbs_refuse(), and the controller helpers that read the
 * acknowledgement, classify a SET and re-send one that did not arrive.
 *
 * The controller tests run against a peripheral context in the same
 * process: a write is handed to crumbs_peripheral_handle_receive() and a
 * read to crumbs_peripheral_build_reply(), and the fake bus can lose a
 * write while reporting it written, which is what a Raspberry Pi 4 does to
 * a retried write (docs/protocol.md).
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"
#include "crumbs_message_helpers.h"
#include "crumbs_ops.h"
#include "test_common.h"

#define DEV_ADDR 0x10
#define DEV_TYPE 0x07
#define OP_APPLY 0x01
#define OP_REFUSE 0x02
#define OP_UNHANDLED 0x03
#define REFUSE_REASON 0x2A

/* ---- Peripheral fixtures ----------------------------------------------- */

static int g_refuse_rc;
static int g_applied; /* OP_APPLY frames the peripheral acted on */

#if CRUMBS_MAX_HANDLERS > 0
static void apply_handler(crumbs_context_t *ctx, uint8_t opcode,
                          const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)ctx;
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;
    g_applied++;
}

static void refuse_handler(crumbs_context_t *ctx, uint8_t opcode,
                           const uint8_t *data, uint8_t data_len, void *user_data)
{
    (void)opcode;
    (void)data;
    (void)data_len;
    (void)user_data;
    g_refuse_rc = crumbs_refuse(ctx, REFUSE_REASON);
}

static int g_ack_reply_handler_calls;

static void ack_reply_handler(crumbs_context_t *ctx, crumbs_message_t *reply, void *user_data)
{
    (void)ctx;
    (void)user_data;
    g_ack_reply_handler_calls++;
    crumbs_msg_init(reply, DEV_TYPE, CRUMBS_CMD_GET_ACK);
}
#endif

/* on_message that refuses OP_REFUSE and accepts everything else. */
static void refusing_on_message(crumbs_context_t *ctx, const crumbs_message_t *msg)
{
    if (msg->opcode == OP_REFUSE)
    {
        g_refuse_rc = crumbs_refuse(ctx, REFUSE_REASON);
    }
    else if (msg->opcode == OP_APPLY)
    {
        g_applied++;
    }
}

/* A peripheral built before GET_ACK that answers every query by echoing the
   requested opcode with a status block, as basic_peripheral's on_request
   echoes it: it passes the opcode check, and its bytes never change. */
static void echo_on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    crumbs_msg_init(reply, DEV_TYPE, ctx->requested_opcode);
    for (uint8_t i = 0; i < 6; i++)
        crumbs_msg_add_u8(reply, (uint8_t)(0x10 + i));
}

/* An old peripheral's fallback: answer every read with the version reply. */
static void version_on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    (void)ctx;
    crumbs_build_version_reply(reply, DEV_TYPE, 1, 0, 0);
}

static void init_peripheral(crumbs_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    crumbs_init(ctx, CRUMBS_ROLE_PERIPHERAL, DEV_ADDR);
    crumbs_set_type_id(ctx, DEV_TYPE);
#if CRUMBS_MAX_HANDLERS > 0
    crumbs_register_handler(ctx, OP_APPLY, apply_handler, NULL);
    crumbs_register_handler(ctx, OP_REFUSE, refuse_handler, NULL);
#else
    crumbs_set_callbacks(ctx, refusing_on_message, NULL, NULL);
#endif
}

static size_t frame_of(uint8_t type_id, uint8_t opcode, uint8_t *buf)
{
    crumbs_message_t m;
    crumbs_msg_init(&m, type_id, opcode);
    crumbs_msg_add_u8(&m, 0x55);
    return crumbs_encode_message(&m, buf, CRUMBS_MESSAGE_MAX_SIZE);
}

static int receive(crumbs_context_t *ctx, uint8_t type_id, uint8_t opcode)
{
    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t n = frame_of(type_id, opcode, buf);
    return crumbs_peripheral_handle_receive(ctx, buf, n);
}

static void stage(crumbs_context_t *ctx, uint8_t opcode)
{
    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    crumbs_message_t m;
    crumbs_msg_init(&m, CRUMBS_TYPE_ID_ANY, CRUMBS_CMD_SET_REPLY);
    crumbs_msg_add_u8(&m, opcode);
    size_t n = crumbs_encode_message(&m, buf, sizeof(buf));
    (void)crumbs_peripheral_handle_receive(ctx, buf, n);
}

/* Stage GET_ACK, build the reply the library sends, decode it. */
static int read_ack_reply(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    uint8_t out[CRUMBS_MESSAGE_MAX_SIZE];
    size_t out_len = 0;
    stage(ctx, CRUMBS_CMD_GET_ACK);
    int rc = crumbs_peripheral_build_reply(ctx, out, sizeof(out), &out_len);
    if (rc != 0)
        return rc;
    return crumbs_decode_message(out, out_len, reply, NULL);
}

#define EXPECT_ACK(name, ctx, counter_, opcode_, status_, reason_)                     \
    do                                                                                  \
    {                                                                                   \
        crumbs_message_t _r;                                                            \
        TEST_ASSERT_EQ(name, read_ack_reply(ctx, &_r), 0, "ack reply should decode");    \
        TEST_ASSERT_EQ(name, _r.type_id, DEV_TYPE, "ack reply carries declared type");  \
        TEST_ASSERT_EQ(name, _r.opcode, CRUMBS_CMD_GET_ACK, "ack reply opcode");         \
        TEST_ASSERT_EQ(name, _r.data_len, CRUMBS_ACK_PAYLOAD_LEN, "ack payload length"); \
        TEST_ASSERT_EQ(name, _r.data[0], counter_, "counter");                          \
        TEST_ASSERT_EQ(name, _r.data[1], opcode_, "last opcode");                       \
        TEST_ASSERT_EQ(name, _r.data[2], status_, "status");                            \
        TEST_ASSERT_EQ(name, _r.data[3], reason_, "reason");                            \
    } while (0)

/* ---- Peripheral tests -------------------------------------------------- */

static int test_initial_ack_is_zero(void)
{
    const char *t = "initial_ack_is_zero";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    EXPECT_ACK(t, &ctx, 0, 0x00, CRUMBS_ACK_NONE, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_init_resets_the_record(void)
{
    const char *t = "init_resets_the_record";
    crumbs_context_t ctx;
    /* crumbs_init() must clear the record itself: a context on the stack
       is not zeroed. Its handler counts are reset too, so this is safe. */
    memset(&ctx, 0xA5, sizeof(ctx));
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, DEV_ADDR);
    crumbs_set_type_id(&ctx, DEV_TYPE);
    EXPECT_ACK(t, &ctx, 0, 0x00, CRUMBS_ACK_NONE, 0);
    TEST_ASSERT_EQ(t, crumbs_refuse(&ctx, 1), -1, "not dispatching after init");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_applied_command_counts(void)
{
    const char *t = "applied_command_counts";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_APPLY), 0, "receive");
    EXPECT_ACK(t, &ctx, 1, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, receive(&ctx, CRUMBS_TYPE_ID_ANY, OP_APPLY), 0, "receive");
    EXPECT_ACK(t, &ctx, 2, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_refused_command_carries_reason(void)
{
    const char *t = "refused_command_carries_reason";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    g_refuse_rc = -99;
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_REFUSE), 0, "receive");
    TEST_ASSERT_EQ(t, g_refuse_rc, 0, "crumbs_refuse inside dispatch returns 0");
    EXPECT_ACK(t, &ctx, 1, OP_REFUSE, CRUMBS_ACK_REFUSED, REFUSE_REASON);
    /* The next applied command clears the refusal. */
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_APPLY), 0, "receive");
    EXPECT_ACK(t, &ctx, 2, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_on_message_refusal(void)
{
    const char *t = "on_message_refusal";
    crumbs_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, DEV_ADDR);
    crumbs_set_type_id(&ctx, DEV_TYPE);
    crumbs_set_callbacks(&ctx, refusing_on_message, NULL, NULL);
    g_refuse_rc = -99;
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_REFUSE), 0, "receive");
    TEST_ASSERT_EQ(t, g_refuse_rc, 0, "crumbs_refuse from on_message returns 0");
    EXPECT_ACK(t, &ctx, 1, OP_REFUSE, CRUMBS_ACK_REFUSED, REFUSE_REASON);
    /* on_message took the frame, so an opcode with no handler is applied. */
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_UNHANDLED), 0, "receive");
    EXPECT_ACK(t, &ctx, 2, OP_UNHANDLED, CRUMBS_ACK_APPLIED, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_unhandled_command_is_rejected(void)
{
    const char *t = "unhandled_command_is_rejected";
    crumbs_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    crumbs_init(&ctx, CRUMBS_ROLE_PERIPHERAL, DEV_ADDR);
    crumbs_set_type_id(&ctx, DEV_TYPE);
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_UNHANDLED), 0, "receive");
    EXPECT_ACK(t, &ctx, 1, OP_UNHANDLED, CRUMBS_ACK_REJECTED, CRUMBS_ACK_REJECT_NO_HANDLER);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_wrong_type_is_rejected_and_counted(void)
{
    const char *t = "wrong_type_is_rejected_and_counted";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    TEST_ASSERT_EQ(t, receive(&ctx, 0x09, OP_APPLY), CRUMBS_RX_TYPE_MISMATCH, "type mismatch");
    EXPECT_ACK(t, &ctx, 1, OP_APPLY, CRUMBS_ACK_REJECTED, CRUMBS_ACK_REJECT_TYPE);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_set_reply_and_bad_frames_do_not_count(void)
{
    const char *t = "set_reply_and_bad_frames_do_not_count";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_APPLY), 0, "receive");

    /* SET_REPLY, including one for another type, is never counted. */
    stage(&ctx, 0x80);
    uint8_t buf[2 * CRUMBS_MESSAGE_MAX_SIZE];
    size_t n = frame_of(0x09, CRUMBS_CMD_SET_REPLY, buf);
    (void)crumbs_peripheral_handle_receive(&ctx, buf, n);

    /* A corrupt frame is lost, not delivered. */
    n = frame_of(DEV_TYPE, OP_REFUSE, buf);
    buf[n - 1] ^= 0x01;
    TEST_ASSERT_EQ(t, crumbs_peripheral_handle_receive(&ctx, buf, n), -2, "crc error");

    /* A retried write merged into the failed one: two frames in one buffer. */
    n = frame_of(DEV_TYPE, OP_REFUSE, buf);
    size_t n2 = frame_of(DEV_TYPE, OP_REFUSE, buf + n);
    TEST_ASSERT_EQ(t, crumbs_peripheral_handle_receive(&ctx, buf, n + n2), -1, "merged frame");

    EXPECT_ACK(t, &ctx, 1, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_refuse_outside_dispatch_is_refused(void)
{
    const char *t = "refuse_outside_dispatch";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_APPLY), 0, "receive");
    TEST_ASSERT_EQ(t, crumbs_refuse(&ctx, 0x01), -1, "refuse after dispatch");
    TEST_ASSERT_EQ(t, crumbs_refuse(NULL, 0x01), -1, "refuse NULL ctx");
    EXPECT_ACK(t, &ctx, 1, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_counter_wraps(void)
{
    const char *t = "counter_wraps";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    for (int i = 0; i < 256; i++)
    {
        TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_APPLY), 0, "receive");
    }
    EXPECT_ACK(t, &ctx, 0, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, receive(&ctx, DEV_TYPE, OP_APPLY), 0, "receive");
    EXPECT_ACK(t, &ctx, 1, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_ack_query_is_answered_by_the_library(void)
{
    const char *t = "ack_query_is_answered_by_the_library";
    crumbs_context_t ctx;
    init_peripheral(&ctx);
    crumbs_set_callbacks(&ctx, NULL, version_on_request, NULL);
#if CRUMBS_MAX_HANDLERS > 0
    g_ack_reply_handler_calls = 0;
    crumbs_register_reply_handler(&ctx, CRUMBS_CMD_GET_ACK, ack_reply_handler, NULL);
#endif
    EXPECT_ACK(t, &ctx, 0, 0x00, CRUMBS_ACK_NONE, 0);
#if CRUMBS_MAX_HANDLERS > 0
    TEST_ASSERT_EQ(t, g_ack_reply_handler_calls, 0, "a reply handler for 0xFD is never called");
#endif
    /* An undeclared peripheral answers with the wildcard type. */
    crumbs_context_t bare;
    memset(&bare, 0, sizeof(bare));
    crumbs_init(&bare, CRUMBS_ROLE_PERIPHERAL, DEV_ADDR);
    crumbs_message_t r;
    TEST_ASSERT_EQ(t, read_ack_reply(&bare, &r), 0, "ack reply decodes");
    TEST_ASSERT_EQ(t, r.type_id, CRUMBS_TYPE_ID_ANY, "undeclared type");
    TEST_ASSERT_EQ(t, r.opcode, CRUMBS_CMD_GET_ACK, "opcode");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- Decode and classify ----------------------------------------------- */

static int test_ack_decode(void)
{
    const char *t = "ack_decode";
    crumbs_message_t m;
    crumbs_ack_t a;
    crumbs_msg_init(&m, DEV_TYPE, CRUMBS_CMD_GET_ACK);
    crumbs_msg_add_u8(&m, 9);
    crumbs_msg_add_u8(&m, OP_REFUSE);
    crumbs_msg_add_u8(&m, CRUMBS_ACK_REFUSED);
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), CRUMBS_RX_REPLY_MISMATCH,
                   "three bytes are not an acknowledgement");
    crumbs_msg_add_u8(&m, REFUSE_REASON);
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), 0, "four bytes decode");
    TEST_ASSERT_EQ(t, a.counter, 9, "counter");
    TEST_ASSERT_EQ(t, a.opcode, OP_REFUSE, "opcode");
    TEST_ASSERT_EQ(t, a.status, CRUMBS_ACK_REFUSED, "status");
    TEST_ASSERT_EQ(t, a.reason, REFUSE_REASON, "reason");
    crumbs_msg_add_u8(&m, 0xEE);
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), 0, "trailing bytes are an extension");

    /* What an echoing old peripheral sends: the right opcode, a payload
       whose status byte is none of the four. */
    crumbs_msg_init(&m, DEV_TYPE, CRUMBS_CMD_GET_ACK);
    crumbs_msg_add_u8(&m, 0x10);
    crumbs_msg_add_u8(&m, 0x11);
    crumbs_msg_add_u8(&m, 0x12);
    crumbs_msg_add_u8(&m, 0x13);
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), CRUMBS_RX_REPLY_MISMATCH, "unknown status");
    /* Status none is only ever all four bytes zero. */
    m.data[2] = CRUMBS_ACK_NONE;
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), CRUMBS_RX_REPLY_MISMATCH, "none with a record");
    m.data[0] = 0;
    m.data[1] = 0;
    m.data[3] = 0;
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), 0, "none, all zero");

    m.opcode = 0x00;
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, &a), CRUMBS_RX_REPLY_MISMATCH, "another opcode");
    TEST_ASSERT_EQ(t, crumbs_ack_decode(NULL, &a), -1, "NULL reply");
    TEST_ASSERT_EQ(t, crumbs_ack_decode(&m, NULL), -1, "NULL out");
    printf("  %s: PASS\n", t);
    return 0;
}

static crumbs_ack_t ack(uint8_t counter, uint8_t opcode, uint8_t status, uint8_t reason)
{
    crumbs_ack_t a;
    a.counter = counter;
    a.opcode = opcode;
    a.status = status;
    a.reason = reason;
    return a;
}

static int test_ack_classify(void)
{
    const char *t = "ack_classify";
    crumbs_ack_t before = ack(5, 0x00, CRUMBS_ACK_APPLIED, 0);
    crumbs_ack_t after;

    after = before;
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_SET_NOT_ADVANCED,
                   "same counter");
    after = ack(6, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_SET_APPLIED, "applied");
    after = ack(6, OP_APPLY, CRUMBS_ACK_REFUSED, 3);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_SET_REFUSED, "refused");
    after = ack(6, OP_APPLY, CRUMBS_ACK_REJECTED, CRUMBS_ACK_REJECT_NO_HANDLER);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_SET_REJECTED,
                   "rejected");
    after = ack(6, OP_REFUSE, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_RX_ACK_MISMATCH,
                   "advanced on another opcode");
    after = ack(6, OP_APPLY, CRUMBS_ACK_NONE, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), -1, "no status");
    after = ack(6, OP_APPLY, 0x7F, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), -1, "unknown status");
    before = ack(255, 0x00, CRUMBS_ACK_APPLIED, 0);
    after = ack(0, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_SET_APPLIED,
                   "advanced across the wrap");

    /* A peripheral that restarted reads all zero with status none. That is
       the fresh baseline too, and what a wrapped counter can come back to:
       never "not advanced". */
    crumbs_ack_t fresh = ack(0, 0x00, CRUMBS_ACK_NONE, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&fresh, &fresh, OP_APPLY), CRUMBS_RX_ACK_UNKNOWN,
                   "fresh baseline, reset or lost");
    before = ack(0, OP_REFUSE, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &fresh, OP_APPLY), CRUMBS_RX_ACK_UNKNOWN,
                   "wrapped baseline, then reset");
    before = ack(5, OP_REFUSE, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &fresh, OP_APPLY), CRUMBS_RX_ACK_UNKNOWN,
                   "reset from counter 5");
    after = ack(5, OP_APPLY, CRUMBS_ACK_APPLIED, 0);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), CRUMBS_RX_ACK_UNKNOWN,
                   "same counter, another record");
    after = ack(0, 0x00, CRUMBS_ACK_NONE, 4);
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, &after, OP_APPLY), -1, "none with a reason");

    TEST_ASSERT_EQ(t, crumbs_ack_classify(NULL, &after, OP_APPLY), -1, "NULL before");
    TEST_ASSERT_EQ(t, crumbs_ack_classify(&before, NULL, OP_APPLY), -1, "NULL after");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- Controller over a fake bus ---------------------------------------- */

typedef struct
{
    crumbs_context_t *periph;
    int drop_commands;   /* lose the next N command writes, reporting success */
    int drop_stages;     /* lose the next N SET_REPLY writes, reporting success */
    int fail_reads;      /* fail the next N reads */
    uint32_t fail_bits;  /* fail read i (0-based) when bit i is set */
    int read_index;
    int command_rc;      /* what a delivered command write returns */
    int reset_after;     /* restart the peripheral after this many command writes */
    int commands;        /* command writes delivered */
    int old_echo;        /* answer 0xFD as a pre-GET_ACK echoing peripheral */
    int echo_record;     /* ... with [5, 0x10, 1, 0], which is a valid record */
    int version;         /* if non-zero, answer opcode 0x00 with this CRUMBS_VERSION */
    int inject_opcode;   /* if non-zero, deliver this frame after each command */
    int delays;
    char log[128];       /* S: SET_REPLY write, C: command write, R: read */
    size_t log_len;
} bus_t;

static void bus_log(bus_t *b, char c)
{
    if (b->log_len + 1 < sizeof(b->log))
    {
        b->log[b->log_len++] = c;
        b->log[b->log_len] = '\0';
    }
}

static int bus_write(void *user, uint8_t addr, const uint8_t *data, size_t len)
{
    bus_t *b = (bus_t *)user;
    (void)addr;
    int is_stage = len >= 2 && data[1] == CRUMBS_CMD_SET_REPLY;
    bus_log(b, is_stage ? 'S' : 'C');
    if (is_stage && b->drop_stages > 0)
    {
        b->drop_stages--;
        return 0;
    }
    if (!is_stage && b->drop_commands > 0)
    {
        b->drop_commands--;
        return 0;
    }
    (void)crumbs_peripheral_handle_receive(b->periph, data, len);
    if (!is_stage)
    {
        b->commands++;
        if (b->inject_opcode)
            (void)receive(b->periph, DEV_TYPE, (uint8_t)b->inject_opcode);
        if (b->reset_after && b->commands == b->reset_after)
            init_peripheral(b->periph); /* brown-out: firmware starts over */
    }
    return is_stage ? 0 : b->command_rc;
}

static int bus_read(void *user, uint8_t addr, uint8_t *buf, size_t len, uint32_t timeout_us)
{
    bus_t *b = (bus_t *)user;
    (void)addr;
    (void)timeout_us;
    bus_log(b, 'R');
    int index = b->read_index++;
    if (index < 32 && (b->fail_bits & (1u << index)))
    {
        return -1;
    }
    if (b->fail_reads > 0)
    {
        b->fail_reads--;
        return -1;
    }
    size_t out_len = 0;
    memset(buf, 0xFF, len);
    if (b->version && b->periph->requested_opcode == 0x00)
    {
        /* The version reply (docs/protocol.md, opcode 0x00), with the
           library version this test says the peripheral was built with. */
        crumbs_message_t r;
        crumbs_msg_init(&r, DEV_TYPE, 0x00);
        crumbs_msg_add_u16(&r, (uint16_t)b->version);
        crumbs_msg_add_u8(&r, 1);
        crumbs_msg_add_u8(&r, 0);
        crumbs_msg_add_u8(&r, 0);
        (void)crumbs_encode_message(&r, buf, len);
        return (int)len;
    }
    if (b->old_echo && b->periph->requested_opcode == CRUMBS_CMD_GET_ACK)
    {
        /* An old library does not intercept 0xFD; on_request answers. */
        crumbs_message_t r;
        if (b->echo_record)
        {
            crumbs_msg_init(&r, DEV_TYPE, CRUMBS_CMD_GET_ACK);
            crumbs_msg_add_u8(&r, 5);
            crumbs_msg_add_u8(&r, 0x10);
            crumbs_msg_add_u8(&r, CRUMBS_ACK_APPLIED);
            crumbs_msg_add_u8(&r, 0);
        }
        else
        {
            echo_on_request(b->periph, &r);
        }
        (void)crumbs_encode_message(&r, buf, len);
        return (int)len;
    }
    if (crumbs_peripheral_build_reply(b->periph, buf, len, &out_len) != 0)
        return -1;
    if (out_len == 0)
        buf[0] = 0x00; /* AVR Wire with nothing to send */
    return (int)len;
}

static bus_t *g_delay_bus;

static void bus_delay(uint32_t us)
{
    (void)us;
    if (g_delay_bus)
        g_delay_bus->delays++;
}

static void setup(crumbs_context_t *periph, crumbs_context_t *ctrl, bus_t *bus, crumbs_device_t *dev)
{
    init_peripheral(periph);
    memset(ctrl, 0, sizeof(*ctrl));
    crumbs_init(ctrl, CRUMBS_ROLE_CONTROLLER, 0);
    memset(bus, 0, sizeof(*bus));
    bus->periph = periph;
    bus->version = CRUMBS_ACK_MIN_VERSION;
    g_delay_bus = bus;
    dev->ctx = ctrl;
    dev->addr = DEV_ADDR;
    dev->write_fn = bus_write;
    dev->read_fn = bus_read;
    dev->delay_fn = bus_delay;
    dev->io = bus;
}

static crumbs_message_t command(uint8_t opcode)
{
    crumbs_message_t m;
    crumbs_msg_init(&m, DEV_TYPE, opcode);
    crumbs_msg_add_u8(&m, 0x55);
    return m;
}

static int test_read_ack(void)
{
    const char *t = "read_ack";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    TEST_ASSERT_EQ(t, receive(&periph, DEV_TYPE, OP_APPLY), 0, "receive");
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(&dev, &a), 0, "read_ack");
    TEST_ASSERT(t, strcmp(bus.log, "SR") == 0, "one stage and one read");
    TEST_ASSERT_EQ(t, bus.delays, 1, "one query delay");
    TEST_ASSERT_EQ(t, a.counter, 1, "counter");
    TEST_ASSERT_EQ(t, a.opcode, OP_APPLY, "opcode");
    TEST_ASSERT_EQ(t, a.status, CRUMBS_ACK_APPLIED, "status");

    /* A lost SET_REPLY leaves another opcode staged; the read is retried. */
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_set_callbacks(&periph, NULL, version_on_request, NULL);
    bus.drop_stages = 1;
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(&dev, &a), 0, "read_ack after a lost stage");
    TEST_ASSERT(t, strcmp(bus.log, "SRSR") == 0, "staged again");

    /* A failed read is read again. */
    setup(&periph, &ctrl, &bus, &dev);
    bus.fail_reads = 2;
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(&dev, &a), 0, "read_ack after two failed reads");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRSR") == 0, "three attempts");

    bus.log_len = 0;
    bus.fail_reads = CRUMBS_ACK_READ_ATTEMPTS;
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(&dev, &a), -1, "gives up after its attempts");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRSR") == 0, "CRUMBS_ACK_READ_ATTEMPTS attempts");

    dev.delay_fn = NULL;
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(&dev, &a), -1, "needs delay_fn");
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(NULL, &a), -1, "NULL dev");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_read_ack_detects_an_old_peripheral(void)
{
    const char *t = "read_ack_detects_an_old_peripheral";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_set_callbacks(&periph, NULL, version_on_request, NULL);

    /* A peripheral built before GET_ACK answers a read with whatever its
       on_request builds, here the version reply. Dropping every SET_REPLY
       keeps this one on opcode 0x00, which puts the same frame on the bus. */
    bus.drop_stages = 100;
    TEST_ASSERT_EQ(t, crumbs_controller_read_ack(&dev, &a), CRUMBS_RX_REPLY_MISMATCH,
                   "another opcode's reply on every attempt");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRSR") == 0, "three attempts");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_applied(void)
{
    const char *t = "send_confirmed_applied";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_SET_APPLIED,
                   "applied");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "version, baseline, one send, one ack read");
    TEST_ASSERT_EQ(t, a.counter, 1, "out_ack counter");
    TEST_ASSERT_EQ(t, a.opcode, OP_APPLY, "out_ack opcode");
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 1, NULL), CRUMBS_SET_APPLIED,
                   "out_ack may be NULL");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_resends_a_lost_frame(void)
{
    const char *t = "send_confirmed_resends_a_lost_frame";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    int rc;
    crumbs_message_t m = command(OP_APPLY);

    /* A peripheral that has counted a command already: "not advanced" is
       then distinguishable from a restart. */
    setup(&periph, &ctrl, &bus, &dev);
    (void)receive(&periph, DEV_TYPE, OP_REFUSE);
    bus.drop_commands = 1;
    rc = crumbs_controller_send_confirmed(&dev, &m, 3, &a);
    TEST_ASSERT_EQ(t, rc, CRUMBS_SET_APPLIED, "applied on the second send");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSRCSR") == 0, "re-sent once");
    TEST_ASSERT_EQ(t, a.counter, 2, "applied once");

    setup(&periph, &ctrl, &bus, &dev);
    (void)receive(&periph, DEV_TYPE, OP_REFUSE);
    bus.drop_commands = 100;
    rc = crumbs_controller_send_confirmed(&dev, &m, 2, &a);
    TEST_ASSERT_EQ(t, rc, CRUMBS_SET_NOT_ADVANCED, "not advanced after the bound");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSRCSR") == 0, "two sends in all");
    TEST_ASSERT_EQ(t, a.counter, 1, "out_ack is the last read");

    /* A peripheral that has counted nothing: a lost first frame reads the
       same as one applied before a restart, so it is not re-sent. */
    setup(&periph, &ctrl, &bus, &dev);
    bus.drop_commands = 1;
    rc = crumbs_controller_send_confirmed(&dev, &m, 3, &a);
    TEST_ASSERT_EQ(t, rc, CRUMBS_RX_ACK_UNKNOWN, "fresh peripheral: unknown");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "not re-sent");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_refused_is_not_resent(void)
{
    const char *t = "send_confirmed_refused_is_not_resent";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_message_t m = command(OP_REFUSE);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_SET_REFUSED,
                   "refused");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "one send");
    TEST_ASSERT_EQ(t, a.reason, REFUSE_REASON, "reason reaches the caller");

    m = command(OP_UNHANDLED);
    crumbs_context_t bare;
    setup(&periph, &ctrl, &bus, &dev);
    memset(&bare, 0, sizeof(bare));
    crumbs_init(&bare, CRUMBS_ROLE_PERIPHERAL, DEV_ADDR);
    bus.periph = &bare;
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_SET_REJECTED,
                   "rejected");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "one send");
    TEST_ASSERT_EQ(t, a.reason, CRUMBS_ACK_REJECT_NO_HANDLER, "rejection reason");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_needs_a_baseline(void)
{
    const char *t = "send_confirmed_needs_a_baseline";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_message_t m = command(OP_APPLY);
    bus.fail_bits = 0xEu; /* every baseline attempt; read 0 is the version */
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), -1,
                   "the baseline read's error");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRSRSR") == 0, "nothing sent without a baseline");
    TEST_ASSERT_EQ(t, periph.ack_counter, 0, "nothing applied");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_failed_ack_read_is_not_a_resend(void)
{
    const char *t = "send_confirmed_failed_ack_read_is_not_a_resend";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    crumbs_message_t m = command(OP_APPLY);

    /* Read 0 is the version reply, read 1 the baseline. Two failed reads
       after the send, then a good one: the acknowledgement is read again
       and the frame goes out once. */
    setup(&periph, &ctrl, &bus, &dev);
    bus.fail_bits = 0xCu;
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_SET_APPLIED,
                   "applied");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSRSRSR") == 0, "re-read, not re-sent");
    TEST_ASSERT_EQ(t, periph.ack_counter, 1, "applied once");

    /* Every read after the send fails: the read's error, still one send. */
    setup(&periph, &ctrl, &bus, &dev);
    bus.fail_bits = 0x1Cu;
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), -1,
                   "the ack read's error");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSRSRSR") == 0, "sent once");
    TEST_ASSERT_EQ(t, periph.ack_counter, 1, "applied once");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_write_error_still_reads_the_ack(void)
{
    const char *t = "send_confirmed_write_error_still_reads_the_ack";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    bus.command_rc = -5; /* the frame landed, the controller saw an error */
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_SET_APPLIED,
                   "applied despite the write error");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "not re-sent");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_old_peripheral_is_unsupported(void)
{
    const char *t = "send_confirmed_old_peripheral_is_unsupported";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_set_callbacks(&periph, NULL, version_on_request, NULL);
    bus.version = 0;       /* version_on_request answers, with this library's */
    bus.drop_stages = 100; /* see read_ack_detects_an_old_peripheral */
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_RX_REPLY_MISMATCH,
                   "unsupported");
    TEST_ASSERT(t, strchr(bus.log, 'C') == NULL, "nothing sent");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_arguments(void)
{
    const char *t = "send_confirmed_arguments";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 0, &a), -1, "max_sends 0");
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, NULL, 3, &a), -1, "NULL msg");
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(NULL, &m, 3, &a), -1, "NULL dev");
    m.opcode = CRUMBS_CMD_SET_REPLY;
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), -1, "SET_REPLY");
    TEST_ASSERT_EQ(t, (int)bus.log_len, 0, "nothing on the bus");
    printf("  %s: PASS\n", t);
    return 0;
}

/* The peripheral applies the SET, then restarts before the acknowledgement
   is read (a brown-out from the load it switched on). Re-sending would
   apply it twice. */
static int test_send_confirmed_reset_after_apply(void)
{
    const char *t = "send_confirmed_reset_after_apply";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    bus.reset_after = 1;
    g_applied = 0;
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_RX_ACK_UNKNOWN,
                   "unknown, from a zero baseline");
    TEST_ASSERT_EQ(t, g_applied, 1, "applied once");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "not re-sent");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_reset_at_the_wrapped_counter(void)
{
    const char *t = "send_confirmed_reset_at_the_wrapped_counter";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    for (int i = 0; i < 256; i++)
        (void)receive(&periph, DEV_TYPE, OP_REFUSE); /* counter back at 0 */
    bus.reset_after = 1;
    g_applied = 0;
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_RX_ACK_UNKNOWN,
                   "unknown, from a wrapped baseline");
    TEST_ASSERT_EQ(t, g_applied, 1, "applied once");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "not re-sent");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_echoing_old_peripheral(void)
{
    const char *t = "send_confirmed_echoing_old_peripheral";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    /* Its version reply passes here: the record check alone rejects it. */
    setup(&periph, &ctrl, &bus, &dev);
    bus.old_echo = 1;
    g_applied = 0;
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_RX_REPLY_MISMATCH,
                   "no acknowledgement");
    TEST_ASSERT_EQ(t, g_applied, 0, "nothing applied");
    TEST_ASSERT(t, strchr(bus.log, 'C') == NULL, "nothing sent");
    printf("  %s: PASS\n", t);
    return 0;
}

/* An older peripheral whose echo of 0xFD happens to form a valid record
   reads as an acknowledgement whose counter never moves. Its version reply
   is what tells it apart, and it is sent nothing. */
static int test_send_confirmed_checks_the_version(void)
{
    const char *t = "send_confirmed_checks_the_version";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    crumbs_message_t m = command(OP_APPLY);

    setup(&periph, &ctrl, &bus, &dev);
    bus.old_echo = 1;
    bus.echo_record = 1;
    bus.version = CRUMBS_ACK_MIN_VERSION - 100;
    g_applied = 0;
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_RX_REPLY_MISMATCH,
                   "below CRUMBS_ACK_MIN_VERSION");
    TEST_ASSERT_EQ(t, g_applied, 0, "nothing applied");
    TEST_ASSERT(t, strcmp(bus.log, "SR") == 0, "the version read, and nothing sent");

    /* No version reply at all (nothing staged for opcode 0x00). */
    setup(&periph, &ctrl, &bus, &dev);
    bus.version = 0;
    g_applied = 0;
    TEST_ASSERT(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a) < 0, "no version reply");
    TEST_ASSERT(t, strchr(bus.log, 'C') == NULL, "nothing sent");
    TEST_ASSERT_EQ(t, g_applied, 0, "nothing applied");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_send_confirmed_another_command_counted(void)
{
    const char *t = "send_confirmed_another_command_counted";
    crumbs_context_t periph, ctrl;
    bus_t bus;
    crumbs_device_t dev;
    crumbs_ack_t a;
    setup(&periph, &ctrl, &bus, &dev);
    bus.inject_opcode = OP_REFUSE; /* another frame lands after ours */
    g_applied = 0;
    crumbs_message_t m = command(OP_APPLY);
    TEST_ASSERT_EQ(t, crumbs_controller_send_confirmed(&dev, &m, 3, &a), CRUMBS_RX_ACK_MISMATCH,
                   "advanced on another opcode");
    TEST_ASSERT_EQ(t, g_applied, 1, "applied once");
    TEST_ASSERT(t, strcmp(bus.log, "SRSRCSR") == 0, "not re-sent");
    TEST_ASSERT_EQ(t, a.opcode, OP_REFUSE, "out_ack names the other command");
    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("test_ack:\n");
    failures += test_initial_ack_is_zero();
    failures += test_init_resets_the_record();
    failures += test_applied_command_counts();
    failures += test_refused_command_carries_reason();
    failures += test_on_message_refusal();
    failures += test_unhandled_command_is_rejected();
    failures += test_wrong_type_is_rejected_and_counted();
    failures += test_set_reply_and_bad_frames_do_not_count();
    failures += test_refuse_outside_dispatch_is_refused();
    failures += test_counter_wraps();
    failures += test_ack_query_is_answered_by_the_library();
    failures += test_ack_decode();
    failures += test_ack_classify();
    failures += test_read_ack();
    failures += test_read_ack_detects_an_old_peripheral();
    failures += test_send_confirmed_applied();
    failures += test_send_confirmed_resends_a_lost_frame();
    failures += test_send_confirmed_refused_is_not_resent();
    failures += test_send_confirmed_needs_a_baseline();
    failures += test_send_confirmed_failed_ack_read_is_not_a_resend();
    failures += test_send_confirmed_write_error_still_reads_the_ack();
    failures += test_send_confirmed_old_peripheral_is_unsupported();
    failures += test_send_confirmed_arguments();
    failures += test_send_confirmed_reset_after_apply();
    failures += test_send_confirmed_reset_at_the_wrapped_counter();
    failures += test_send_confirmed_echoing_old_peripheral();
    failures += test_send_confirmed_another_command_counted();
    failures += test_send_confirmed_checks_the_version();
    if (failures)
    {
        fprintf(stderr, "test_ack: %d failed\n", failures);
        return 1;
    }
    printf("test_ack: all passed\n");
    return 0;
}
