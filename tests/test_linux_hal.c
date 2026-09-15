/* The Linux HAL against a scripted linux-wire (fake_linux_wire.c). Each
 * test drives one documented return code or one transaction shape of
 * crumbs_linux.h. The fake pads reads the way i2c-dev does, and can also
 * return short counts and end-of-data, which the HAL's read loops handle
 * even though i2c-dev never produces them. */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "crumbs_linux.h"
#include "fake_linux_wire.h"
#include "test_common.h"

#define DEV "/dev/i2c-1"

static int open_bus(crumbs_context_t *ctx, crumbs_linux_i2c_t *i2c, uint32_t timeout_us)
{
    fake_lw_reset();
    return crumbs_linux_init_controller(ctx, i2c, DEV, timeout_us);
}

/* A device that clocks out the encoded frame for (type, opcode, payload)
   and then 0xFF padding, as a peripheral does on a fixed-count read. */
static fake_lw_device_t *add_crumbs_device(uint8_t addr, uint8_t type_id, uint8_t opcode,
                                          const uint8_t *payload, uint8_t payload_len)
{
    crumbs_message_t m;
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t n;
    fake_lw_device_t *d = fake_lw_add_device(addr);

    test_msg_create(&m, type_id, opcode, payload, payload_len);
    n = test_encode(&m, frame);
    fake_lw_set_reply(d, frame, n);
    return d;
}

/* ---- init / close ------------------------------------------------------- */

static int test_init(void)
{
    const char *t = "init";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;

    TEST_ASSERT_EQ(t, open_bus(&ctx, &i2c, 0), 0, "opens the bus");
    TEST_ASSERT(t, i2c.bus.fd >= 0, "handle holds the open descriptor");
    TEST_ASSERT_EQ(t, ctx.role, CRUMBS_ROLE_CONTROLLER, "context is a controller");
    TEST_ASSERT_EQ(t, i2c.bus.timeout_us, 0, "timeout 0 leaves the bus default");
    TEST_ASSERT_EQ(t, strcmp(i2c.bus.device_path, DEV), 0, "device path recorded");

    TEST_ASSERT_EQ(t, open_bus(&ctx, &i2c, 25000), 0, "opens with a timeout");
    TEST_ASSERT_EQ(t, i2c.bus.timeout_us, 25000, "timeout stored on the bus handle");

    TEST_ASSERT_EQ(t, crumbs_linux_init_controller(NULL, &i2c, DEV, 0), -1, "NULL ctx");
    TEST_ASSERT_EQ(t, crumbs_linux_init_controller(&ctx, NULL, DEV, 0), -1, "NULL handle");
    TEST_ASSERT_EQ(t, crumbs_linux_init_controller(&ctx, &i2c, NULL, 0), -1, "NULL path");
    TEST_ASSERT_EQ(t, crumbs_linux_init_controller(&ctx, &i2c, "", 0), -1, "empty path");

    fake_lw_reset();
    fake_lw.open_fails = 1;
    TEST_ASSERT_EQ(t, crumbs_linux_init_controller(&ctx, &i2c, DEV, 0), -2, "open failure");
    TEST_ASSERT_EQ(t, i2c.bus.fd, -1, "failed open leaves the handle closed");

    crumbs_linux_close(&i2c);
    TEST_ASSERT_EQ(t, i2c.bus.fd, -1, "close leaves fd -1");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- crumbs_linux_i2c_write ---------------------------------------------- */

static int test_write(void)
{
    const char *t = "write";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    const uint8_t frame[] = {0x11, 0x02, 0x01, 0xAA, 0xED}; /* type 0x11, SET 0x02, one byte */
    fake_lw_device_t *d;

    open_bus(&ctx, &i2c, 0);
    d = fake_lw_add_device(0x10);
    fake_lw_add_device(0x20)->driver_owned = 1;

    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x10, frame, sizeof frame), 0, "write succeeds");
    TEST_ASSERT_SIZE_EQ(t, d->written_len, sizeof frame, "whole frame reached the device");
    TEST_ASSERT(t, memcmp(d->written, frame, sizeof frame) == 0, "bytes intact");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "T10 W5 ") == 0, "select then one write");

    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x30, frame, sizeof frame), -3,
                   "-3 when the address NACKs");
    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x20, frame, sizeof frame), -2,
                   "-2 when a kernel driver owns the address");

    fake_lw.write_returns = (ssize_t)sizeof frame - 1;
    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x10, frame, sizeof frame), -4, "-4 on a short write");
    fake_lw.write_returns = -1;

    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(NULL, 0x10, frame, sizeof frame), -1, "NULL handle");
    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x10, NULL, 1), -1, "NULL data");
    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x10, frame, 0), -1, "zero length");
    crumbs_linux_close(&i2c);
    TEST_ASSERT_EQ(t, crumbs_linux_i2c_write(&i2c, 0x10, frame, sizeof frame), -1, "closed bus");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- crumbs_linux_read --------------------------------------------------- */

