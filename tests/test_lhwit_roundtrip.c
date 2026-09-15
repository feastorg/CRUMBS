/* Every lhwit operation round-trips through the family's own codec: each
 * SET goes out through its controller wrapper, is captured off the fake
 * bus, decoded and unpacked with the unpack the peripheral uses; each GET
 * reply is packed with the pack the peripheral uses, served on the fake
 * bus and read back through the controller getter. A layout that the two
 * sides disagreed on could not survive either direction. */
#include <stdio.h>
#include <string.h>

#include "calculator_ops.h"
#include "display_ops.h"
#include "led_ops.h"
#include "servo_ops.h"
#include "test_common.h"

/* ---- fake bus ---------------------------------------------------------- */

typedef struct
{
    crumbs_message_t last_write; /* decoded from the last frame written */
    int write_rc;
    uint8_t reply[CRUMBS_MESSAGE_MAX_SIZE]; /* served, padded, on read */
    size_t reply_len;
} bus_t;

static int bus_write(void *io, uint8_t addr, const uint8_t *data, size_t len)
{
    bus_t *b = (bus_t *)io;
    (void)addr;
    b->write_rc = crumbs_decode_message(data, len, &b->last_write, NULL);
    return 0;
}

static int bus_read(void *io, uint8_t addr, uint8_t *buf, size_t len, uint32_t timeout_us)
{
    bus_t *b = (bus_t *)io;
    (void)addr;
    (void)timeout_us;
    memset(buf, 0xFF, len);
    memcpy(buf, b->reply, b->reply_len < len ? b->reply_len : len);
    return (int)len;
}

static void bus_delay(uint32_t us) { (void)us; }

/* The peripheral's side of a GET: build the reply the way its reply handler does. */
static void serve(bus_t *b, const crumbs_message_t *reply)
{
    b->reply_len = crumbs_encode_message(reply, b->reply, sizeof b->reply);
}

static crumbs_context_t g_ctx;
static bus_t g_bus;
static crumbs_device_t g_dev;

static void bind(void)
{
    test_init_controller(&g_ctx);
    memset(&g_bus, 0, sizeof g_bus);
    g_dev.ctx = &g_ctx;
    g_dev.addr = 0x10;
    g_dev.write_fn = bus_write;
    g_dev.read_fn = bus_read;
    g_dev.delay_fn = bus_delay;
    g_dev.io = &g_bus;
}

#define SENT(t, type, op) \
    do \
    { \
        TEST_ASSERT_EQ(t, g_bus.write_rc, 0, "frame on the bus decodes"); \
        TEST_ASSERT_EQ(t, g_bus.last_write.type_id, type, "type on the wire"); \
        TEST_ASSERT_EQ(t, g_bus.last_write.opcode, op, "opcode on the wire"); \
    } while (0)

/* ---- LED ----------------------------------------------------------------- */

