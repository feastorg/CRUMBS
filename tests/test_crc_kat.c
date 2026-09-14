/**
 * Known-answer tests for the CRUMBS wire checksum.
 *
 * The rest of the suite only checks encode -> decode self-consistency, which
 * any deterministic one-byte checksum would pass. These vectors pin the
 * algorithm itself: CRC-8/SMBUS (poly 0x07, init 0x00, no reflection, no
 * final XOR), whose standard check value over "123456789" is 0xF4.
 */

#include <stdio.h>
#include <string.h>
#include "crumbs_crc.h"

static int check(const char *name, const uint8_t *data, size_t len, uint8_t expected)
{
    uint8_t got = crumbs_crc8(data, len);
    if (got != expected)
    {
        fprintf(stderr, "FAIL: %s: expected 0x%02X, got 0x%02X\n", name, expected, got);
        return 1;
    }
    printf("  %s: 0x%02X PASS\n", name, got);
    return 0;
}

int main(void)
{
    int failures = 0;

    /* The standard CRC catalogue check value for CRC-8/SMBUS. */
    const uint8_t check_str[] = "123456789";
    failures += check("crc8(\"123456789\")", check_str, 9, 0xF4);

    /* Empty input: init 0x00 with no final XOR gives 0x00. */
    failures += check("crc8(<empty>)", check_str, 0, 0x00);

    /* The non-strict scan probe header (type 0, opcode 0, len 0). */
    const uint8_t probe[] = {0x00, 0x00, 0x00};
    failures += check("crc8(00 00 00)", probe, sizeof(probe), 0x00);

    /* A single non-zero byte exercises the polynomial, not just the table
       identity: 0x01 shifted through poly 0x07 gives 0x07. */
    const uint8_t one[] = {0x01};
    failures += check("crc8(01)", one, sizeof(one), 0x07);

    if (failures)
    {
        fprintf(stderr, "FAILED %d known-answer test(s)\n", failures);
        return 1;
    }
    printf("PASS: CRC-8 known-answer vectors\n");
    return 0;
}