static int test_read(void)
{
    const char *t = "read";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    const uint8_t payload[] = {1, 2, 3};
    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    fake_lw_device_t *d;

    open_bus(&ctx, &i2c, 0);
    d = add_crumbs_device(0x10, 0x11, 0x05, payload, sizeof payload);

    /* i2c-dev returns the count asked for: the 7-byte frame then padding. */
    memset(buf, 0, sizeof buf);
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, buf, sizeof buf, 0), (int)sizeof buf,
                   "padded read returns the full count");
    TEST_ASSERT(t, memcmp(buf, d->reply, d->reply_len) == 0, "frame at the front");
    TEST_ASSERT_EQ(t, buf[d->reply_len], 0xFF, "padding after the frame");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "T10 R31 ") == 0, "one select, one read");

    /* Short counts accumulate into one contiguous buffer. */
    d->reply_pos = 0;
    d->read_chunk = 3;
    memset(buf, 0, sizeof buf);
    fake_lw.trace[0] = '\0';
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, buf, 10, 0), 10, "short reads add up to the count");
    TEST_ASSERT(t, memcmp(buf, d->reply, d->reply_len) == 0, "chunks land in order");
    TEST_ASSERT_EQ(t, buf[7], 0xFF, "padding continues across chunks");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "T10 R10 R7 R4 R1 ") == 0, "each read asks for what is left");

    /* End of data stops the loop early. */
    d->reply_pos = 0;
    d->read_chunk = 0;
    d->pad_reads = 0;
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, buf, sizeof buf, 0), (int)d->reply_len,
                   "read returns what the device had when it stops early");

    TEST_ASSERT_EQ(t, i2c.bus.timeout_us, 0, "timeout untouched so far");
    d->reply_pos = 0;
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, buf, sizeof buf, 5000), (int)d->reply_len,
                   "read with a timeout");
    TEST_ASSERT_EQ(t, i2c.bus.timeout_us, 5000, "timeout stored on the bus handle");

    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x30, buf, sizeof buf, 0), -3, "-3 when the address NACKs");
    fake_lw_add_device(0x20)->driver_owned = 1;
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x20, buf, sizeof buf, 0), -2, "-2 when a driver owns it");
    fake_lw.read_errno = EIO;
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, buf, sizeof buf, 0), -3, "-3 on an I/O error");
    fake_lw.read_errno = 0;

    TEST_ASSERT_EQ(t, crumbs_linux_read(NULL, 0x10, buf, sizeof buf, 0), -1, "NULL handle");
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, NULL, 1, 0), -1, "NULL buffer");
    TEST_ASSERT_EQ(t, crumbs_linux_read(&i2c, 0x10, buf, 0, 0), -1, "zero length");
    printf("  %s: PASS\n", t);
    return 0;
}

/* The core's controller read over this HAL: the padded read is trimmed to
   the header length and decodes. */
