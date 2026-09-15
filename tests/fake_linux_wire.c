/* See fake_linux_wire.h. Return values and errno follow linux-wire 0.1.3. */
#include "fake_linux_wire.h"

#include <errno.h>
#include <linux_wire.h>
#include <stdio.h>
#include <string.h>

fake_lw_bus_t fake_lw;

static const int k_fake_fd = 42;

void fake_lw_reset(void)
{
    memset(&fake_lw, 0, sizeof fake_lw);
    fake_lw.write_returns = -1;
    fake_lw.target = -1;
}

fake_lw_device_t *fake_lw_add_device(uint8_t addr)
{
    fake_lw_device_t *d = &fake_lw.dev[fake_lw.ndev++];
    memset(d, 0, sizeof *d);
    d->addr = addr;
    d->pad_reads = 1;
    d->pad_byte = 0xFF;
    return d;
}

void fake_lw_set_reply(fake_lw_device_t *d, const uint8_t *bytes, size_t len)
{
    memcpy(d->reply, bytes, len);
    d->reply_len = len;
    d->reply_pos = 0;
}

static fake_lw_device_t *find_dev(int addr)
{
    for (size_t i = 0; i < fake_lw.ndev; ++i)
        if (fake_lw.dev[i].addr == addr)
            return &fake_lw.dev[i];
    return NULL;
}

static void trace_item(const char *item)
{
    strncat(fake_lw.trace, item, sizeof fake_lw.trace - strlen(fake_lw.trace) - 1);
}

static void trace_addr(char op, int addr)
{
    char item[16];
    snprintf(item, sizeof item, "%c%02X ", op, addr);
    trace_item(item);
}

static void trace_len(char op, size_t len)
{
    char item[24];
    snprintf(item, sizeof item, "%c%zu ", op, len);
    trace_item(item);
}

/* ---- linux-wire API ---------------------------------------------------- */

int lw_open_bus(lw_i2c_bus *bus, const char *device_path)
{
    if (!bus || !device_path || strncmp(device_path, "/dev/i2c-", 9) != 0)
    {
        errno = EINVAL;
        return -1;
    }
    memset(bus, 0, sizeof *bus);
    bus->fd = -1;
    if (fake_lw.open_fails)
    {
        errno = ENOENT;
        return -1;
    }
    strncpy(bus->device_path, device_path, sizeof bus->device_path - 1);
    bus->fd = k_fake_fd;
    bus->log_errors = 1;
    return 0;
}

void lw_close_bus(lw_i2c_bus *bus)
{
    if (bus)
        bus->fd = -1;
}

int lw_set_target(lw_i2c_bus *bus, uint8_t addr)
{
    fake_lw_device_t *d;
    trace_addr('T', addr);
    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    d = find_dev(addr);
    if (d && d->driver_owned)
    {
        errno = EBUSY; /* I2C_SLAVE refused: a kernel driver has it */
        return -1;
    }
    fake_lw.target = addr; /* selecting an absent address puts nothing on the bus */
    return 0;
}

int lw_probe(lw_i2c_bus *bus, uint8_t addr)
{
    fake_lw_device_t *d;
    trace_addr('P', addr);
    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    if (fake_lw.quick_unsupported)
    {
        errno = EOPNOTSUPP;
        return -1;
    }
    d = find_dev(addr);
    if (!d)
    {
        errno = EREMOTEIO; /* NACK */
        return -1;
    }
    return d->driver_owned ? 1 : 0;
}

ssize_t lw_write(lw_i2c_bus *bus, const uint8_t *data, size_t len, int send_stop)
{
    fake_lw_device_t *d;
    (void)send_stop; /* linux-wire always ends a write() with STOP */
    trace_len('W', len);
    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    d = find_dev(fake_lw.target);
    if (!d)
    {
        errno = EREMOTEIO;
        return -1;
    }
    if (len > FAKE_LW_BUF - d->written_len)
        len = FAKE_LW_BUF - d->written_len;
    memcpy(d->written + d->written_len, data, len);
    d->written_len += len;
    d->writes++;
    return fake_lw.write_returns >= 0 ? fake_lw.write_returns : (ssize_t)len;
}

/* Serve one read against a device: the scripted reply first, then padding
   or end-of-data. A read() honours the per-call chunk limit and the pad
   setting; an I2C_RDWR transfer (whole) always fills the count asked for. */
static ssize_t serve_read(fake_lw_device_t *d, uint8_t *buf, size_t len, int whole)
{
    size_t left = d->reply_len - d->reply_pos;
    size_t n;

    if (fake_lw.read_errno)
    {
        errno = fake_lw.read_errno;
        return -1;
    }
    if (d->reply_after_write && d->writes == 0)
    {
        errno = EREMOTEIO;
        return -1;
    }
    if (!whole && d->read_chunk && len > d->read_chunk)
        len = d->read_chunk;

    n = left < len ? left : len;
    memcpy(buf, d->reply + d->reply_pos, n);
    d->reply_pos += n;
    if (n < len && (whole || d->pad_reads))
    {
        memset(buf + n, d->pad_byte, len - n);
        n = len;
    }
    return (ssize_t)n;
}

ssize_t lw_read(lw_i2c_bus *bus, uint8_t *buf, size_t len)
{
    fake_lw_device_t *d;
    trace_len('R', len);
    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    d = find_dev(fake_lw.target);
    if (!d)
    {
        errno = EREMOTEIO;
        return -1;
    }
    return serve_read(d, buf, len, 0);
}

ssize_t lw_ioctl_read(lw_i2c_bus *bus, uint16_t addr, const uint8_t *iaddr, size_t iaddr_len,
                      uint8_t *data, size_t len, uint16_t flags)
{
    fake_lw_device_t *d;
    ssize_t n;
    (void)flags;
    trace_addr('I', addr);
    if (bus->fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    if (!data || len == 0 || (iaddr_len > 0 && !iaddr))
    {
        errno = EINVAL;
        return -1;
    }
    d = find_dev(addr);
    if (!d || d->driver_owned)
    {
        errno = d ? EBUSY : EREMOTEIO;
        return -1;
    }
    if (iaddr_len > 0)
    {
        if (iaddr_len > FAKE_LW_BUF - d->written_len)
            iaddr_len = FAKE_LW_BUF - d->written_len;
        memcpy(d->written + d->written_len, iaddr, iaddr_len);
        d->written_len += iaddr_len;
        d->writes++;
    }
    n = serve_read(d, data, len, 1);
    return n < 0 ? -1 : n;
}

int lw_set_timeout(lw_i2c_bus *bus, uint32_t timeout_us)
{
    if (!bus)
    {
        errno = EINVAL;
        return -1;
    }
    bus->timeout_us = timeout_us;
    return 0;
}

void lw_set_error_logging(lw_i2c_bus *bus, int enable)
{
    if (bus)
        bus->log_errors = enable ? 1 : 0;
}
