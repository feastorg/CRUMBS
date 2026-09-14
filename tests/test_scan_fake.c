/*
 * Test the crumbs_controller_scan_for_crumbs logic using fake read/write
 * implementations that emulate devices at specific addresses.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "crumbs.h"

/* We'll emulate two CRUMBS devices at 0x08 and 0x10 */
#define DEV_A 0x08
#define DEV_B 0x10

static int fake_write(void *user_ctx, uint8_t addr, const uint8_t *data, size_t len)
{
    (void)user_ctx;
    (void)data;
    (void)len;
    /* accept writes for DEV_A and DEV_B */
    if (addr == DEV_A || addr == DEV_B)
        return 0;
    return -1;
}

static int fake_read(void *user_ctx, uint8_t addr, uint8_t *buffer, size_t len, uint32_t timeout_us)
{
    (void)user_ctx;
    (void)timeout_us;
    if (addr != DEV_A && addr != DEV_B)
        return 0; /* no data */

    /* Build a valid CRUMBS frame using encode helper */
    crumbs_message_t m;
    memset(&m, 0, sizeof(m));
    m.type_id = (uint8_t)addr; /* unique but arbitrary */
    m.opcode = 0x1;
    m.data_len = 3;
    m.data[0] = (uint8_t)addr;
    m.data[1] = 0xAA;
    m.data[2] = 0xBB;

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t w = crumbs_encode_message(&m, frame, sizeof(frame));
    if (w == 0 || w > len)
        return -1;

    memcpy(buffer, frame, w);
    return (int)w;
}

static int fake_read_noncrumbs(void *user_ctx, uint8_t addr, uint8_t *buf, size_t len, uint32_t to)
{
    (void)user_ctx;
    (void)to;
    if (addr != DEV_A)
        return 0;
    if (len < 5)
        return 0;
    /* write 5 bytes of garbage (not a valid CRUMBS frame) */
    for (size_t i = 0; i < 5 && i < len; ++i)
        buf[i] = (uint8_t)(i + 1);
    return 5;
}

/* Foreign devices on a mixed bus. These model what real non-CRUMBS parts put
   on the wire when read, so the scanner's rejection is checked against
   plausible traffic rather than arbitrary garbage. */

/* Atlas Scientific EZO: a status byte followed by an ASCII reading, e.g.
   "9.560" (dissolved oxygen in mg/L), NUL-terminated. Byte 2 is '.' (0x2E =
   46), above CRUMBS_MAX_PAYLOAD, so the header itself is rejected - and even
   with that bound removed the declared 50-byte frame is longer than the 7
   bytes read. Every ASCII digit and '.' exceeds 27, so no EZO reading can
   pass the header check; the CRC is never consulted. */
static int fake_read_ezo(void *user_ctx, uint8_t addr, uint8_t *buf, size_t len, uint32_t to)
{
    (void)user_ctx;
    (void)to;
    static const uint8_t reply[] = {0x01, '9', '.', '5', '6', '0', 0x00};
    if (addr != DEV_A)
        return 0;
    if (len < sizeof(reply))
        return 0;
    memcpy(buf, reply, sizeof(reply));
    return (int)sizeof(reply);
}

/* Bosch BMP280 read with the register pointer at 0xD0: chip id 0x58, then the
   reserved registers 0xD1.. which read as zero. As a CRUMBS header this is
   type 0x58, opcode 0x00, data_len 0 - well-formed, declaring a 4-byte frame -
   so the only thing that can reject it is the CRC: crc8(58 00 00) = 0x75, and
   the device supplies 0x00. */
#define BMP280_CRC_OK 0x75

static int fake_read_bmp280_impl(uint8_t addr, uint8_t *buf, size_t len, uint8_t byte3)
{
    if (addr != DEV_A)
        return 0;
    if (len < CRUMBS_MESSAGE_MAX_SIZE)
        return 0;
    memset(buf, 0, CRUMBS_MESSAGE_MAX_SIZE);
    buf[0] = 0x58;
    buf[3] = byte3;
    return (int)CRUMBS_MESSAGE_MAX_SIZE;
}

