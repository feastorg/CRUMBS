"""CRUMBS for Python: the frame, its CRC, and a controller over Linux I2C.

    from crumbs_i2c import Controller, LinuxBus, Message

    with LinuxBus("/dev/i2c-1") as bus:
        crumbs = Controller(bus)
        crumbs.send(0x10, Message(type_id=0x01, opcode=0x02, data=b"\\x01"))
        reply = crumbs.query(0x10, type_id=0x01, opcode=0x80)
        outcome, ack = crumbs.send_confirmed(0x10, Message(0x01, 0x02, b"\\x01"))

Protocol errors are CrumbsError (FrameError, ReplyMismatch, AckMismatch,
AckUnknown); bus errors are OSError. The wire format is docs/protocol.md;
the frame functions are checked against vectors written by the C library.
"""

from crumbs_i2c._controller import (
    ACK_APPLIED,
    ACK_MIN_VERSION,
    ACK_NONE,
    ACK_REFUSED,
    ACK_REJECTED,
    CONFIRMED_SENDS,
    FIRST_ADDRESS,
    LAST_ADDRESS,
    NO_ANSWER,
    QUERY_ATTEMPTS,
    QUERY_DELAY_S,
    REJECT_NO_HANDLER,
    REJECT_TYPE,
    Ack,
    AckMismatch,
    AckUnknown,
    Bus,
    Controller,
    ReplyMismatch,
    SetOutcome,
    classify,
    decode_ack,
)
from crumbs_i2c._frame import (
    GET_ACK,
    MAX_FRAME,
    MAX_PAYLOAD,
    MIN_FRAME,
    SET_REPLY,
    TYPE_ID_ANY,
    CrumbsError,
    FrameError,
    Message,
    crc8,
    decode,
    encode,
    frame_length,
)
from crumbs_i2c._linux import LinuxBus

__all__ = [
    "ACK_APPLIED",
    "ACK_MIN_VERSION",
    "ACK_NONE",
    "ACK_REFUSED",
    "ACK_REJECTED",
    "CONFIRMED_SENDS",
    "FIRST_ADDRESS",
    "GET_ACK",
    "LAST_ADDRESS",
    "MAX_FRAME",
    "MAX_PAYLOAD",
    "MIN_FRAME",
    "NO_ANSWER",
    "QUERY_ATTEMPTS",
    "QUERY_DELAY_S",
    "REJECT_NO_HANDLER",
    "REJECT_TYPE",
    "SET_REPLY",
    "TYPE_ID_ANY",
    "Ack",
    "AckMismatch",
    "AckUnknown",
    "Bus",
    "Controller",
    "CrumbsError",
    "FrameError",
    "LinuxBus",
    "Message",
    "ReplyMismatch",
    "SetOutcome",
    "classify",
    "crc8",
    "decode",
    "decode_ack",
    "encode",
    "frame_length",
]
