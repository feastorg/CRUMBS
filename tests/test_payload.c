/* CRUMBS_DEFINE_PAYLOAD and CRUMBS_DEFINE_FAMILY: the generated codec round
 * trips every field type through an encoded frame, refuses short buffers,
 * accepts extended ones, and the family enum carries what was declared.
 * The compile-time checks are exercised by tests/compile_fail/. Written in
 * the C/C++ common subset: CMake also builds it as C++. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "crumbs_ops.h"
#include "test_common.h"

#define TEST_OPS(X)                 \
    X(TEST_OP_SET_ALL, 0x01)        \
    X(TEST_OP_GET_VERSION, 0x00)    \
    X(TEST_OP_SET_PAIR, 0x02)       \
    X(TEST_OP_GET_ALL, 0x80)        \
    X(TEST_OP_LAST, 0xFF)
CRUMBS_DEFINE_FAMILY(TEST, 0x5A, TEST_OPS)

#define ALL_FIELDS(X) \
    X(u8, a)          \
    X(u16, b)         \
    X(u32, c)         \
    X(i8, d)          \
    X(i16, e)         \
    X(i32, f)         \
    X(float, g)
CRUMBS_DEFINE_PAYLOAD(test_all, 18, ALL_FIELDS)

#define PAIR_FIELDS(X) X(u8, ch) X(i16, limit)
CRUMBS_DEFINE_PAYLOAD(test_pair, 3, PAIR_FIELDS)

#define FULL_FIELDS(X) \
    X(u32, w0) X(u32, w1) X(u32, w2) X(u32, w3) X(u32, w4) X(u32, w5) X(u16, h) X(u8, b)
CRUMBS_DEFINE_PAYLOAD(test_full, 27, FULL_FIELDS)

static int test_family(void)
{
    const char *t = "family";
    TEST_ASSERT_EQ(t, TEST_TYPE_ID, 0x5A, "type id");
    TEST_ASSERT_EQ(t, TEST_OP_SET_ALL, 0x01, "first opcode");
    TEST_ASSERT_EQ(t, TEST_OP_GET_VERSION, 0x00, "opcode 0x00 is allowed");
    TEST_ASSERT_EQ(t, TEST_OP_GET_ALL, 0x80, "a GET opcode");
    TEST_ASSERT_EQ(t, TEST_OP_LAST, 0xFF, "0xFF is allowed");
    TEST_ASSERT_EQ(t, (int)sizeof(uint8_t), (int)sizeof((uint8_t)TEST_OP_GET_ALL), "usable as a byte");
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_round_trip_all_types(void)
{
    const char *t = "round_trip";
    crumbs_message_t m, back;
    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    test_all_t in, out;
    size_t n;

    in.a = 0xA5;
    in.b = 0xBEEF;
    in.c = 0xDEADBEEFu;
    in.d = -7;
    in.e = -1234;
    in.f = -123456789;
    in.g = 1.5f;
    memset(&out, 0xAA, sizeof out);

    TEST_ASSERT_EQ(t, test_all_wire_size, 18, "wire size");
    crumbs_msg_init(&m, TEST_TYPE_ID, TEST_OP_SET_ALL);
    TEST_ASSERT_EQ(t, test_all_pack(&m, &in), 0, "pack");
    TEST_ASSERT_EQ(t, m.data_len, 18, "pack advanced data_len");
    TEST_ASSERT_EQ(t, m.data[0], 0xA5, "u8 on the wire");
    TEST_ASSERT(t, m.data[1] == 0xEF && m.data[2] == 0xBE, "u16 little-endian");
    TEST_ASSERT(t, m.data[3] == 0xEF && m.data[4] == 0xBE && m.data[5] == 0xAD && m.data[6] == 0xDE,
                "u32 little-endian");
    TEST_ASSERT_EQ(t, (int8_t)m.data[7], -7, "i8 two's complement");

    n = test_encode(&m, frame);
    TEST_ASSERT_SIZE_EQ(t, n, 22, "frame is header + 18 + crc");
    TEST_ASSERT_EQ(t, crumbs_decode_message(frame, n, &back, NULL), 0, "decodes");
    TEST_ASSERT_EQ(t, test_all_unpack(back.data, back.data_len, &out), 0, "unpack");
    TEST_ASSERT_EQ(t, out.a, in.a, "u8");
    TEST_ASSERT_EQ(t, out.b, in.b, "u16");
    TEST_ASSERT(t, out.c == in.c, "u32");
    TEST_ASSERT_EQ(t, out.d, in.d, "i8");
    TEST_ASSERT_EQ(t, out.e, in.e, "i16");
    TEST_ASSERT(t, out.f == in.f, "i32");
    TEST_ASSERT(t, out.g == in.g, "float");

    /* The generated codec agrees with the hand helpers byte for byte. */
    {
        crumbs_message_t h;
        crumbs_msg_init(&h, TEST_TYPE_ID, TEST_OP_SET_ALL);
        crumbs_msg_add_u8(&h, in.a);
        crumbs_msg_add_u16(&h, in.b);
        crumbs_msg_add_u32(&h, in.c);
        crumbs_msg_add_i8(&h, in.d);
        crumbs_msg_add_i16(&h, in.e);
        crumbs_msg_add_i32(&h, in.f);
        crumbs_msg_add_float(&h, in.g);
        TEST_ASSERT_EQ(t, h.data_len, m.data_len, "same length as crumbs_msg_add_*");
        TEST_ASSERT(t, memcmp(h.data, m.data, m.data_len) == 0, "same bytes as crumbs_msg_add_*");
    }
    printf("  %s: PASS\n", t);
    return 0;
}

