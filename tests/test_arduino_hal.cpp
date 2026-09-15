/* The Arduino HAL on the host, against the scripted core and Wire in
 * tests/fake_arduino/. The peripheral half drives the onReceive/onRequest
 * handlers the HAL registers, as the controller's traffic would; the
 * controller half checks each documented return code of crumbs_arduino.h
 * and the shape of each transaction. */
#include <stdio.h>
#include <string.h>

#include <Arduino.h>
#include <Wire.h>

#include "crumbs_arduino.h"
#include "crumbs_message_helpers.h"
#include "test_common.h"

/* ---- callbacks under observation ---------------------------------------- */

static int g_messages;
static crumbs_message_t g_last;
static int g_requests;

static void on_message(crumbs_context_t *ctx, const crumbs_message_t *msg)
{
    (void)ctx;
    g_messages++;
    g_last = *msg;
}

static void on_request_untouched(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    (void)ctx;
    (void)reply;
    g_requests++;
}

static void reply_info(crumbs_context_t *ctx, crumbs_message_t *reply, void *user)
{
    (void)ctx;
    (void)user;
    crumbs_build_version_reply(reply, 0x11, 1, 2, 3);
}

static size_t encode(uint8_t type_id, uint8_t opcode, const uint8_t *payload, uint8_t len, uint8_t *frame)
{
    crumbs_message_t m;
    test_msg_create(&m, type_id, opcode, payload, len);
    return test_encode(&m, frame);
}

static fake_wire_device_t *add_crumbs_device(uint8_t addr, uint8_t type_id, uint8_t opcode,
                                            const uint8_t *payload, uint8_t len)
{
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t n = encode(type_id, opcode, payload, len, frame);
    fake_wire_device_t *d = fake_wire_add_device(addr);
    fake_wire_set_reply(d, frame, n);
    return d;
}

static void reset(void)
{
    fake_wire_reset();
    g_messages = 0;
    g_requests = 0;
    memset(&g_last, 0, sizeof g_last);
}

/* ---- peripheral ---------------------------------------------------------- */

