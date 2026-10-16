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
  the C getters do, reads a full 31-byte frame, trims it to the length its
  header declares, and checks the reply's opcode and type. A GET is safe to
  repeat, so the whole exchange is tried again, up to three times in all,
  when:
  - the reply is corrupt (with a Raspberry Pi as controller, the bus can
    lose a clock edge around the peripheral's clock stretch unless it runs
    slowly, so a reply reads correctly up to some byte, then as `0xFF`
    bytes; see
    [When the reply is built](https://github.com/feastorg/CRUMBS/blob/main/docs/protocol.md#when-the-reply-is-built));
  - it is the same device's reply to another opcode (a lost SET_REPLY, or a
    device that restarted);
  - the device did not answer (`ENXIO`, `EREMOTEIO`, `EIO`, `ETIMEDOUT`,
    `EAGAIN`).

  Another device type at the address raises `ReplyMismatch` at once, and any
  other bus error is raised as the `OSError` it is. `send` is never retried,
  since a SET need not be safe to repeat.
- `Controller.send_confirmed` sends a SET and confirms it from the device's
  acknowledgement
  ([GET_ACK](https://github.com/feastorg/CRUMBS/blob/main/docs/protocol.md#get_ack-0xfd-the-set-acknowledgement)):
  it reads the acknowledgement (`Controller.read_ack`), sends, reads it
  again, and returns a `SetOutcome` (`APPLIED`, `REFUSED`, `REJECTED` or
  `NOT_ADVANCED`) with the last `Ack`. A frame that did not arrive (all
  four acknowledgement bytes equal to the baseline, status not none) is sent
  again, up to three times in all; an acknowledgement that cannot be read
  after a send is raised rather than answered with a re-send, so a frame is
  never applied twice because a read failed. `AckMismatch` means another
  command was counted in between; `AckUnknown` that the acknowledgement
  cannot say, because the device restarted or its counter came round, so the
  SET is not sent again. The first SET to a device that has counted nothing
  since it started is never re-sent. A device built with CRUMBS before
  0.16.0 has no acknowledgement and `send_confirmed` does not check for one:
  its version reply reports a `CRUMBS_VERSION` below `ACK_MIN_VERSION`, so
  check that and send to it with `send`, unconfirmed. `read_ack` usually
  raises `ReplyMismatch` for such a device, but one that echoes the opcode
  with bytes that happen to form a valid record is not caught. `decode_ack` and `classify` are the same steps for a
  caller with its own transport.
- `Controller.scan` reads each address once and writes nothing. An address
  with no device, or one that answers with something that is not a frame, is
  skipped; any other bus error is raised.
- A `Controller` may be shared between threads: each exchange holds its lock,
  so one thread's GET is never interleaved with another's.
- Protocol errors share the base `CrumbsError` (`FrameError`,
  `ReplyMismatch`, `AckMismatch`, `AckUnknown`); bus errors are `OSError`.
- `LinuxBus` uses `/dev/i2c-N` like the C library's Linux HAL: the
  `I2C_SLAVE` ioctl, then a plain write or read. Anything with the same
  `write(address, data)` and `read(address, count)` methods works as a bus.

The package version is the CRUMBS library version it matches.

## Licence

AGPL-3.0-or-later, like CRUMBS.