static int fake_read_bmp280(void *user_ctx, uint8_t addr, uint8_t *buf, size_t len, uint32_t to)
{
    (void)user_ctx;
    (void)to;
    return fake_read_bmp280_impl(addr, buf, len, 0x00);
}

/* Same dump, but byte 3 happens to equal the CRC of the first three bytes.
   Used as a control: if this is accepted and the plain dump is rejected, the
   CRC - and nothing else - is what rejected the plain dump. */
static int fake_read_bmp280_lucky_crc(void *user_ctx, uint8_t addr, uint8_t *buf, size_t len, uint32_t to)
{
    (void)user_ctx;
    (void)to;
    return fake_read_bmp280_impl(addr, buf, len, BMP280_CRC_OK);
}

static int test_scan_finds_devices(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t found[16];

    int n = crumbs_controller_scan_for_crumbs(&ctx, 0x03, 0x20, 0 /* non-strict */,
                                              fake_write, fake_read, NULL, found, sizeof(found), 10000);

    if (n < 0)
    {
        fprintf(stderr, "scan_finds_devices: scan failed rc=%d\n", n);
        return 1;
    }

    /* Expect to find both devices */
    int sawA = 0, sawB = 0;
    for (int i = 0; i < n; ++i)
    {
        if (found[i] == DEV_A)
            sawA = 1;
        if (found[i] == DEV_B)
            sawB = 1;
    }

    if (!sawA || !sawB)
    {
        fprintf(stderr, "scan_finds_devices: did not find expected devices: sawA=%d sawB=%d n=%d\n",
                sawA, sawB, n);
        return 1;
    }

    printf("  scan finds devices: PASS\n");
    return 0;
}

static int test_scan_rejects_noncrumbs(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t found[16];

    int n = crumbs_controller_scan_for_crumbs(&ctx, 0x03, 0x20, 0,
                                              fake_write, fake_read_noncrumbs, NULL, found, sizeof(found), 10000);
    if (n < 0)
    {
        fprintf(stderr, "scan_rejects_noncrumbs: scan failed rc=%d\n", n);
        return 1;
    }
    if (n != 0)
    {
        fprintf(stderr, "scan_rejects_noncrumbs: incorrectly identified non-CRUMBS device n=%d\n", n);
        return 1;
    }

    printf("  scan rejects non-CRUMBS: PASS\n");
    return 0;
}

static int run_scan_expect(const char *name, crumbs_i2c_read_fn read_fn, int expect_n)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t found[16];
    int n = crumbs_controller_scan_for_crumbs(&ctx, DEV_A, DEV_A, 0 /* non-strict */,
                                              fake_write, read_fn, NULL, found, sizeof(found), 10000);
    if (n < 0)
    {
        fprintf(stderr, "%s: scan failed rc=%d\n", name, n);
        return 1;
    }
    if (n != expect_n)
    {
        fprintf(stderr, "%s: expected %d device(s), scanner reported %d\n", name, expect_n, n);
        return 1;
    }
    printf("  %s: PASS\n", name);
    return 0;
}

static int test_scan_rejects_ezo_ascii(void)
{
    return run_scan_expect("scan rejects EZO ASCII reply (header)", fake_read_ezo, 0);
}

static int test_scan_rejects_bmp280_dump(void)
{
    return run_scan_expect("scan rejects BMP280 register dump (crc)", fake_read_bmp280, 0);
}

static int test_scan_crc_is_the_discriminator(void)
{
    /* Documents the real property: a foreign device whose bytes happen to
       carry a valid CRC is indistinguishable from a CRUMBS peripheral. */
    return run_scan_expect("scan accepts BMP280 dump with a lucky CRC (control)",
                           fake_read_bmp280_lucky_crc, 1);
}

static int test_scan_empty_range(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t found[16];

    /* Scan a range with no devices */
    int n = crumbs_controller_scan_for_crumbs(&ctx, 0x50, 0x60, 0,
                                              fake_write, fake_read, NULL, found, sizeof(found), 10000);
    if (n < 0)
    {
        fprintf(stderr, "scan_empty_range: scan failed rc=%d\n", n);
        return 1;
    }
    if (n != 0)
    {
        fprintf(stderr, "scan_empty_range: should find 0 devices, got %d\n", n);
        return 1;
    }

    printf("  scan empty range: PASS\n");
    return 0;
}

