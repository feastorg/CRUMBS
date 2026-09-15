/* See Arduino.h and Wire.h in this directory. */
#include "Arduino.h"
#include "Wire.h"

#include <stdio.h>
#include <string.h>

/* ---- clock --------------------------------------------------------------- */

static unsigned long g_now_us;

unsigned long millis(void) { return g_now_us / 1000UL; }
unsigned long micros(void) { return g_now_us; }
void delayMicroseconds(unsigned int us) { g_now_us += us; }
void fake_arduino_reset_clock(void) { g_now_us = 0; }
void fake_arduino_advance_us(unsigned long us) { g_now_us += us; }

/* ---- bus ----------------------------------------------------------------- */

TwoWire Wire;

static void trace_item(const char *item)
{
    strncat(Wire.trace, item, sizeof Wire.trace - strlen(Wire.trace) - 1);
}

void fake_wire_reset(void)
{
    memset(&Wire, 0, sizeof Wire);
    fake_arduino_reset_clock();
}

fake_wire_device_t *fake_wire_add_device(uint8_t addr)
{
    fake_wire_device_t *d = &Wire.dev[Wire.ndev++];
    memset(d, 0, sizeof *d);
    d->addr = addr;
    return d;
}

void fake_wire_set_reply(fake_wire_device_t *d, const uint8_t *bytes, size_t len)
{
    memcpy(d->reply, bytes, len);
    d->reply_len = len;
    d->reply_pos = 0;
}

fake_wire_device_t *TwoWire::find(int addr)
{
    for (size_t i = 0; i < ndev; ++i)
        if (dev[i].addr == addr)
            return &dev[i];
    return NULL;
}

void TwoWire::begin() { begun++; }

void TwoWire::begin(uint8_t address)
{
    begun_as_target++;
    target_address = address;
}

void TwoWire::setClock(uint32_t frequency) { clock = frequency; }
void TwoWire::onReceive(void (*fn)(int)) { receive_fn = fn; }
void TwoWire::onRequest(void (*fn)()) { request_fn = fn; }

void TwoWire::beginTransmission(uint8_t address)
{
    char item[16];
    snprintf(item, sizeof item, "S%02X ", address);
    trace_item(item);
    tx_addr = address;
    tx_len = 0;
    in_transmission = 1;
}

void TwoWire::beginTransmission(int address) { beginTransmission((uint8_t)address); }

size_t TwoWire::write(uint8_t data) { return write(&data, 1); }

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    char item[16];
    snprintf(item, sizeof item, "W%zu ", quantity);
    trace_item(item);
    if (quantity > sizeof tx - tx_len)
        quantity = sizeof tx - tx_len; /* the real buffer is BUFFER_LENGTH; the HAL checks that itself */
    memcpy(tx + tx_len, data, quantity);
    tx_len += quantity;
    return write_returns ? write_returns : quantity;
}

uint8_t TwoWire::endTransmission() { return endTransmission(1); }

/* Wire's codes: 0 ok, 2 address NACK. */
uint8_t TwoWire::endTransmission(uint8_t sendStop)
{
    char item[16];
    fake_wire_device_t *d;
    snprintf(item, sizeof item, "P%d ", sendStop ? 1 : 0);
    trace_item(item);
    in_transmission = 0;
    if (end_returns)
        return end_returns;
    d = find(tx_addr);
    if (!d)
        return 2;
    d->addressed++;
    if (tx_len)
    {
        size_t n = tx_len;
        if (n > sizeof d->written - d->written_len)
            n = sizeof d->written - d->written_len;
        memcpy(d->written + d->written_len, tx, n);
        d->written_len += n;
        d->writes++;
    }
    tx_len = 0;
    return 0;
}

/* The AVR core clocks exactly @p quantity bytes from a target that ACKs, and
   reports 0 for one that does not. */
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity)
{
    char item[16];
    fake_wire_device_t *d;
    size_t left, n;
    snprintf(item, sizeof item, "Q%02X:%u ", address, (unsigned)quantity);
    trace_item(item);
    rx_len = 0;
    rx_pos = 0;
    d = find(address);
    if (!d || (d->reply_after_write && d->writes == 0))
        return 0;
    if (quantity > BUFFER_LENGTH)
        quantity = BUFFER_LENGTH;
    n = quantity;
    if (d->deliver_max && n > d->deliver_max)
        n = d->deliver_max;
    left = d->reply_len - d->reply_pos;
    if (left > n)
        left = n;
    memcpy(rx, d->reply + d->reply_pos, left);
    d->reply_pos += left;
    memset(rx + left, 0xFF, n - left);
    rx_len = n;
    return (uint8_t)n;
}

uint8_t TwoWire::requestFrom(int address, int quantity)
{
    return requestFrom((uint8_t)address, (uint8_t)quantity);
}

int TwoWire::available() { return (int)(rx_len - rx_pos); }

int TwoWire::read() { return rx_pos < rx_len ? rx[rx_pos++] : -1; }

/* ---- the controller's side of a peripheral test ------------------------- */

void fake_wire_deliver(const uint8_t *bytes, size_t len)
{
    if (len > sizeof Wire.rx)
        len = sizeof Wire.rx;
    memcpy(Wire.rx, bytes, len);
    Wire.rx_len = len;
    Wire.rx_pos = 0;
    if (Wire.receive_fn)
        Wire.receive_fn((int)len);
}

size_t fake_wire_request(void)
{
    Wire.tx_len = 0;
    if (Wire.request_fn)
        Wire.request_fn();
    return Wire.tx_len;
}
