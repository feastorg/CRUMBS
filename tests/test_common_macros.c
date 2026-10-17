/*
 * Tests for the assertion macros in test_common.h: a failing assertion
 * evaluates each argument once, and reports the value that failed. Each
 * case makes one assertion fail on purpose, so its message on stderr is
 * expected output.
 */

#include <stdio.h>

#include "test_common.h"

static int g_calls;
static int g_dummy;

static int next_int(void) { return ++g_calls; }
static size_t next_size(void) { return (size_t)++g_calls; }
static void *next_ptr(void)
{
    ++g_calls;
    return &g_dummy;
}

static int fail_eq(void)
{
    TEST_ASSERT_EQ("fail_eq", next_int(), 0, "expected failure");
    return 0;
}

static int fail_eq_expected(void)
{
    TEST_ASSERT_EQ("fail_eq_expected", 0, next_int(), "expected failure");
    return 0;
}

static int fail_size_eq(void)
{
    TEST_ASSERT_SIZE_EQ("fail_size_eq", next_size(), 0, "expected failure");
    return 0;
}

static int fail_null(void)
{
    TEST_ASSERT_NULL("fail_null", next_ptr(), "expected failure");
    return 0;
}

static int test_failing_assertions_evaluate_once(void)
{
    const char *t = "failing_assertions_evaluate_once";

    g_calls = 0;
    TEST_ASSERT(t, fail_eq() == 1, "TEST_ASSERT_EQ fails");
    TEST_ASSERT(t, g_calls == 1, "TEST_ASSERT_EQ evaluates actual once");

    g_calls = 0;
    TEST_ASSERT(t, fail_eq_expected() == 1, "TEST_ASSERT_EQ fails");
    TEST_ASSERT(t, g_calls == 1, "TEST_ASSERT_EQ evaluates expected once");

    g_calls = 0;
    TEST_ASSERT(t, fail_size_eq() == 1, "TEST_ASSERT_SIZE_EQ fails");
    TEST_ASSERT(t, g_calls == 1, "TEST_ASSERT_SIZE_EQ evaluates actual once");

    g_calls = 0;
    TEST_ASSERT(t, fail_null() == 1, "TEST_ASSERT_NULL fails");
    TEST_ASSERT(t, g_calls == 1, "TEST_ASSERT_NULL evaluates ptr once");

    printf("  %s: PASS\n", t);
    return 0;
}

int main(void)
{
    int failures = 0;
    printf("test_common_macros:\n");
    failures += test_failing_assertions_evaluate_once();
    if (failures)
    {
        fprintf(stderr, "test_common_macros: %d failed\n", failures);
        return 1;
    }
    printf("test_common_macros: all passed\n");
    return 0;
}