static int test_peripheral_init(void)
{
    const char *t = "peripheral_init";
    crumbs_context_t ctx;

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_arduino_init_peripheral(&ctx, 0x10);
    TEST_ASSERT_EQ(t, ctx.role, CRUMBS_ROLE_PERIPHERAL, "peripheral role");
    TEST_ASSERT_EQ(t, ctx.address, 0x10, "address in the context");
    TEST_ASSERT_EQ(t, Wire.begun_as_target, 1, "Wire.begin(address) once");
    TEST_ASSERT_EQ(t, Wire.target_address, 0x10, "Wire joined at the address");
    TEST_ASSERT_EQ(t, Wire.begun, 0, "not also begun as a controller");
    TEST_ASSERT_EQ(t, Wire.clock, 100000, "100 kHz on AVR");
    TEST_ASSERT(t, Wire.receive_fn != NULL, "onReceive attached");
    TEST_ASSERT(t, Wire.request_fn != NULL, "onRequest attached");

    crumbs_arduino_init_peripheral(NULL, 0x10);
    TEST_ASSERT_EQ(t, Wire.begun_as_target, 1, "NULL ctx does nothing");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_peripheral_receive(void)
{
    const char *t = "peripheral_receive";
    crumbs_context_t ctx;
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    uint8_t big[BUFFER_LENGTH];
    const uint8_t payload[] = {0xAB, 0xCD};
    uint8_t full[CRUMBS_MAX_PAYLOAD];
    size_t n;

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_arduino_init_peripheral(&ctx, 0x10);
    crumbs_set_callbacks(&ctx, on_message, NULL, NULL);

    n = encode(0x11, 0x02, payload, sizeof payload, frame);
    fake_wire_deliver(frame, n);
    TEST_ASSERT_EQ(t, g_messages, 1, "a valid SET reaches on_message");
    TEST_ASSERT_EQ(t, g_last.type_id, 0x11, "type_id");
    TEST_ASSERT_EQ(t, g_last.opcode, 0x02, "opcode");
    TEST_ASSERT_EQ(t, g_last.data_len, 2, "data_len");
    TEST_ASSERT(t, memcmp(g_last.data, payload, 2) == 0, "payload");
    TEST_ASSERT_EQ(t, Wire.available(), 0, "the receive buffer is drained");
    TEST_ASSERT(t, crumbs_last_crc_ok(&ctx), "CRC stat set");

    frame[n - 1] ^= 0x01;
    fake_wire_deliver(frame, n);
    TEST_ASSERT_EQ(t, g_messages, 1, "a corrupt CRC never reaches on_message");
    TEST_ASSERT_EQ(t, crumbs_get_crc_error_count(&ctx), 1, "and is counted");
    TEST_ASSERT_EQ(t, Wire.available(), 0, "drained after a rejected frame too");

    /* The largest frame fills the HAL's buffer exactly. */
    for (size_t i = 0; i < sizeof full; ++i)
        full[i] = (uint8_t)i;
    n = encode(0x11, 0x03, full, sizeof full, frame);
    TEST_ASSERT_SIZE_EQ(t, n, CRUMBS_MESSAGE_MAX_SIZE, "31-byte frame");
    fake_wire_deliver(frame, n);
    TEST_ASSERT_EQ(t, g_messages, 2, "a 31-byte frame is delivered");
    TEST_ASSERT_EQ(t, g_last.data_len, CRUMBS_MAX_PAYLOAD, "27-byte payload");
    TEST_ASSERT(t, memcmp(g_last.data, full, sizeof full) == 0, "27 payload bytes intact");

    /* A full 32-byte Wire buffer: 31 are read, the rest drained, nothing dispatched. */
    memset(big, 0xAA, sizeof big);
    fake_wire_deliver(big, sizeof big);
    TEST_ASSERT_EQ(t, g_messages, 2, "garbage is not dispatched");
    TEST_ASSERT_EQ(t, Wire.available(), 0, "bytes past the frame buffer are drained");

    /* Wire can call onReceive with nothing. */
    fake_wire_deliver(frame, 0);
    TEST_ASSERT_EQ(t, g_messages, 2, "an empty receive is ignored");

    /* A SET_REPLY frame is intercepted: it selects the reply, on_message never sees it. */
    fake_wire_deliver(frame, encode(0x11, CRUMBS_CMD_SET_REPLY, payload, 1, frame));
    TEST_ASSERT_EQ(t, g_messages, 2, "SET_REPLY is not a message");
    TEST_ASSERT_EQ(t, ctx.requested_opcode, 0xAB, "SET_REPLY selected the opcode");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_peripheral_request(void)
{
    const char *t = "peripheral_request";
    crumbs_context_t ctx;
    crumbs_message_t reply;
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    const uint8_t want_info[] = {0x00};
    size_t n;

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_arduino_init_peripheral(&ctx, 0x10);

    /* Nothing registered: the HAL writes nothing (the AVR target then clocks
       0x00 to the controller on its own). */
    n = fake_wire_request();
    TEST_ASSERT_SIZE_EQ(t, n, 0, "no reply configured: nothing written");

    /* on_request that leaves the reply untouched sends the zero frame. */
    crumbs_set_callbacks(&ctx, NULL, on_request_untouched, NULL);
    n = fake_wire_request();
    TEST_ASSERT_EQ(t, g_requests, 1, "on_request ran");
    TEST_ASSERT_SIZE_EQ(t, n, 4, "an untouched reply is the 4-byte zero frame");
    TEST_ASSERT(t, memcmp(Wire.tx, "\0\0\0\0", 4) == 0, "00 00 00 00");

#if CRUMBS_MAX_HANDLERS > 0
    /* A reply handler wins over on_request, selected by SET_REPLY. */
    TEST_ASSERT_EQ(t, crumbs_register_reply_handler(&ctx, 0x00, reply_info, NULL), 0, "register");
    fake_wire_deliver(frame, encode(0x11, CRUMBS_CMD_SET_REPLY, want_info, 1, frame));
    n = fake_wire_request();
    TEST_ASSERT_EQ(t, g_requests, 1, "on_request not consulted when a handler matches");
    TEST_ASSERT_EQ(t, crumbs_decode_message(Wire.tx, n, &reply, NULL), 0, "reply is a valid frame");
    TEST_ASSERT_EQ(t, reply.type_id, 0x11, "reply type_id");
    TEST_ASSERT_EQ(t, reply.opcode, 0x00, "reply opcode");
    TEST_ASSERT_EQ(t, reply.data_len, 5, "version reply payload");
#else
    (void)reply;
    (void)want_info;
    (void)reply_info;
    (void)frame;
#endif
    printf("  %s: PASS\n", t);
    return 0;
}

/* crumbs_init(), which init_peripheral calls, clears callbacks and handler
   tables; they must be installed after it. */
static int test_peripheral_init_order(void)
{
    const char *t = "peripheral_init_order";
    crumbs_context_t ctx;
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    const uint8_t payload[] = {0x01};

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_set_callbacks(&ctx, on_message, NULL, NULL);
    crumbs_arduino_init_peripheral(&ctx, 0x10);
    fake_wire_deliver(frame, encode(0x11, 0x02, payload, 1, frame));
    TEST_ASSERT_EQ(t, g_messages, 0, "callbacks installed before init_peripheral are dropped");

    crumbs_set_callbacks(&ctx, on_message, NULL, NULL);
    fake_wire_deliver(frame, encode(0x11, 0x02, payload, 1, frame));
    TEST_ASSERT_EQ(t, g_messages, 1, "installed after, they run");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- controller ---------------------------------------------------------- */

static int test_controller_init(void)
{
    const char *t = "controller_init";
    crumbs_context_t ctx;

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_arduino_init_controller(&ctx);
    TEST_ASSERT_EQ(t, ctx.role, CRUMBS_ROLE_CONTROLLER, "controller role");
    TEST_ASSERT_EQ(t, Wire.begun, 1, "Wire.begin() once");
    TEST_ASSERT_EQ(t, Wire.begun_as_target, 0, "no target address");
    TEST_ASSERT_EQ(t, Wire.clock, 100000, "100 kHz on AVR");
    TEST_ASSERT(t, Wire.receive_fn == NULL && Wire.request_fn == NULL, "no callbacks in controller mode");

    crumbs_arduino_init_controller(NULL);
    TEST_ASSERT_EQ(t, Wire.begun, 1, "NULL ctx does nothing");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_wire_write(void)
{
    const char *t = "wire_write";
    crumbs_context_t ctx;
    crumbs_message_t m;
    const uint8_t frame[] = {0x11, 0x02, 0x01, 0xAA, 0xED}; /* type 0x11, SET 0x02, one byte */
    fake_wire_device_t *d;

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_arduino_init_controller(&ctx);
    d = fake_wire_add_device(0x10);

    TEST_ASSERT_EQ(t, crumbs_arduino_wire_write(NULL, 0x10, frame, sizeof frame), 0, "write via &Wire");
    TEST_ASSERT(t, strcmp(Wire.trace, "S10 W5 P1 ") == 0, "one transmission ending in STOP");
    TEST_ASSERT_SIZE_EQ(t, d->written_len, sizeof frame, "whole frame delivered");
    TEST_ASSERT(t, memcmp(d->written, frame, sizeof frame) == 0, "bytes intact");

    Wire.trace[0] = '\0';
    TEST_ASSERT_EQ(t, crumbs_arduino_wire_write(&Wire, 0x10, frame, sizeof frame), 0, "explicit TwoWire*");
    TEST_ASSERT_EQ(t, crumbs_arduino_wire_write(NULL, 0x30, frame, sizeof frame), 2,
                   "an address NACK is Wire's code 2");
    TEST_ASSERT_EQ(t, crumbs_arduino_wire_write(NULL, 0x10, NULL, 1), -1, "NULL data with a length");
    TEST_ASSERT_EQ(t, crumbs_arduino_wire_write(NULL, 0x10, NULL, 0), 0, "zero bytes is an address-only transmission");

    Wire.write_returns = 3;
    TEST_ASSERT_EQ(t, crumbs_arduino_wire_write(NULL, 0x10, frame, sizeof frame), -2, "-2 on a short write");
    Wire.write_returns = 0;

    /* Through the core: the frame on the wire is the encoded message. */
    d->written_len = 0;
    test_msg_create(&m, 0x11, 0x02, frame + 3, 1);
    TEST_ASSERT_EQ(t, crumbs_controller_send(&ctx, 0x10, &m, crumbs_arduino_wire_write, NULL), 0, "send");
    TEST_ASSERT_SIZE_EQ(t, d->written_len, sizeof frame, "send wrote one frame");
    TEST_ASSERT(t, memcmp(d->written, frame, sizeof frame) == 0, "encoded as expected");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_scan(void)
{
    const char *t = "scan";
    uint8_t found[8];
    const uint8_t one[] = {0x01};
    fake_wire_device_t *d;
    int n;

    reset();
    d = add_crumbs_device(0x10, 0x11, 0x00, one, 1);
    fake_wire_add_device(0x20); /* ACKs; clocks padding on a read */

    n = crumbs_arduino_scan(NULL, 0x08, 0x77, 1, found, sizeof found);
    TEST_ASSERT_EQ(t, n, 2, "strict: every address that ACKs a read");
    TEST_ASSERT_EQ(t, found[0], 0x10, "0x10");
    TEST_ASSERT_EQ(t, found[1], 0x20, "0x20");
    TEST_ASSERT(t, strstr(Wire.trace, "Q10:1 ") != NULL, "strict probe is a one-byte read");
    TEST_ASSERT(t, strstr(Wire.trace, "S") == NULL, "strict mode never addresses for a write");
    TEST_ASSERT_SIZE_EQ(t, d->reply_pos, 1, "the probe consumed one byte");
    TEST_ASSERT_EQ(t, Wire.available(), 0, "the byte was drained");

    Wire.trace[0] = '\0';
    n = crumbs_arduino_scan(NULL, 0x08, 0x77, 0, found, sizeof found);
    TEST_ASSERT_EQ(t, n, 2, "non-strict: every address that ACKs");
    TEST_ASSERT(t, strstr(Wire.trace, "S10 P1 ") != NULL, "address-only transmission");
    TEST_ASSERT(t, strstr(Wire.trace, "Q") == NULL && strstr(Wire.trace, "W") == NULL,
                "non-strict mode neither reads nor writes data");
    TEST_ASSERT_SIZE_EQ(t, d->writes, 0, "nothing written to the device");

    n = crumbs_arduino_scan(NULL, 0x08, 0x77, 0, found, 1);
    TEST_ASSERT_EQ(t, n, 2, "count is the total, found holds max_found");
    TEST_ASSERT_EQ(t, crumbs_arduino_scan(NULL, 0x08, 0x77, 0, NULL, 1), -1, "NULL found");
    TEST_ASSERT_EQ(t, crumbs_arduino_scan(NULL, 0x08, 0x77, 0, found, 0), -1, "max_found 0");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_read(void)
{
    const char *t = "read";
    crumbs_context_t ctx;
    crumbs_message_t reply;
    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    const uint8_t payload[] = {1, 2, 3};
    fake_wire_device_t *d;
    unsigned long before;

    reset();
    memset(&ctx, 0, sizeof ctx);
    crumbs_arduino_init_controller(&ctx);
    d = add_crumbs_device(0x10, 0x11, 0x05, payload, sizeof payload);

    TEST_ASSERT_EQ(t, crumbs_arduino_read(NULL, 0x10, buf, sizeof buf, 0), (int)sizeof buf,
                   "a 31-byte request yields 31 bytes");
    TEST_ASSERT(t, strcmp(Wire.trace, "Q10:31 ") == 0, "one requestFrom");
    TEST_ASSERT(t, memcmp(buf, d->reply, d->reply_len) == 0, "frame at the front");
    TEST_ASSERT_EQ(t, buf[d->reply_len], 0xFF, "padding after it");
    TEST_ASSERT_EQ(t, micros(), 0, "no waiting when everything arrived");

    d->reply_pos = 0;
    TEST_ASSERT_EQ(t, crumbs_controller_read(&ctx, 0x10, &reply, crumbs_arduino_read, NULL), 0,
                   "the core decodes through the padding");
    TEST_ASSERT_EQ(t, reply.data_len, 3, "payload length");
    TEST_ASSERT(t, memcmp(reply.data, payload, 3) == 0, "payload");

    /* An absent address: timeout 0 takes what is buffered and returns. */
    before = micros();
    TEST_ASSERT_EQ(t, crumbs_arduino_read(NULL, 0x30, buf, sizeof buf, 0), 0, "NACK reads nothing");
    TEST_ASSERT_EQ(t, micros() - before, 0, "timeout 0 does not wait");

    /* With a timeout it polls until the deadline, 50 us a step. */
    before = micros();
    TEST_ASSERT_EQ(t, crumbs_arduino_read(NULL, 0x30, buf, sizeof buf, 1000), 0, "still nothing");
    TEST_ASSERT_EQ(t, micros() - before, 1000, "waited exactly the timeout");

    /* A short delivery returns what arrived. */
    d->reply_pos = 0;
    d->deliver_max = 4;
    TEST_ASSERT_EQ(t, crumbs_arduino_read(NULL, 0x10, buf, sizeof buf, 0), 4, "short delivery, no wait");
    TEST_ASSERT_EQ(t, crumbs_controller_read(&ctx, 0x10, &reply, crumbs_arduino_read, NULL), -1,
                   "a truncated frame does not decode");

    TEST_ASSERT_EQ(t, crumbs_arduino_read(NULL, 0x10, NULL, 1, 0), -1, "NULL buffer");
    TEST_ASSERT_EQ(t, crumbs_arduino_read(NULL, 0x10, buf, 0, 0), -1, "zero length");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_write_then_read(void)
{
    const char *t = "write_then_read";
    const uint8_t tx[] = {0x11, 0x01, 0x00, 0xDC}; /* a payload-less query frame */
    const uint8_t payload[] = {0x42};
    uint8_t rx[8];
    uint8_t big[BUFFER_LENGTH + 1];
    fake_wire_device_t *d;

    reset();
    d = add_crumbs_device(0x10, 0x11, 0x01, payload, sizeof payload);

    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, NULL, 0, NULL, 0, 0, 0), 0, "empty call");
    TEST_ASSERT(t, Wire.trace[0] == '\0', "empty call touches the bus not at all");

    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, tx, sizeof tx, NULL, 0, 0, 0), 0, "write only");
    TEST_ASSERT(t, strcmp(Wire.trace, "S10 W4 P1 ") == 0, "write only: one transmission");
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x30, tx, sizeof tx, NULL, 0, 0, 0), -2,
                   "write only to an absent address");

    /* Write, STOP, then read. */
    Wire.trace[0] = '\0';
    d->written_len = 0;
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, tx, sizeof tx, rx, 5, 0, 0), 5, "five bytes back");
    TEST_ASSERT(t, strcmp(Wire.trace, "S10 W4 P1 Q10:5 ") == 0, "STOP between the phases");
    TEST_ASSERT(t, memcmp(rx, d->reply, 5) == 0, "reply bytes");
    TEST_ASSERT(t, memcmp(d->written, tx, sizeof tx) == 0, "tx bytes");

    /* Repeated start: the write phase ends without STOP. */
    Wire.trace[0] = '\0';
    d->reply_pos = 0;
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, tx, sizeof tx, rx, 5, 0, 1), 5, "five bytes back");
    TEST_ASSERT(t, strcmp(Wire.trace, "S10 W4 P0 Q10:5 ") == 0, "no STOP between the phases");

    /* Read only through the same call. */
    Wire.trace[0] = '\0';
    d->reply_pos = 0;
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, NULL, 0, rx, 5, 0, 0), 5, "read only");
    TEST_ASSERT(t, strcmp(Wire.trace, "Q10:5 ") == 0, "read only: one requestFrom");

    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x30, tx, sizeof tx, rx, 5, 0, 0), -2,
                   "-2 when the write phase NACKs");
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x30, NULL, 0, rx, 5, 500, 0), 0,
                   "a NACKed read phase returns nothing");
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, big, sizeof big, rx, 5, 0, 0), -6,
                   "-6 when tx exceeds the Wire buffer");
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, tx, sizeof tx, big, sizeof big, 0, 0), -6,
                   "-6 when rx exceeds the Wire buffer");
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, NULL, 2, rx, 5, 0, 0), -1, "tx NULL with tx_len");
    TEST_ASSERT_EQ(t, crumbs_arduino_write_then_read(NULL, 0x10, tx, 2, NULL, 5, 0, 0), -1, "rx NULL with rx_len");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_timing(void)
{
    const char *t = "timing";
    reset();
    fake_arduino_advance_us(2500);
    TEST_ASSERT_EQ(t, crumbs_arduino_millis(), 2, "millis from the core clock");
    crumbs_arduino_delay_us(0);
    TEST_ASSERT_EQ(t, micros(), 2500, "delay 0 delays nothing");
    crumbs_arduino_delay_us(300);
    TEST_ASSERT_EQ(t, micros(), 2800, "delay_us wraps delayMicroseconds");
    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("test_arduino_hal:\n");
    failures += test_peripheral_init();
    failures += test_peripheral_receive();
    failures += test_peripheral_request();
    failures += test_peripheral_init_order();
    failures += test_controller_init();
    failures += test_wire_write();
    failures += test_scan();
    failures += test_read();
    failures += test_write_then_read();
    failures += test_timing();
    if (failures)
    {
        fprintf(stderr, "test_arduino_hal: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_arduino_hal: OK\n");
    return 0;
}
