"""The controller side of CRUMBS: send a SET, query a GET, scan a bus.

Mirrors ``crumbs_controller_send()``, ``crumbs_controller_read()``,
``crumbs_controller_read_expect()`` and the read-probe scanner in
``src/core/crumbs_core.c``, over any object with the Bus methods.
"""

from __future__ import annotations

import time
from collections.abc import Callable, Iterable
from typing import Protocol

from crumbs_i2c._frame import (
    MAX_FRAME,
    SET_REPLY,
    TYPE_ID_ANY,
    FrameError,
    Message,
    decode,
    encode,
    frame_length,
)

#: The pause between a SET_REPLY write and the read, as the C library's
#: generated getters wait (``CRUMBS_DEFAULT_QUERY_DELAY_US``).
QUERY_DELAY_S = 0.010

#: How many times a reply is read before a corrupt one is given up on. The
#: Raspberry Pi's I2C controller ignores clock stretching, so a reply the
#: peripheral is still building reads as 0xFF bytes; a few percent of first
#: reads fail this way (feastorg/Slice_DCMT#3). The requested opcode persists
#: on the peripheral, so reading again is safe.
READ_ATTEMPTS = 3

#: The 7-bit addresses I2C leaves for devices.
FIRST_ADDRESS = 0x08
LAST_ADDRESS = 0x77


class Bus(Protocol):
    def write(self, address: int, data: bytes) -> None: ...

    def read(self, address: int, count: int) -> bytes: ...


class ReplyMismatch(Exception):
    """A valid frame, but not the reply asked for: another opcode's staged
    reply, or another device type at the address. The CRC cannot catch
    this."""

    def __init__(self, reply: Message, type_id: int, opcode: int):
        super().__init__(
            f"expected type 0x{type_id:02X} opcode 0x{opcode:02X}, "
            f"got type 0x{reply.type_id:02X} opcode 0x{reply.opcode:02X}"
        )
        self.reply = reply


class Controller:
    def __init__(
        self,
        bus: Bus,
        *,
        query_delay_s: float = QUERY_DELAY_S,
        read_attempts: int = READ_ATTEMPTS,
        sleep: Callable[[float], None] = time.sleep,
    ):
        if read_attempts < 1:
            raise ValueError("read_attempts must be at least 1")
        self.bus = bus
        self.query_delay_s = query_delay_s
        self.read_attempts = read_attempts
        self._sleep = sleep

    def send(self, address: int, message: Message) -> None:
        """A SET: write one frame. Nothing comes back on the bus."""
        self.bus.write(address, encode(message))

    def read(self, address: int) -> Message:
        """Read one frame: a full-size read, trimmed to the length its header
        declares, then decoded. Raises FrameError if it is not a frame."""
        raw = self.bus.read(address, MAX_FRAME)
        return decode(raw[: frame_length(raw)])

    def read_expect(self, address: int, type_id: int, opcode: int) -> Message:
        """read(), then check it is the reply asked for: ``opcode`` always,
        ``type_id`` unless it is TYPE_ID_ANY. A frame that fails to decode is
        read again, up to ``read_attempts`` reads in all."""
        for attempt in range(self.read_attempts):
            try:
                reply = self.read(address)
            except FrameError:
                if attempt + 1 == self.read_attempts:
                    raise
                self._sleep(self.query_delay_s)
                continue
            if reply.opcode != opcode or (type_id != TYPE_ID_ANY and reply.type_id != type_id):
                raise ReplyMismatch(reply, type_id, opcode)
            return reply
        raise AssertionError("unreachable")

    def query(self, address: int, type_id: int, opcode: int) -> Message:
        """A GET: ask for ``opcode``'s reply with SET_REPLY, wait, and read it.
        Raises FrameError if no read decodes, ReplyMismatch if the reply is
        another one."""
        self.send(address, Message(TYPE_ID_ANY, SET_REPLY, bytes((opcode,))))
        self._sleep(self.query_delay_s)
        return self.read_expect(address, type_id, opcode)

    def scan(self, addresses: Iterable[int] | None = None) -> dict[int, int]:
        """Find CRUMBS devices: read each address once and keep those whose
        bytes decode as a frame, with the type id each one answered with.
        Nothing is written, so no device sees a command. An address that does
        not answer is skipped."""
        found: dict[int, int] = {}
        for address in (
            addresses if addresses is not None else range(FIRST_ADDRESS, LAST_ADDRESS + 1)
        ):
            try:
                found[address] = self.read(address).type_id
            except (OSError, FrameError):
                continue
        return found