static int test_controller_read_over_hal(void)
{
    const char *t = "controller_read";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    crumbs_message_t reply;
    const uint8_t payload[] = {0xDE, 0xAD};

    open_bus(&ctx, &i2c, 0);
    add_crumbs_device(0x10, 0x11, 0x05, payload, sizeof payload);

    TEST_ASSERT_EQ(t, crumbs_controller_read(&ctx, 0x10, &reply, crumbs_linux_read, &i2c), 0,
                   "decodes through the padding");
    TEST_ASSERT_EQ(t, reply.type_id, 0x11, "type_id");
    TEST_ASSERT_EQ(t, reply.opcode, 0x05, "opcode");
    TEST_ASSERT_EQ(t, reply.data_len, 2, "data_len");
    TEST_ASSERT(t, memcmp(reply.data, payload, 2) == 0, "payload");
    TEST_ASSERT(t, crumbs_last_crc_ok(&ctx), "CRC stat set");

    TEST_ASSERT_EQ(t, crumbs_controller_read(&ctx, 0x30, &reply, crumbs_linux_read, &i2c), -1,
                   "an absent address is a short read");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- crumbs_linux_read_message ------------------------------------------ */

static int test_read_message(void)
{
    const char *t = "read_message";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    crumbs_message_t msg;
    const uint8_t payload[] = {7, 8, 9, 10};
    fake_lw_device_t *d;
    uint8_t bad[] = {0x11, 0x05, 46, 0x00};

    open_bus(&ctx, &i2c, 0);
    d = add_crumbs_device(0x10, 0x11, 0x05, payload, sizeof payload);

    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x10, &ctx, &msg), 0, "decodes a padded frame");
    TEST_ASSERT_EQ(t, msg.data_len, 4, "payload length");
    TEST_ASSERT(t, memcmp(msg.data, payload, 4) == 0, "payload bytes");
    TEST_ASSERT(t, crumbs_last_crc_ok(&ctx), "CRC stat set");

    /* Reads that stop early still decode from what arrived. */
    d->reply_pos = 0;
    d->pad_reads = 0;
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x10, &ctx, &msg), 0, "decodes an unpadded frame");

    /* A header claiming 46 payload bytes is rejected before the CRC. */
    fake_lw_set_reply(d, bad, sizeof bad);
    d->pad_reads = 1;
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x10, &ctx, &msg), -1, "garbage header");
    TEST_ASSERT(t, !crumbs_last_crc_ok(&ctx), "garbage header clears the CRC stat");

    /* A corrupt CRC is a decode error and counts. */
    d->reply_pos = 0;
    add_crumbs_device(0x12, 0x11, 0x05, payload, sizeof payload)->reply[7] ^= 0x01;
    TEST_ASSERT(t, crumbs_linux_read_message(&i2c, 0x12, &ctx, &msg) < 0, "corrupt CRC rejected");
    TEST_ASSERT_EQ(t, crumbs_get_crc_error_count(&ctx), 1, "CRC error counted");

    /* Nothing at all from the device. */
    fake_lw_add_device(0x14)->pad_reads = 0;
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x14, &ctx, &msg), -4, "-4 when no bytes arrive");

    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x30, &ctx, &msg), -3, "-3 when the address NACKs");
    fake_lw_add_device(0x20)->driver_owned = 1;
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x20, &ctx, &msg), -2, "-2 when a driver owns it");
    add_crumbs_device(0x16, 0x11, 0x05, payload, sizeof payload);
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x16, NULL, &msg), 0, "ctx may be NULL");
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(NULL, 0x10, &ctx, &msg), -1, "NULL handle");
    TEST_ASSERT_EQ(t, crumbs_linux_read_message(&i2c, 0x10, &ctx, NULL), -1, "NULL out");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- crumbs_linux_write_then_read --------------------------------------- */

