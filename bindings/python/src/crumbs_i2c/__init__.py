"""CRUMBS for Python: the frame, its CRC, and a controller over Linux I2C.

    from crumbs_i2c import Controller, LinuxBus, Message

    with LinuxBus("/dev/i2c-1") as bus:
        crumbs = Controller(bus)
        crumbs.send(0x10, Message(type_id=0x01, opcode=0x02, data=b"\\x01"))
        reply = crumbs.query(0x10, type_id=0x01, opcode=0x80)

The wire format is docs/protocol.md; the frame functions are checked against
vectors written by the C library.
"""

from crumbs_i2c._controller import (
    FIRST_ADDRESS,
    LAST_ADDRESS,
    QUERY_DELAY_S,
    READ_ATTEMPTS,
    Bus,
    Controller,
    ReplyMismatch,
)
from crumbs_i2c._frame import (
    MAX_FRAME,
    MAX_PAYLOAD,
    MIN_FRAME,
    SET_REPLY,
    TYPE_ID_ANY,
    FrameError,
    Message,
    crc8,
    decode,
    encode,
    frame_length,
)
from crumbs_i2c._linux import LinuxBus

__all__ = [
    "FIRST_ADDRESS",
    "LAST_ADDRESS",
    "MAX_FRAME",
    "MAX_PAYLOAD",
    "MIN_FRAME",
    "QUERY_DELAY_S",
    "READ_ATTEMPTS",
    "SET_REPLY",
    "TYPE_ID_ANY",
    "Bus",
    "Controller",
    "FrameError",
    "LinuxBus",
    "Message",
    "ReplyMismatch",
    "crc8",
    "decode",
    "encode",
    "frame_length",
]