static int test_lengths(void)
{
    const char *t = "lengths";
    crumbs_message_t m;
    test_pair_t in, out;
    test_full_t full;

    in.ch = 3;
    in.limit = -500;
    crumbs_msg_init(&m, TEST_TYPE_ID, TEST_OP_SET_PAIR);
    TEST_ASSERT_EQ(t, test_pair_pack(&m, &in), 0, "pack");
    TEST_ASSERT_EQ(t, m.data_len, 3, "three bytes");

    TEST_ASSERT_EQ(t, test_pair_unpack(m.data, 2, &out), -1, "one byte short is refused");
    TEST_ASSERT_EQ(t, test_pair_unpack(m.data, 3, &out), 0, "exact length");
    TEST_ASSERT_EQ(t, out.ch, 3, "ch");
    TEST_ASSERT_EQ(t, out.limit, -500, "limit");
    m.data[3] = 0x99; /* a later revision appended a byte */
    TEST_ASSERT_EQ(t, test_pair_unpack(m.data, 4, &out), 0, "trailing bytes are accepted");
    TEST_ASSERT_EQ(t, out.limit, -500, "and ignored");

    /* Pack appends after what is already there and refuses to overflow. */
    crumbs_msg_init(&m, TEST_TYPE_ID, TEST_OP_SET_PAIR);
    crumbs_msg_add_u8(&m, 0x11);
    TEST_ASSERT_EQ(t, test_pair_pack(&m, &in), 0, "pack after a byte");
    TEST_ASSERT_EQ(t, m.data_len, 4, "appended");
    TEST_ASSERT_EQ(t, m.data[1], 3, "ch after the existing byte");
    m.data_len = 25;
    TEST_ASSERT_EQ(t, test_pair_pack(&m, &in), -1, "25 + 3 does not fit");
    TEST_ASSERT_EQ(t, m.data_len, 25, "refused pack leaves data_len alone");

    /* A 27-byte payload fills the frame exactly. */
    memset(&full, 0, sizeof full);
    full.b = 0x42;
    TEST_ASSERT_EQ(t, test_full_wire_size, 27, "27-byte layout");
    crumbs_msg_init(&m, TEST_TYPE_ID, TEST_OP_SET_ALL);
    TEST_ASSERT_EQ(t, test_full_pack(&m, &full), 0, "pack 27");
    TEST_ASSERT_EQ(t, m.data_len, 27, "full payload");
    TEST_ASSERT_EQ(t, m.data[26], 0x42, "last byte");

    TEST_ASSERT_EQ(t, test_pair_pack(NULL, &in), -1, "NULL msg");
    TEST_ASSERT_EQ(t, test_pair_pack(&m, NULL), -1, "NULL value");
    TEST_ASSERT_EQ(t, test_pair_unpack(NULL, 3, &out), -1, "NULL data");
    TEST_ASSERT_EQ(t, test_pair_unpack(m.data, 3, NULL), -1, "NULL out");
    printf("  %s: PASS\n", t);
    return 0;
}

/* The generated functions have the shapes the wrapper macros take. */
static int fake_write(void *io, uint8_t addr, const uint8_t *data, size_t len)
{
    crumbs_message_t *captured = (crumbs_message_t *)io;
    (void)addr;
    return crumbs_decode_message(data, len, captured, NULL);
}

CRUMBS_DEFINE_SEND_OP(test, pair, TEST_TYPE_ID, TEST_OP_SET_PAIR,
                      const test_pair_t *v, test_pair_pack(&_m, v))
CRUMBS_DEFINE_GET_OP(test, all, TEST_TYPE_ID, TEST_OP_GET_ALL, test_all_t, test_all_unpack)

static int test_wrappers_take_the_codec(void)
{
    const char *t = "wrappers";
    crumbs_context_t ctx;
    crumbs_device_t dev;
    crumbs_message_t captured;
    test_pair_t in, out;

    test_init_controller(&ctx);
    memset(&dev, 0, sizeof dev);
    dev.ctx = &ctx;
    dev.addr = 0x10;
    dev.write_fn = fake_write;
    dev.io = &captured;
    in.ch = 1;
    in.limit = 250;
    TEST_ASSERT_EQ(t, test_send_pair(&dev, &in), 0, "send through the wrapper");
    TEST_ASSERT_EQ(t, captured.type_id, TEST_TYPE_ID, "type on the wire");
    TEST_ASSERT_EQ(t, captured.opcode, TEST_OP_SET_PAIR, "opcode on the wire");
    TEST_ASSERT_EQ(t, test_pair_unpack(captured.data, captured.data_len, &out), 0, "unpack what was sent");
    TEST_ASSERT_EQ(t, out.limit, 250, "field survived");
    TEST_ASSERT_EQ(t, test_get_all(&dev, NULL), -1, "get with NULL out (compiles against unpack)");
    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("test_payload:\n");
    failures += test_family();
    failures += test_round_trip_all_types();
    failures += test_lengths();
    failures += test_wrappers_take_the_codec();
    if (failures)
    {
        fprintf(stderr, "test_payload: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_payload: OK\n");
    return 0;
}
