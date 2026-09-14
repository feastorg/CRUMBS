/* crumbs_linux_close() must leave a handle that a second close (or any HAL
 * call) treats as closed. A zeroed handle has fd == 0, which is stdin. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "crumbs_linux.h"
#include "test_common.h"

static int stdin_open(void)
{
    return fcntl(0, F_GETFD) != -1;
}

int main(void)
{
    crumbs_linux_i2c_t i2c;
    uint8_t byte = 0;

    memset(&i2c, 0, sizeof i2c);
    i2c.bus.fd = open("/dev/null", O_RDWR); /* stands in for an opened bus */
    TEST_ASSERT("linux_close", i2c.bus.fd > 0, "test needs a descriptor above stdin");
    TEST_ASSERT("linux_close", stdin_open(), "stdin open before the test");

    crumbs_linux_close(&i2c);
    TEST_ASSERT_EQ("linux_close", i2c.bus.fd, -1, "closed handle reports fd -1");

    crumbs_linux_close(&i2c);
    TEST_ASSERT("linux_close", stdin_open(), "second close leaves stdin alone");

    TEST_ASSERT_EQ("linux_close", crumbs_linux_read(&i2c, 0x10, &byte, 1, 0), -1,
                   "read on a closed handle is refused");
    TEST_ASSERT("linux_close", stdin_open(), "refused read leaves stdin alone");

    crumbs_linux_close(NULL);
    printf("test_linux_close: OK\n");
    return 0;
}
