"""The CRUMBS frame (docs/protocol.md): encoding, decoding and the CRC.

Mirrors ``crumbs_encode_message()``, ``crumbs_decode_message()`` and
``crumbs_frame_length()`` in ``src/core/crumbs_core.c``; the tests check all
three against vectors written by the C code.
"""

from __future__ import annotations

from typing import NamedTuple

#: Largest payload a frame carries.
MAX_PAYLOAD = 27
#: Largest frame: a three-byte header, the payload, and the CRC.
MAX_FRAME = 3 + MAX_PAYLOAD + 1
#: Smallest frame: a header and a CRC around an empty payload.
MIN_FRAME = 4
#: Wildcard type id; a peripheral never rejects a frame carrying it.
TYPE_ID_ANY = 0x00
#: The opcode a controller sends to choose which reply the next read returns.
SET_REPLY = 0xFE


def _table() -> tuple[int, ...]:
    table: list[int] = []
    for byte in range(256):
        crc = byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
        table.append(crc)
    return tuple(table)


_CRC_TABLE = _table()


def crc8(data: bytes) -> int:
    """CRC-8/SMBUS: polynomial 0x07, init 0, not reflected, no final XOR."""
    crc = 0
    for byte in data:
        crc = _CRC_TABLE[crc ^ byte]
    return crc


class Message(NamedTuple):
    type_id: int
    opcode: int
    data: bytes = b""


class FrameError(ValueError):
    """Bytes that are not a CRUMBS frame. ``crc`` is true only for a CRC
    mismatch in an otherwise well-formed frame, which the C library alone
    counts as a CRC error."""

    def __init__(self, message: str, *, crc: bool = False):
        super().__init__(message)
        self.crc = crc


def encode(message: Message) -> bytes:
    """The frame for ``message``. Raises ValueError for a type id or opcode
    outside 0-255, or a payload longer than MAX_PAYLOAD."""
    type_id, opcode, data = message
    for name, value in (("type_id", type_id), ("opcode", opcode)):
        if not 0 <= value <= 0xFF:
            raise ValueError(f"{name} must be 0-255, not {value}")
    if len(data) > MAX_PAYLOAD:
        raise ValueError(f"a payload is at most {MAX_PAYLOAD} bytes, not {len(data)}")
    body = bytes((type_id, opcode, len(data))) + bytes(data)
    return body + bytes((crc8(body),))


def frame_length(buffer: bytes) -> int:
    """The length of the frame a received header declares. A fixed-size read
    is padded past the frame, and decode() wants the frame exactly."""
    if len(buffer) < MIN_FRAME:
        raise FrameError(f"{len(buffer)} bytes is shorter than a frame")
    data_len = buffer[2]
    if data_len > MAX_PAYLOAD:
        raise FrameError(f"the header declares {data_len} payload bytes, more than {MAX_PAYLOAD}")
    declared = 3 + data_len + 1
    if len(buffer) < declared:
        raise FrameError(f"the header declares {declared} bytes, but {len(buffer)} arrived")
    return declared


def decode(frame: bytes) -> Message:
    """The message in exactly one frame. Raises FrameError for anything else:
    too short, a payload length over the maximum, more or fewer bytes than the
    header declares, or a CRC that does not match."""
    if len(frame) < MIN_FRAME:
        raise FrameError(f"{len(frame)} bytes is shorter than a frame")
    data_len = frame[2]
    if data_len > MAX_PAYLOAD:
        raise FrameError(f"the header declares {data_len} payload bytes, more than {MAX_PAYLOAD}")
    declared = 3 + data_len + 1
    if len(frame) != declared:
        raise FrameError(f"the header declares {declared} bytes, but the frame is {len(frame)}")
    expected = crc8(frame[:-1])
    if frame[-1] != expected:
        raise FrameError(f"CRC 0x{frame[-1]:02X} does not match 0x{expected:02X}", crc=True)
    return Message(frame[0], frame[1], bytes(frame[3:-1]))
