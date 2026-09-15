/* A scripted TwoWire. As a controller it talks to per-address devices that
 * ACK or not, clock out a scripted reply (then 0xFF padding, as the AVR
 * target does past its buffer) and record what was written. As a
 * peripheral, fake_wire_deliver() and fake_wire_request() play the
 * controller's side and drive the callbacks the HAL registered. Every bus
 * operation lands in a trace ("S10 W5 P1 Q10:31 ...": start at 0x10, write 5, stop, request 31) so a test can assert
 * the transaction shape. */
#ifndef FAKE_WIRE_H
#define FAKE_WIRE_H

#include <stddef.h>
#include <stdint.h>

#define BUFFER_LENGTH 32 /* the AVR Wire buffer; the HAL reads it */
#define FAKE_WIRE_MAX_DEVICES 8
#define FAKE_WIRE_BUF 64

struct fake_wire_device_t
{
    uint8_t addr;
    uint8_t reply[FAKE_WIRE_BUF];
    size_t reply_len;
    size_t reply_pos;
    size_t deliver_max; /* most bytes one requestFrom() yields; 0 = no limit */
    uint8_t written[FAKE_WIRE_BUF];
    size_t written_len;
    size_t writes;     /* transmissions that carried bytes */
};

class TwoWire
{
public:
    void begin();
    void begin(uint8_t address);
    void setClock(uint32_t frequency);
    void onReceive(void (*fn)(int));
    void onRequest(void (*fn)());

    void beginTransmission(uint8_t address);
    void beginTransmission(int address);
    size_t write(uint8_t data);
    size_t write(const uint8_t *data, size_t quantity);
    uint8_t endTransmission();
    uint8_t endTransmission(uint8_t sendStop);

    uint8_t requestFrom(uint8_t address, uint8_t quantity);
    uint8_t requestFrom(int address, int quantity);
    int available();
    int read();

    /* What the HAL configured. */
    int begun;            /* begin() calls */
    int begun_as_target;  /* begin(address) calls */
    uint8_t target_address;
    uint32_t clock;
    void (*receive_fn)(int);
    void (*request_fn)();

    /* Scripting. */
    size_t write_returns; /* if non-zero, write(buf, n) reports this many (the
                             AVR core always reports n; other cores report what
                             fit in the buffer) */

    char trace[4096];

    /* Buffers, public so a test can inspect the peripheral's reply. */
    uint8_t tx[FAKE_WIRE_BUF];
    size_t tx_len;
    uint8_t rx[FAKE_WIRE_BUF];
    size_t rx_len;
    size_t rx_pos;
    uint8_t tx_addr;

    fake_wire_device_t dev[FAKE_WIRE_MAX_DEVICES];
    size_t ndev;

    fake_wire_device_t *find(int addr);
};

extern TwoWire Wire;

void fake_wire_reset(void);
fake_wire_device_t *fake_wire_add_device(uint8_t addr);
void fake_wire_set_reply(fake_wire_device_t *d, const uint8_t *bytes, size_t len);

/* The controller writes @p len bytes to this peripheral: fills the receive
   buffer and runs the registered onReceive handler. */
void fake_wire_deliver(const uint8_t *bytes, size_t len);

/* The controller reads from this peripheral: runs the registered onRequest
   handler and returns how many bytes it queued (in Wire.tx). */
size_t fake_wire_request(void);

#endif /* FAKE_WIRE_H */