static int test_led(void)
{
    const char *t = "led";
    crumbs_message_t reply;
    led_set_all_t all = {0x0A}, all_rx;
    led_set_one_t one = {3, 1}, one_rx;
    led_blink_t blink = {2, 1, 750}, blink_rx;
    led_state_result_t state = {0x05}, state_rx;
    led_blink_result_t bl = {{1, 0, 1, 0}, {100, 200, 300, 65535}}, bl_rx;
    int i;

    bind();
    TEST_ASSERT_EQ(t, led_send_set_all(&g_dev, &all), 0, "send set_all");
    SENT(t, LED_TYPE_ID, LED_OP_SET_ALL);
    TEST_ASSERT_EQ(t, led_set_all_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &all_rx), 0, "unpack set_all");
    TEST_ASSERT_EQ(t, all_rx.mask, 0x0A, "mask");

    TEST_ASSERT_EQ(t, led_send_set_one(&g_dev, &one), 0, "send set_one");
    SENT(t, LED_TYPE_ID, LED_OP_SET_ONE);
    TEST_ASSERT_EQ(t, led_set_one_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &one_rx), 0, "unpack set_one");
    TEST_ASSERT(t, one_rx.led_idx == 3 && one_rx.state == 1, "set_one fields");

    TEST_ASSERT_EQ(t, led_send_blink(&g_dev, &blink), 0, "send blink");
    SENT(t, LED_TYPE_ID, LED_OP_BLINK);
    TEST_ASSERT_EQ(t, g_bus.last_write.data_len, 4, "blink is 4 bytes on the wire");
    TEST_ASSERT(t, memcmp(g_bus.last_write.data, "\x02\x01\xEE\x02", 4) == 0,
                "blink bytes: [led_idx][enable][period_ms LE], as the layout comment and old firmware say");
    TEST_ASSERT_EQ(t, led_blink_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &blink_rx), 0, "unpack blink");
    TEST_ASSERT(t, blink_rx.led_idx == 2 && blink_rx.enable == 1 && blink_rx.period_ms == 750, "blink fields");

    crumbs_msg_init(&reply, LED_TYPE_ID, LED_OP_GET_STATE);
    TEST_ASSERT_EQ(t, led_state_result_pack(&reply, &state), 0, "pack state");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, led_get_state(&g_dev, &state_rx), 0, "get state");
    SENT(t, 0, CRUMBS_CMD_SET_REPLY);
    TEST_ASSERT_EQ(t, g_bus.last_write.data[0], LED_OP_GET_STATE, "the query selected GET_STATE");
    TEST_ASSERT_EQ(t, state_rx.states, 0x05, "states");

    crumbs_msg_init(&reply, LED_TYPE_ID, LED_OP_GET_BLINK);
    TEST_ASSERT_EQ(t, led_blink_result_pack(&reply, &bl), 0, "pack blink result");
    TEST_ASSERT_EQ(t, reply.data_len, 12, "blink result is 12 bytes");
    TEST_ASSERT(t, memcmp(reply.data, "\x01\x64\x00\x00\xC8\x00\x01\x2C\x01\x00\xFF\xFF", 12) == 0,
                "blink result bytes: [enable][period LE] x 4");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, led_get_blink(&g_dev, &bl_rx), 0, "get blink");
    for (i = 0; i < 4; i++)
    {
        TEST_ASSERT_EQ(t, bl_rx.enable[i], bl.enable[i], "blink enable");
        TEST_ASSERT_EQ(t, bl_rx.period_ms[i], bl.period_ms[i], "blink period");
    }
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- Servo --------------------------------------------------------------- */

static int test_servo(void)
{
    const char *t = "servo";
    crumbs_message_t reply;
    servo_set_pos_t pos = {1, 180}, pos_rx;
    servo_set_speed_t spd = {0, 20}, spd_rx;
    servo_sweep_t sw = {1, 1, 10, 170, 5}, sw_rx;
    servo_pos_result_t pr = {45, 135}, pr_rx;
    servo_speed_result_t sr = {0, 7}, sr_rx;

    bind();
    TEST_ASSERT_EQ(t, servo_send_set_pos(&g_dev, &pos), 0, "send set_pos");
    SENT(t, SERVO_TYPE_ID, SERVO_OP_SET_POS);
    TEST_ASSERT_EQ(t, servo_set_pos_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &pos_rx), 0, "unpack");
    TEST_ASSERT(t, pos_rx.servo_idx == 1 && pos_rx.position == 180, "set_pos fields");

    TEST_ASSERT_EQ(t, servo_send_set_speed(&g_dev, &spd), 0, "send set_speed");
    SENT(t, SERVO_TYPE_ID, SERVO_OP_SET_SPEED);
    TEST_ASSERT_EQ(t, servo_set_speed_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &spd_rx), 0, "unpack");
    TEST_ASSERT(t, spd_rx.servo_idx == 0 && spd_rx.speed == 20, "set_speed fields");

    TEST_ASSERT_EQ(t, servo_send_sweep(&g_dev, &sw), 0, "send sweep");
    SENT(t, SERVO_TYPE_ID, SERVO_OP_SWEEP);
    TEST_ASSERT_EQ(t, g_bus.last_write.data_len, 5, "sweep is 5 bytes on the wire");
    TEST_ASSERT_EQ(t, servo_sweep_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &sw_rx), 0, "unpack");
    TEST_ASSERT(t, sw_rx.servo_idx == 1 && sw_rx.enable == 1 && sw_rx.min_pos == 10 && sw_rx.max_pos == 170 && sw_rx.step == 5,
                "sweep fields");

    crumbs_msg_init(&reply, SERVO_TYPE_ID, SERVO_OP_GET_POS);
    TEST_ASSERT_EQ(t, servo_pos_result_pack(&reply, &pr), 0, "pack pos");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, servo_get_pos(&g_dev, &pr_rx), 0, "get pos");
    TEST_ASSERT(t, pr_rx.pos0 == 45 && pr_rx.pos1 == 135, "positions");

    crumbs_msg_init(&reply, SERVO_TYPE_ID, SERVO_OP_GET_SPEED);
    TEST_ASSERT_EQ(t, servo_speed_result_pack(&reply, &sr), 0, "pack speed");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, servo_get_speed(&g_dev, &sr_rx), 0, "get speed");
    TEST_ASSERT(t, sr_rx.speed0 == 0 && sr_rx.speed1 == 7, "speeds");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- Calculator ---------------------------------------------------------- */

