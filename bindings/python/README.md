# crumbs-i2c

[CRUMBS](https://github.com/feastorg/CRUMBS) for Python: the frame, its CRC,
and a controller that talks to CRUMBS peripherals over Linux I2C. Pure
Python, no dependencies, Python 3.11 or newer.

```sh
pip install crumbs-i2c
```

```python
from crumbs_i2c import Controller, LinuxBus, Message

with LinuxBus("/dev/i2c-1") as bus:
    crumbs = Controller(bus)

    # A SET: one frame written, nothing read back.
    crumbs.send(0x10, Message(type_id=0x01, opcode=0x02, data=b"\x01"))

    # A GET: SET_REPLY names the reply, then it is read and checked.
    reply = crumbs.query(0x10, type_id=0x01, opcode=0x80)
    print(reply.data)

    # Every address in 0x08-0x77 that answers with a CRUMBS frame, by type.
    print(crumbs.scan())
```

The payload is opaque here: a device family's encoders and parsers, such as
[bread-crumbs-contracts](https://pypi.org/project/bread-crumbs-contracts/),
build and read `data`.

## What it does

- `encode`, `decode` and `frame_length` follow the C library exactly; the
  tests check them against vectors the C code writes
  (`tests/golden_vectors/`).
- `Controller.query` sends SET_REPLY with the wildcard type, waits 10 ms as
  the C getters do, reads a full 31-byte frame and trims it to the length its
  header declares. A reply that does not decode is read again, up to three
  reads: a Raspberry Pi's I2C controller ignores clock stretching, so a reply
  the peripheral is still building can read as `0xFF` bytes. A valid reply for
  another opcode or device type raises `ReplyMismatch` and is not retried.
- `Controller.scan` reads each address once and writes nothing.
- `LinuxBus` uses `/dev/i2c-N` like the C library's Linux HAL: the
  `I2C_SLAVE` ioctl, then a plain write or read. Anything with the same
  `write(address, data)` and `read(address, count)` methods works as a bus.

The package version is the CRUMBS library version it matches.

## Licence

AGPL-3.0-or-later, like CRUMBS.
