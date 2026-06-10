/*
 * Unit tests for ops-header device handle validation.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "test_common.h"
#include "crumbs_ops.h"
#include "mock_ops.h"
#include "calculator_ops.h"
#include "display_ops.h"
#include "led_ops.h"
#include "servo_ops.h"

typedef struct
{
    int write_calls;
    int read_calls;
    int delay_calls;
} ops_io_t;

typedef struct
{
    uint8_t value;
} ops_test_result_t;

static int ops_write(void *user_ctx, uint8_t addr, const uint8_t *data, size_t len)
{
    (void)addr;
    ops_io_t *io = (ops_io_t *)user_ctx;
    if (!io || (len > 0u && !data))
        return -1;
    io->write_calls++;
    return 0;
}

static int ops_read(void *user_ctx, uint8_t addr, uint8_t *buffer, size_t len, uint32_t timeout_us)
{
    (void)addr;
    (void)buffer;
    (void)len;
    (void)timeout_us;
    ops_io_t *io = (ops_io_t *)user_ctx;
    if (!io)
        return -1;
    io->read_calls++;
    return -1;
}

static void ops_delay(uint32_t us)
{
    (void)us;
}

static int ops_parse_value(const uint8_t *data, size_t len, ops_test_result_t *out)
{
    if (!data || len < 1u || !out)
        return -1;
    out->value = data[0];
    return 0;
}

CRUMBS_DEFINE_GET_OP(ops_test, value, 0x55, 0x80,
                     ops_test_result_t, ops_parse_value)
CRUMBS_DEFINE_SEND_OP(ops_test, set_value, 0x55, 0x01,
                      uint8_t value,
                      crumbs_msg_add_u8(&_m, value))
CRUMBS_DEFINE_SEND_OP_0(ops_test, reset, 0x55, 0x02)

static int test_shared_validation_helpers(void)
{
    const char *t = "shared_validation_helpers";
    crumbs_context_t ctx;
    test_init_controller(&ctx);
    ops_io_t io;
    memset(&io, 0, sizeof(io));

    crumbs_device_t send_dev = {&ctx, 0x20, ops_write, NULL, NULL, &io};
    crumbs_device_t get_dev = {&ctx, 0x20, ops_write, ops_read, ops_delay, &io};

    TEST_ASSERT_EQ(t, crumbs_ops_can_send(NULL), 0, "NULL dev should not send");
    TEST_ASSERT_EQ(t, crumbs_ops_can_send(&send_dev), 1, "send-ready dev rejected");
    TEST_ASSERT_EQ(t, crumbs_ops_can_get(&send_dev), 0, "send-only dev should not get");
    TEST_ASSERT_EQ(t, crumbs_ops_can_get(&get_dev), 1, "get-ready dev rejected");
    return 0;
}

static int test_macro_wrappers_reject_invalid_devices(void)
{
    const char *t = "macro_wrappers_reject_invalid_devices";
    crumbs_context_t ctx;
    test_init_controller(&ctx);
    ops_io_t io;
    memset(&io, 0, sizeof(io));

    crumbs_device_t missing_write = {&ctx, 0x20, NULL, ops_read, ops_delay, &io};
    crumbs_device_t send_only = {&ctx, 0x20, ops_write, NULL, NULL, &io};
    ops_test_result_t out;

    TEST_ASSERT_EQ(t, ops_test_send_set_value(NULL, 7), -1, "send accepted NULL dev");
    TEST_ASSERT_EQ(t, ops_test_send_reset(&missing_write), -1, "send accepted missing write_fn");
    TEST_ASSERT_EQ(t, ops_test_query_value(NULL), -1, "query accepted NULL dev");
    TEST_ASSERT_EQ(t, ops_test_get_value(NULL, &out), -1, "get accepted NULL dev");
    TEST_ASSERT_EQ(t, ops_test_get_value(&send_only, &out), -1, "get accepted missing read/delay");
    TEST_ASSERT_EQ(t, io.write_calls, 0, "invalid macro wrappers should not write");
    TEST_ASSERT_EQ(t, io.read_calls, 0, "invalid macro wrappers should not read");
    return 0;
}

static int test_hand_written_wrappers_reject_invalid_devices(void)
{
    const char *t = "hand_written_wrappers_reject_invalid_devices";
    crumbs_context_t ctx;
    test_init_controller(&ctx);
    ops_io_t io;
    memset(&io, 0, sizeof(io));

    crumbs_device_t missing_write = {&ctx, 0x20, NULL, ops_read, ops_delay, &io};
    crumbs_device_t send_only = {&ctx, 0x20, ops_write, NULL, NULL, &io};
    uint8_t payload[1] = {0xAA};
    uint8_t segments[4] = {1, 2, 3, 4};
    led_state_result_t led_state;
    servo_pos_result_t servo_pos;
    display_value_result_t display_value;
    calc_result_t calc_result;
    mock_status_result_t mock_status;

    TEST_ASSERT_EQ(t, led_send_set_all(NULL, 0x0F), -1, "LED send accepted NULL dev");
    TEST_ASSERT_EQ(t, led_query_state(&missing_write), -1, "LED query accepted missing write_fn");
    TEST_ASSERT_EQ(t, led_get_state(&send_only, &led_state), -1, "LED get accepted send-only dev");

    TEST_ASSERT_EQ(t, servo_send_sweep(NULL, 0, 1, 0, 180, 5), -1,
                   "servo send accepted NULL dev");
    TEST_ASSERT_EQ(t, servo_get_pos(&send_only, &servo_pos), -1,
                   "servo get accepted send-only dev");

    TEST_ASSERT_EQ(t, display_send_set_segments(NULL, segments), -1,
                   "display send accepted NULL dev");
    TEST_ASSERT_EQ(t, display_send_set_segments(&send_only, NULL), -1,
                   "display send accepted NULL segments");
    TEST_ASSERT_EQ(t, display_get_value(&send_only, &display_value), -1,
                   "display get accepted send-only dev");

    TEST_ASSERT_EQ(t, calc_send_add(NULL, 1, 2), -1, "calculator send accepted NULL dev");
    TEST_ASSERT_EQ(t, calc_get_result(&send_only, &calc_result), -1,
                   "calculator get accepted send-only dev");

    TEST_ASSERT_EQ(t, mock_send_echo(NULL, payload, sizeof(payload)), -1,
                   "mock send accepted NULL dev");
    TEST_ASSERT_EQ(t, mock_send_echo(&send_only, NULL, 1), -1,
                   "mock send accepted NULL payload");
    TEST_ASSERT_EQ(t, mock_get_status(&send_only, &mock_status), -1,
                   "mock get accepted send-only dev");

    TEST_ASSERT_EQ(t, io.write_calls, 0, "invalid hand-written wrappers should not write");
    TEST_ASSERT_EQ(t, io.read_calls, 0, "invalid hand-written wrappers should not read");
    return 0;
}

static int test_valid_send_still_uses_transport(void)
{
    const char *t = "valid_send_still_uses_transport";
    crumbs_context_t ctx;
    test_init_controller(&ctx);
    ops_io_t io;
    memset(&io, 0, sizeof(io));
    crumbs_device_t dev = {&ctx, 0x20, ops_write, NULL, NULL, &io};

    TEST_ASSERT_EQ(t, led_send_set_all(&dev, 0x0F), 0, "LED send failed");
    TEST_ASSERT_EQ(t, ops_test_send_set_value(&dev, 9), 0, "macro send failed");
    TEST_ASSERT_EQ(t, io.write_calls, 2, "valid sends did not call write");
    return 0;
}

int main(void)
{
    printf("Running ops validation tests...\n");

    if (test_shared_validation_helpers() != 0)
        return 1;
    if (test_macro_wrappers_reject_invalid_devices() != 0)
        return 1;
    if (test_hand_written_wrappers_reject_invalid_devices() != 0)
        return 1;
    if (test_valid_send_still_uses_transport() != 0)
        return 1;

    printf("PASS: ops validation tests\n");
    return 0;
}