static int test_write_then_read(void)
{
    const char *t = "write_then_read";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    const uint8_t tx[] = {0x11, 0x01, 0x00, 0xDC}; /* a payload-less query frame */
    const uint8_t payload[] = {0x42};
    uint8_t rx[8];
    fake_lw_device_t *d;

    open_bus(&ctx, &i2c, 0);
    d = add_crumbs_device(0x10, 0x11, 0x01, payload, sizeof payload);

    /* Nothing asked for: nothing on the bus. */
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, NULL, 0, NULL, 0, 0, 0), 0, "empty call");
    TEST_ASSERT(t, fake_lw.trace[0] == '\0', "empty call touches the bus not at all");

    /* Write only. */
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, tx, sizeof tx, NULL, 0, 0, 0), 0, "write only");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "T10 W4 ") == 0, "write only: select, write");
    TEST_ASSERT_SIZE_EQ(t, d->written_len, sizeof tx, "bytes delivered");
    fake_lw.write_returns = 2;
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, tx, sizeof tx, NULL, 0, 0, 0), -4, "short write");
    fake_lw.write_returns = -1;
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x30, tx, sizeof tx, NULL, 0, 0, 0), -3, "write NACK");

    /* Write, STOP, then read. */
    d->written_len = 0;
    fake_lw.trace[0] = '\0';
    memset(rx, 0, sizeof rx);
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, tx, sizeof tx, rx, 5, 0, 0), 5,
                   "returns the bytes read");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "T10 W4 T10 R5 ") == 0, "two transactions with a STOP between");
    TEST_ASSERT(t, memcmp(rx, d->reply, 5) == 0, "reply bytes");
    TEST_ASSERT(t, memcmp(d->written, tx, sizeof tx) == 0, "tx bytes");

    /* Read only through the same call. */
    d->reply_pos = 0;
    fake_lw.trace[0] = '\0';
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, NULL, 0, rx, 5, 0, 0), 5, "read only");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "T10 R5 ") == 0, "read only: select, read");

    /* Repeated start: one I2C_RDWR transfer, no select, no separate write. */
    d->reply_pos = 0;
    d->written_len = 0;
    fake_lw.trace[0] = '\0';
    memset(rx, 0, sizeof rx);
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, tx, sizeof tx, rx, 5, 0, 1), 5,
                   "repeated start returns the bytes read");
    TEST_ASSERT(t, strcmp(fake_lw.trace, "I10 ") == 0, "repeated start is one combined transfer");
    TEST_ASSERT(t, memcmp(rx, d->reply, 5) == 0, "combined transfer reply");
    TEST_ASSERT(t, memcmp(d->written, tx, sizeof tx) == 0, "combined transfer tx");
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x30, tx, sizeof tx, rx, 5, 0, 1), -3,
                   "combined transfer NACK");

    /* Reads that stop early. */
    d->reply_pos = 0;
    d->pad_reads = 0;
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, NULL, 0, rx, 8, 0, 0), 5,
                   "early end of data returns what arrived");

    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, NULL, 0, rx, 5, 7000, 0), 0,
                   "timeout path with an exhausted device");
    TEST_ASSERT_EQ(t, i2c.bus.timeout_us, 7000, "timeout stored on the bus handle");

    fake_lw_add_device(0x20)->driver_owned = 1;
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x20, tx, sizeof tx, rx, 5, 0, 0), -2,
                   "-2 when a driver owns it");
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, NULL, 2, rx, 5, 0, 0), -1, "tx NULL with tx_len");
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, tx, 2, NULL, 5, 0, 0), -1, "rx NULL with rx_len");
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(NULL, 0x10, tx, 2, rx, 5, 0, 0), -1, "NULL handle");
    crumbs_linux_close(&i2c);
    TEST_ASSERT_EQ(t, crumbs_linux_write_then_read(&i2c, 0x10, tx, 2, rx, 5, 0, 0), -1, "closed bus");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- crumbs_linux_scan (raw address sweep) ------------------------------- */

