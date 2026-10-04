"""The controller side of CRUMBS: send a SET, query a GET, scan a bus.

Mirrors ``crumbs_controller_send()``, ``crumbs_controller_read()``,
``crumbs_controller_read_expect()`` and the strict read-probe scanner in
``src/core/crumbs_core.c``, over any object with the Bus methods.
"""

from __future__ import annotations

import errno
import threading
import time
from collections.abc import Callable, Iterable
from typing import Protocol

from crumbs_i2c._frame import (
    MAX_FRAME,
    SET_REPLY,
    TYPE_ID_ANY,
    CrumbsError,
    FrameError,
    Message,
    decode,
    encode,
    frame_length,
)

#: The pause between a SET_REPLY write and the read, as the C library's
#: generated getters wait (``CRUMBS_DEFAULT_QUERY_DELAY_US``).
QUERY_DELAY_S = 0.010

#: How many times query() tries a GET before giving up.
QUERY_ATTEMPTS = 3

#: The 7-bit addresses I2C leaves for devices.
FIRST_ADDRESS = 0x08
LAST_ADDRESS = 0x77

#: What Linux i2c-dev reports for a device that did not acknowledge, a bus
#: that timed out, or a transfer the adapter gave up on: a transient fault
#: on the wire, or no device at the address.
NO_ANSWER = frozenset({errno.ENXIO, errno.EREMOTEIO, errno.EIO, errno.ETIMEDOUT, errno.EAGAIN})


class Bus(Protocol):
    def write(self, address: int, data: bytes) -> None: ...

    def read(self, address: int, count: int) -> bytes: ...


class ReplyMismatch(CrumbsError):
    """A valid frame, but not the reply asked for: another opcode's staged
    reply, or another device type at the address. The CRC cannot catch
    this."""

    def __init__(self, reply: Message, type_id: int, opcode: int):
        super().__init__(
            f"expected type 0x{type_id:02X} opcode 0x{opcode:02X}, "
            f"got type 0x{reply.type_id:02X} opcode 0x{reply.opcode:02X}"
        )
        self.reply = reply
        self.type_id = type_id
        self.opcode = opcode

    @property
    def wrong_device(self) -> bool:
        """Another device type answered: retrying cannot help."""
        return self.type_id != TYPE_ID_ANY and self.reply.type_id != self.type_id


def _transient(exc: BaseException) -> bool:
    """A failed GET that asking again can fix."""
    if isinstance(exc, FrameError):
        # A reply read before the peripheral finished building it: a
        # Raspberry Pi's I2C controller ignores clock stretching, so it reads
        # 0xFF bytes (feastorg/Slice_DCMT#3).
        return True
    if isinstance(exc, ReplyMismatch):
        # The right device, staged on another opcode: the SET_REPLY was lost
        # on the wire, or the device restarted and is back on 0x00.
        return not exc.wrong_device
    return isinstance(exc, OSError) and exc.errno in NO_ANSWER


class Controller:
    """One controller on one bus. Its methods may be called from several
    threads; each exchange with a device holds a lock, so a GET's write,
    pause and read are never interleaved with another's."""

    def __init__(
        self,
        bus: Bus,
        *,
        query_delay_s: float = QUERY_DELAY_S,
        query_attempts: int = QUERY_ATTEMPTS,
        sleep: Callable[[float], None] = time.sleep,
    ):
        if query_attempts < 1:
            raise ValueError("query_attempts must be at least 1")
        self.bus = bus
        self.query_delay_s = query_delay_s
        self.query_attempts = query_attempts
        self._sleep = sleep
        self._lock = threading.RLock()

    def send(self, address: int, message: Message) -> None:
        """A SET: write one frame. Nothing comes back on the bus. Never
        retried here: a SET need not be safe to repeat."""
        with self._lock:
            self.bus.write(address, encode(message))

    def read(self, address: int) -> Message:
        """Read one frame: a full-size read, trimmed to the length its header
        declares, then decoded. Raises FrameError if it is not a frame."""
        with self._lock:
            raw = self.bus.read(address, MAX_FRAME)
        return decode(raw[: frame_length(raw)])

    def read_expect(self, address: int, *, type_id: int, opcode: int) -> Message:
        """read(), then check it is the reply asked for: ``opcode`` always,
        ``type_id`` unless it is TYPE_ID_ANY. Raises ReplyMismatch if not."""
        reply = self.read(address)
        if reply.opcode != opcode or (type_id != TYPE_ID_ANY and reply.type_id != type_id):
            raise ReplyMismatch(reply, type_id, opcode)
        return reply

    def query(self, address: int, *, type_id: int, opcode: int) -> Message:
        """A GET: ask for ``opcode``'s reply with SET_REPLY, wait, read it and
        check it. A GET is safe to repeat, so the whole exchange is tried
        again, up to ``query_attempts`` times, when the reply is corrupt, is
        the same device's reply to another opcode, or the device did not
        answer. Another device type at the address is not retried.

        Raises FrameError, ReplyMismatch or OSError from the last attempt."""
        with self._lock:
            for attempt in range(self.query_attempts):
                try:
                    self.send(address, Message(TYPE_ID_ANY, SET_REPLY, bytes((opcode,))))
                    self._sleep(self.query_delay_s)
                    return self.read_expect(address, type_id=type_id, opcode=opcode)
                except (CrumbsError, OSError) as exc:
                    if attempt + 1 == self.query_attempts or not _transient(exc):
                        raise
        raise AssertionError("unreachable")

    def scan(self, addresses: Iterable[int] | None = None) -> dict[int, int]:
        """Find CRUMBS devices: read each address once and keep those whose
        bytes decode as a frame, with the type id each one answered with.
        Nothing is written, so no device sees a command. An address with no
        device, or one that answers with something else, is skipped; any
        other bus error is raised."""
        found: dict[int, int] = {}
        span = range(FIRST_ADDRESS, LAST_ADDRESS + 1) if addresses is None else addresses
        with self._lock:
            for address in span:
                try:
                    found[address] = self.read(address).type_id
                except FrameError:
                    continue
                except OSError as exc:
                    if exc.errno in NO_ANSWER or exc.errno == errno.EBUSY:
                        # EBUSY: a kernel driver owns the address.
                        continue
                    raise
        return found