static int test_calculator(void)
{
    const char *t = "calculator";
    crumbs_message_t reply;
    calc_operands_t ops = {0xDEADBEEFu, 0x00C0FFEEu}, ops_rx;
    calc_result_t res = {0xDF7EBEDDu}, res_rx;
    calc_hist_meta_t meta = {12, 3}, meta_rx;
    calc_hist_entry_t ent = {"MUL", 6, 7, 42}, ent_rx;

    bind();
    TEST_ASSERT_EQ(t, calc_send_add(&g_dev, &ops), 0, "send add");
    SENT(t, CALC_TYPE_ID, CALC_OP_ADD);
    TEST_ASSERT_EQ(t, g_bus.last_write.data_len, 8, "operands are 8 bytes on the wire");
    TEST_ASSERT_EQ(t, calc_operands_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &ops_rx), 0, "unpack");
    TEST_ASSERT(t, ops_rx.a == 0xDEADBEEFu && ops_rx.b == 0x00C0FFEEu, "operands");
    TEST_ASSERT_EQ(t, calc_send_div(&g_dev, &ops), 0, "send div");
    SENT(t, CALC_TYPE_ID, CALC_OP_DIV);

    crumbs_msg_init(&reply, CALC_TYPE_ID, CALC_OP_GET_RESULT);
    TEST_ASSERT_EQ(t, calc_result_pack(&reply, &res), 0, "pack result");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, calc_get_result(&g_dev, &res_rx), 0, "get result");
    TEST_ASSERT(t, res_rx.result == 0xDF7EBEDDu, "result");

    crumbs_msg_init(&reply, CALC_TYPE_ID, CALC_OP_GET_HIST_META);
    TEST_ASSERT_EQ(t, calc_hist_meta_pack(&reply, &meta), 0, "pack meta");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, calc_get_hist_meta(&g_dev, &meta_rx), 0, "get meta");
    TEST_ASSERT(t, meta_rx.count == 12 && meta_rx.write_pos == 3, "meta");

    /* The hand-written entry codec: 16 bytes, op NUL-padded on the wire. */
    crumbs_msg_init(&reply, CALC_TYPE_ID, CALC_OP_GET_HIST_5);
    TEST_ASSERT_EQ(t, calc_hist_entry_pack(&reply, &ent), 0, "pack entry");
    TEST_ASSERT_EQ(t, reply.data_len, calc_hist_entry_wire_size, "entry is 16 bytes");
    TEST_ASSERT(t, memcmp(reply.data, "MUL\0", 4) == 0, "op NUL-padded");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, calc_get_hist_entry(&g_dev, 5, &ent_rx), 0, "get entry 5");
    SENT(t, 0, CRUMBS_CMD_SET_REPLY);
    TEST_ASSERT_EQ(t, g_bus.last_write.data[0], CALC_OP_GET_HIST_5, "the query selected entry 5");
    TEST_ASSERT(t, strcmp(ent_rx.op, "MUL") == 0, "op string");
    TEST_ASSERT(t, ent_rx.a == 6 && ent_rx.b == 7 && ent_rx.result == 42, "entry values");

    /* An unused entry: the peripheral replies with no payload. */
    crumbs_msg_init(&reply, CALC_TYPE_ID, CALC_OP_GET_HIST_11);
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, calc_get_hist_entry(&g_dev, 11, &ent_rx), -1, "empty entry is -1");
    TEST_ASSERT_EQ(t, calc_get_hist_entry(&g_dev, 12, &ent_rx), -1, "index 12 is refused");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- Display ------------------------------------------------------------- */

