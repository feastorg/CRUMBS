/* A scripted stand-in for the nine linux-wire calls the Linux HAL makes.
 *
 * One bus, up to FAKE_LW_MAX_DEVICES addresses. Each device says whether it
 * ACKs, whether a kernel driver owns it, what it clocks out on a read and
 * how, and records what was written to it. The bus keeps a trace of every
 * call ("T10 W4 R31 ...") so a test can assert the shape of a transaction,
 * not just its result. */
#ifndef FAKE_LINUX_WIRE_H
#define FAKE_LINUX_WIRE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define FAKE_LW_MAX_DEVICES 8
#define FAKE_LW_BUF 64

typedef struct
{
    uint8_t addr;
    int driver_owned;   /* selecting it fails with EBUSY; lw_probe() reports 1 */
    int reply_after_write; /* reads NACK until the device has been written to */

    uint8_t reply[FAKE_LW_BUF]; /* bytes clocked out on reads, in order */
    size_t reply_len;
    size_t reply_pos;
    size_t read_chunk;  /* most bytes one lw_read() returns; 0 = no limit */
    int pad_reads;      /* 1: past the reply, fill with pad_byte to the count
                           asked for (what i2c-dev does); 0: return 0 */
    uint8_t pad_byte;

    uint8_t written[FAKE_LW_BUF]; /* everything written, in order */
    size_t written_len;
    size_t writes;
} fake_lw_device_t;

typedef struct
{
    int open_fails;        /* lw_open_bus() fails with ENOENT */
    int quick_unsupported; /* lw_probe() fails with EOPNOTSUPP */
    ssize_t write_returns; /* -1: lw_write() returns len; else this */
    int read_errno;        /* non-zero: lw_read() fails with it */

    fake_lw_device_t dev[FAKE_LW_MAX_DEVICES];
    size_t ndev;
    int target; /* last lw_set_target() address, -1 before any */
    size_t logged_calls; /* bus calls made while bus->log_errors was on */

    char trace[4096]; /* "T10 W4 R31 P11 I10 " ... */
} fake_lw_bus_t;

extern fake_lw_bus_t fake_lw;

void fake_lw_reset(void);
fake_lw_device_t *fake_lw_add_device(uint8_t addr);
void fake_lw_set_reply(fake_lw_device_t *d, const uint8_t *bytes, size_t len);

#endif /* FAKE_LINUX_WIRE_H */