static int test_scan_strict(void)
{
    const char *t = "scan_strict";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    uint8_t found[8];
    const uint8_t one[] = {0x01};
    int n;

    open_bus(&ctx, &i2c, 0);
    add_crumbs_device(0x10, 0x11, 0x00, one, 1);
    fake_lw_add_device(0x20)->driver_owned = 1;
    fake_lw_add_device(0x30)->pad_reads = 0; /* ACKs but clocks out nothing */

    n = crumbs_linux_scan(&i2c, 0x08, 0x77, 1, found, sizeof found);
    TEST_ASSERT_EQ(t, n, 2, "responder and driver-owned address found; silent ACK is not");
    TEST_ASSERT_EQ(t, found[0], 0x10, "0x10");
    TEST_ASSERT_EQ(t, found[1], 0x20, "0x20 (driver-owned)");
    TEST_ASSERT(t, strstr(fake_lw.trace, "T10 R1 ") != NULL, "strict probe is a one-byte read");
    TEST_ASSERT(t, strstr(fake_lw.trace, "P") == NULL, "strict mode never uses Quick Write");
    TEST_ASSERT_EQ(t, i2c.bus.log_errors, 1, "error logging restored after the sweep");

    n = crumbs_linux_scan(&i2c, 0x08, 0x77, 1, found, 1);
    TEST_ASSERT_EQ(t, n, 1, "stops at max_found");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_scan_quick(void)
{
    const char *t = "scan_quick";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    uint8_t found[8];
    int n;

    open_bus(&ctx, &i2c, 0);
    fake_lw_add_device(0x10);
    fake_lw_add_device(0x20)->driver_owned = 1;
    fake_lw_add_device(0x30)->pad_reads = 0;

    n = crumbs_linux_scan(&i2c, 0x08, 0x77, 0, found, sizeof found);
    TEST_ASSERT_EQ(t, n, 3, "every ACKing address found, including driver-owned");
    TEST_ASSERT_EQ(t, found[2], 0x30, "an address that ACKs without data is present");
    TEST_ASSERT(t, strstr(fake_lw.trace, "R") == NULL, "Quick Write puts no data on the bus");
    TEST_ASSERT_EQ(t, i2c.bus.log_errors, 1, "error logging restored after the sweep");

    i2c.bus.log_errors = 0;
    n = crumbs_linux_scan(&i2c, 0x08, 0x77, 0, found, sizeof found);
    TEST_ASSERT_EQ(t, i2c.bus.log_errors, 0, "restores the caller's setting, not a default");
    i2c.bus.log_errors = 1;

    fake_lw.quick_unsupported = 1;
    n = crumbs_linux_scan(&i2c, 0x08, 0x77, 0, found, sizeof found);
    TEST_ASSERT_EQ(t, n, -2, "-2 when the adapter cannot do Quick Write");
    TEST_ASSERT_EQ(t, i2c.bus.log_errors, 1, "error logging restored on that failure too");
    fake_lw.quick_unsupported = 0;

    TEST_ASSERT_EQ(t, crumbs_linux_scan(NULL, 0x08, 0x77, 0, found, sizeof found), -1, "NULL handle");
    TEST_ASSERT_EQ(t, crumbs_linux_scan(&i2c, 0x08, 0x77, 0, NULL, 1), -1, "NULL found");
    TEST_ASSERT_EQ(t, crumbs_linux_scan(&i2c, 0x08, 0x77, 0, found, 0), -1, "max_found 0");
    crumbs_linux_close(&i2c);
    TEST_ASSERT_EQ(t, crumbs_linux_scan(&i2c, 0x08, 0x77, 0, found, sizeof found), -1, "closed bus");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- crumbs_linux_scan_for_crumbs[_with_types] --------------------------- */

static int test_scan_for_crumbs(void)
{
    const char *t = "scan_for_crumbs";
    crumbs_context_t ctx;
    crumbs_linux_i2c_t i2c;
    uint8_t found[8], types[8];
    const uint8_t v[] = {0x01};
    const uint8_t ezo[] = {0x01, '9', '.', '5', '6', '0', 0x00};
    fake_lw_device_t *quiet;
    crumbs_message_t probe;
    int n;

    open_bus(&ctx, &i2c, 0);
    add_crumbs_device(0x10, 0x11, 0x00, v, 1);
    add_crumbs_device(0x12, 0x22, 0x00, v, 1);
    fake_lw_set_reply(fake_lw_add_device(0x63), ezo, sizeof ezo); /* EZO-style ASCII */
    quiet = add_crumbs_device(0x15, 0x33, 0x00, v, 1);
    quiet->reply_after_write = 1; /* answers only once asked */

    memset(types, 0, sizeof types);
    n = crumbs_linux_scan_for_crumbs_with_types(&ctx, &i2c, 0x08, 0x77, 1, found, types, 8, 0);
    TEST_ASSERT_EQ(t, n, 2, "strict: the two devices that reply unprompted");
    TEST_ASSERT_EQ(t, found[0], 0x10, "0x10 found");
    TEST_ASSERT_EQ(t, types[0], 0x11, "0x10 type");
    TEST_ASSERT_EQ(t, found[1], 0x12, "0x12 found");
    TEST_ASSERT_EQ(t, types[1], 0x22, "0x12 type");
    TEST_ASSERT_EQ(t, i2c.bus.log_errors, 1, "error logging back on after the scan");
    TEST_ASSERT_SIZE_EQ(t, quiet->writes, 0, "strict mode writes nothing");

    for (size_t i = 0; i < fake_lw.ndev; ++i)
        fake_lw.dev[i].reply_pos = 0;
    n = crumbs_linux_scan_for_crumbs_with_types(&ctx, &i2c, 0x08, 0x77, 0, found, types, 8, 0);
    TEST_ASSERT_EQ(t, n, 3, "non-strict: the probe write wakes the quiet device");
    TEST_ASSERT_EQ(t, found[2], 0x15, "0x15 found after the probe");
    TEST_ASSERT_EQ(t, types[2], 0x33, "0x15 type");
    TEST_ASSERT_SIZE_EQ(t, quiet->writes, 1, "one probe write");
    TEST_ASSERT_EQ(t, crumbs_decode_message(quiet->written, quiet->written_len, &probe, NULL), 0,
                   "the probe is a valid frame");
    TEST_ASSERT_EQ(t, probe.type_id, 0, "probe type_id 0");
    TEST_ASSERT_EQ(t, probe.opcode, 0, "probe opcode 0");
    TEST_ASSERT_EQ(t, probe.data_len, 0, "probe carries no payload");

    for (size_t i = 0; i < fake_lw.ndev; ++i)
        fake_lw.dev[i].reply_pos = 0;
    quiet->writes = 0; /* forget the probe: quiet again */
    n = crumbs_linux_scan_for_crumbs(&ctx, &i2c, 0x08, 0x77, 1, found, 8, 0);
    TEST_ASSERT_EQ(t, n, 2, "address-only wrapper agrees");

    TEST_ASSERT_EQ(t, crumbs_linux_scan_for_crumbs_with_types(NULL, &i2c, 0x08, 0x77, 1, found, types, 8, 0),
                   -1, "NULL ctx");
    TEST_ASSERT_EQ(t, crumbs_linux_scan_for_crumbs_with_types(&ctx, NULL, 0x08, 0x77, 1, found, types, 8, 0),
                   -1, "NULL handle");
    TEST_ASSERT_EQ(t, crumbs_linux_scan_for_crumbs_with_types(&ctx, &i2c, 0x08, 0x77, 1, NULL, types, 8, 0),
                   -1, "NULL found");
    printf("  %s: PASS\n", t);
    return 0;
}

/* ---- timing -------------------------------------------------------------- */

static int test_timing(void)
{
    const char *t = "timing";
    uint32_t before = crumbs_linux_millis();
    crumbs_linux_delay_us(0);
    crumbs_linux_delay_us(2000);
    TEST_ASSERT(t, crumbs_linux_millis() - before >= 1, "2 ms delay advances the millisecond clock");
    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("test_linux_hal:\n");
    failures += test_init();
    failures += test_write();
    failures += test_read();
    failures += test_controller_read_over_hal();
    failures += test_read_message();
    failures += test_write_then_read();
    failures += test_scan_strict();
    failures += test_scan_quick();
    failures += test_scan_for_crumbs();
    failures += test_timing();
    if (failures)
    {
        fprintf(stderr, "test_linux_hal: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_linux_hal: OK\n");
    return 0;
}