static int test_display(void)
{
    const char *t = "display";
    crumbs_message_t reply;
    display_set_number_t num = {9999, 2}, num_rx;
    display_set_segments_t seg = {0x3F, 0x06, 0x5B, 0x4F}, seg_rx;
    display_set_brightness_t br = {7}, br_rx;
    display_value_result_t val = {1234, 4, 10}, val_rx;

    bind();
    TEST_ASSERT_EQ(t, display_send_set_number(&g_dev, &num), 0, "send set_number");
    SENT(t, DISPLAY_TYPE_ID, DISPLAY_OP_SET_NUMBER);
    TEST_ASSERT_EQ(t, g_bus.last_write.data_len, 3, "set_number is 3 bytes on the wire");
    TEST_ASSERT(t, memcmp(g_bus.last_write.data, "\x0F\x27\x02", 3) == 0, "set_number bytes: [number LE][decimal_pos]");
    TEST_ASSERT_EQ(t, display_set_number_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &num_rx), 0, "unpack");
    TEST_ASSERT(t, num_rx.number == 9999 && num_rx.decimal_pos == 2, "set_number fields");

    TEST_ASSERT_EQ(t, display_send_set_segments(&g_dev, &seg), 0, "send set_segments");
    SENT(t, DISPLAY_TYPE_ID, DISPLAY_OP_SET_SEGMENTS);
    TEST_ASSERT_EQ(t, display_set_segments_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &seg_rx), 0, "unpack");
    TEST_ASSERT(t, seg_rx.digit0 == 0x3F && seg_rx.digit3 == 0x4F, "segments");

    TEST_ASSERT_EQ(t, display_send_set_brightness(&g_dev, &br), 0, "send set_brightness");
    SENT(t, DISPLAY_TYPE_ID, DISPLAY_OP_SET_BRIGHTNESS);
    TEST_ASSERT_EQ(t, display_set_brightness_unpack(g_bus.last_write.data, g_bus.last_write.data_len, &br_rx), 0, "unpack");
    TEST_ASSERT_EQ(t, br_rx.level, 7, "level");

    TEST_ASSERT_EQ(t, display_send_clear(&g_dev), 0, "send clear");
    SENT(t, DISPLAY_TYPE_ID, DISPLAY_OP_CLEAR);
    TEST_ASSERT_EQ(t, g_bus.last_write.data_len, 0, "clear has no payload");

    crumbs_msg_init(&reply, DISPLAY_TYPE_ID, DISPLAY_OP_GET_VALUE);
    TEST_ASSERT_EQ(t, display_value_result_pack(&reply, &val), 0, "pack value");
    serve(&g_bus, &reply);
    TEST_ASSERT_EQ(t, display_get_value(&g_dev, &val_rx), 0, "get value");
    TEST_ASSERT(t, val_rx.number == 1234 && val_rx.decimal_pos == 4 && val_rx.brightness == 10, "value");
    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("test_lhwit_roundtrip:\n");
    failures += test_led();
    failures += test_servo();
    failures += test_calculator();
    failures += test_display();
    if (failures)
    {
        fprintf(stderr, "test_lhwit_roundtrip: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_lhwit_roundtrip: OK\n");
    return 0;
}