static int test_scan_with_types(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t found[16];
    uint8_t types[16];
    memset(types, 0xFF, sizeof(types));

    int n = crumbs_controller_scan_for_crumbs_with_types(&ctx, 0x03, 0x20, 0 /* non-strict */,
                                                         fake_write, fake_read, NULL,
                                                         found, types, sizeof(found), 10000);

    if (n < 0)
    {
        fprintf(stderr, "scan_with_types: scan failed rc=%d\n", n);
        return 1;
    }

    /* Expect to find both devices with correct type_ids */
    /* fake_read sets type_id = addr, so DEV_A=0x08 has type_id=0x08 */
    int sawA = 0, sawB = 0;
    uint8_t typeA = 0, typeB = 0;
    for (int i = 0; i < n; ++i)
    {
        if (found[i] == DEV_A)
        {
            sawA = 1;
            typeA = types[i];
        }
        if (found[i] == DEV_B)
        {
            sawB = 1;
            typeB = types[i];
        }
    }

    if (!sawA || !sawB)
    {
        fprintf(stderr, "scan_with_types: did not find expected devices: sawA=%d sawB=%d n=%d\n",
                sawA, sawB, n);
        return 1;
    }

    if (typeA != DEV_A)
    {
        fprintf(stderr, "scan_with_types: wrong type_id for DEV_A: got 0x%02X, expected 0x%02X\n",
                typeA, DEV_A);
        return 1;
    }

    if (typeB != DEV_B)
    {
        fprintf(stderr, "scan_with_types: wrong type_id for DEV_B: got 0x%02X, expected 0x%02X\n",
                typeB, DEV_B);
        return 1;
    }

    printf("  scan with types: PASS\n");
    return 0;
}

static int test_scan_with_types_null_types(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0);

    uint8_t found[16];

    /* types=NULL should still work (just don't return types) */
    int n = crumbs_controller_scan_for_crumbs_with_types(&ctx, 0x03, 0x20, 0,
                                                         fake_write, fake_read, NULL,
                                                         found, NULL, sizeof(found), 10000);

    if (n < 0)
    {
        fprintf(stderr, "scan_with_types_null: scan failed rc=%d\n", n);
        return 1;
    }

    if (n < 2)
    {
        fprintf(stderr, "scan_with_types_null: expected at least 2 devices, got %d\n", n);
        return 1;
    }

    printf("  scan with types (NULL types): PASS\n");
    return 0;
}

static int test_scan_null_ctx_strict(void)
{
    /* ctx=NULL should work in strict mode (no probe writes needed) */
    uint8_t found[16];

    int n = crumbs_controller_scan_for_crumbs(NULL, 0x03, 0x20, 1 /* strict */,
                                              NULL, fake_read, NULL, found, sizeof(found), 10000);

    if (n < 0)
    {
        fprintf(stderr, "scan_null_ctx_strict: scan failed rc=%d\n", n);
        return 1;
    }

    /* Should still find devices via direct read */
    if (n < 2)
    {
        fprintf(stderr, "scan_null_ctx_strict: expected 2 devices, got %d\n", n);
        return 1;
    }

    printf("  scan null ctx (strict): PASS\n");
    return 0;
}

int main(void)
{
    int failures = 0;

    printf("Running scan tests:\n");

    failures += test_scan_finds_devices();
    failures += test_scan_rejects_noncrumbs();
    failures += test_scan_rejects_ezo_ascii();
    failures += test_scan_rejects_bmp280_dump();
    failures += test_scan_crc_is_the_discriminator();
    failures += test_scan_empty_range();
    failures += test_scan_with_types();
    failures += test_scan_with_types_null_types();
    failures += test_scan_null_ctx_strict();

    if (failures == 0)
    {
        printf("OK all scan tests passed\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "FAILED %d test(s)\n", failures);
        return 1;
    }
}
