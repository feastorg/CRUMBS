"""The controller side of CRUMBS: send a SET, query a GET, scan a bus.

Mirrors ``crumbs_controller_send()``, ``crumbs_controller_read()``,
``crumbs_controller_read_expect()``, the SET acknowledgement
(``crumbs_ack_decode()``, ``crumbs_ack_classify()``,
``crumbs_controller_read_ack()``, ``crumbs_controller_send_confirmed()``)
and the strict read-probe scanner in ``src/core/crumbs_core.c``, over any
object with the Bus methods.
"""

from __future__ import annotations

import errno
import threading
import time
from collections.abc import Callable, Iterable
from enum import IntEnum
from typing import NamedTuple, Protocol

from crumbs_i2c._frame import (
    GET_ACK,
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

#: The last command's status in a GET_ACK reply (``CRUMBS_ACK_*``).
ACK_NONE = 0x00
ACK_APPLIED = 0x01
ACK_REFUSED = 0x02
ACK_REJECTED = 0x03

#: Why a command was rejected (``CRUMBS_ACK_REJECT_*``).
REJECT_NO_HANDLER = 0x01
REJECT_TYPE = 0x02

#: The first ``CRUMBS_VERSION`` whose peripherals answer GET_ACK, as the
#: opcode 0x00 version reply carries it in its first two bytes.
ACK_MIN_VERSION = 1600

#: How many SETs send_confirmed() sends at most, by default.
CONFIRMED_SENDS = 3


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


class Ack(NamedTuple):
    """A peripheral's acknowledgement: how many command frames it has
    counted (modulo 256), and the last one's opcode, status and reason."""

    counter: int
    opcode: int
    status: int
    reason: int


class SetOutcome(IntEnum):
    """What the acknowledgement says became of a SET (``CRUMBS_SET_*``).
    Only APPLIED means the command took effect."""

    APPLIED = 0
    REFUSED = 1
    REJECTED = 2
    NOT_ADVANCED = 3


class AckMismatch(CrumbsError):
    """The counter advanced, but on another opcode: some other frame was
    dispatched, and whether this SET was applied cannot be told."""

    def __init__(self, before: Ack, after: Ack, opcode: int):
        super().__init__(
            f"sent opcode 0x{opcode:02X}, but the last command counted is 0x{after.opcode:02X}"
        )
        self.before = before
        self.after = after
        self.opcode = opcode


class AckUnknown(CrumbsError):
    """The acknowledgement cannot say what became of the SET: the device
    holds no record (it has just started, or restarted after acting on the
    SET), or its counter is back at the baseline with another record. The
    SET may have been applied, so it is not sent again."""

    def __init__(self, before: Ack, after: Ack, opcode: int):
        super().__init__(
            f"cannot tell whether opcode 0x{opcode:02X} was applied: "
            f"acknowledgement went from {tuple(before)} to {tuple(after)}"
        )
        self.before = before
        self.after = after
        self.opcode = opcode


_OUTCOMES = {
    ACK_APPLIED: SetOutcome.APPLIED,
    ACK_REFUSED: SetOutcome.REFUSED,
    ACK_REJECTED: SetOutcome.REJECTED,
}


def _valid(ack: Ack) -> bool:
    """A record a device can hold: one of the three outcomes, or status none
    with every other byte zero, as before its first command."""
    if ack.status == ACK_NONE:
        return ack.counter == 0 and ack.opcode == 0 and ack.reason == 0
    return ack.status in _OUTCOMES


def decode_ack(reply: Message) -> Ack:
    """The acknowledgement in a GET_ACK reply. Raises ReplyMismatch for
    another opcode's reply, a payload under four bytes, or four bytes that
    are not a record a device can hold (an older device echoing the opcode
    with a payload of its own); longer payloads are accepted, as an
    extension."""
    if reply.opcode != GET_ACK or len(reply.data) < 4:
        raise ReplyMismatch(reply, TYPE_ID_ANY, GET_ACK)
    ack = Ack(*reply.data[:4])
    if not _valid(ack):
        raise ReplyMismatch(reply, TYPE_ID_ANY, GET_ACK)
    return ack


class Version(NamedTuple):
    """A peripheral's opcode 0x00 version reply, as
    ``crumbs_build_version_reply()`` builds it: the ``CRUMBS_VERSION`` it
    was built with (``major * 10000 + minor * 100 + patch``), then its
    module's own major, minor and patch."""

    crumbs_version: int
    major: int
    minor: int
    patch: int


def decode_version(reply: Message) -> Version:
    """The version in an opcode 0x00 reply: a little-endian u16, then three
    bytes. Raises ReplyMismatch for another opcode's reply or a payload
    under five bytes; longer payloads are accepted, as an extension."""
    if reply.opcode != 0x00 or len(reply.data) < 5:
        raise ReplyMismatch(reply, TYPE_ID_ANY, 0x00)
    data = reply.data
    return Version(int.from_bytes(data[:2], "little"), data[2], data[3], data[4])


def classify(before: Ack, after: Ack, opcode: int) -> SetOutcome:
    """What became of a SET with ``opcode``, from the acknowledgements read
    before and after it. NOT_ADVANCED, the one outcome on which a SET may be
    sent again, needs ``after`` equal to ``before`` and a status other than
    none. Raises AckUnknown when the device holds no record or its counter is
    unchanged with another record, AckMismatch if the counter advanced on
    another opcode, and CrumbsError for a record no device can hold."""
    if not _valid(before) or not _valid(after):
        raise CrumbsError(
            f"not an acknowledgement a device can hold: {tuple(before)}, {tuple(after)}"
        )
    if after.status == ACK_NONE:
        raise AckUnknown(before, after, opcode)
    if after.counter == before.counter:
        if after == before:
            return SetOutcome.NOT_ADVANCED
        raise AckUnknown(before, after, opcode)
    if after.opcode != opcode:
        raise AckMismatch(before, after, opcode)
    return _OUTCOMES[after.status]


def _transient(exc: BaseException) -> bool:
    """A failed GET that asking again can fix."""
    if isinstance(exc, FrameError):
        # A reply that slipped a bit: with a Raspberry Pi as controller, the
        # bus can lose a clock edge around the peripheral's clock stretch
        # unless it runs slowly, so the reply is correct up to some byte,
        # then 0xFF (docs/protocol.md, "When the reply is built").
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

    def get_version(self, address: int, *, type_id: int = TYPE_ID_ANY) -> Version:
        """The device's version reply: a query for opcode 0x00, retried as
        any query is, then parsed. Raises what query() and decode_version()
        raise."""
        reply = self.query(address, type_id=type_id, opcode=0x00)
        return decode_version(reply)

    def read_ack(self, address: int) -> Ack:
        """The device's acknowledgement: a query for GET_ACK, retried as any
        query is, then parsed. A device built before GET_ACK answers with
        another opcode's reply, an echo of 0xFD that is not a valid record,
        or nothing, so ReplyMismatch here, or a read that keeps failing, means
        it has no acknowledgement; an echo that happens to form a valid record
        is not caught. Its version reply (ACK_MIN_VERSION) is the reliable
        test."""
        reply = self.query(address, type_id=TYPE_ID_ANY, opcode=GET_ACK)
        return decode_ack(reply)

    def _check_ack_version(self, address: int) -> None:
        """Raise ReplyMismatch unless the device's version reply (opcode
        0x00) decodes and reports a CRUMBS_VERSION of at least
        ACK_MIN_VERSION. An older device's echo of GET_ACK can form a valid
        record whose counter never moves; its version reply cannot."""
        reply = self.query(address, type_id=TYPE_ID_ANY, opcode=0x00)
        if decode_version(reply).crumbs_version < ACK_MIN_VERSION:
            raise ReplyMismatch(reply, TYPE_ID_ANY, GET_ACK)

    def send_confirmed(
        self, address: int, message: Message, *, sends: int = CONFIRMED_SENDS
    ) -> tuple[SetOutcome, Ack]:
        """A SET confirmed by the device's acknowledgement: check its version
        reply, read the acknowledgement, send, read it again and classify.
        If all four bytes equal the baseline and the status is not none, the
        frame never arrived, and it is sent again, up to ``sends`` times in
        all; nothing else is re-sent. A
        write that raises is still followed by a read, since the frame may
        have landed. An acknowledgement that cannot be read after a send is
        raised, never answered with a re-send, so a frame is not applied
        twice because a read failed; if the version or the first
        acknowledgement cannot be read, nothing is sent.

        Returns the outcome and the last acknowledgement read, with
        NOT_ADVANCED once the sends are spent. Raises AckMismatch if another
        command was counted, AckUnknown if the acknowledgement cannot say
        (the device restarted, or its counter came round), ReplyMismatch if
        its version reply reports a CRUMBS_VERSION below ACK_MIN_VERSION
        (it has no acknowledgement; send to it with send(), unconfirmed),
        and what query(), decode_version() and read_ack() raise."""
        if sends < 1:
            raise ValueError("sends must be at least 1")
        if message.opcode == SET_REPLY:
            raise ValueError("SET_REPLY is never counted, so it cannot be confirmed")
        with self._lock:
            self._check_ack_version(address)
            before = self.read_ack(address)
            after = before
            for _ in range(sends):
                try:
                    self.send(address, message)
                except OSError:
                    pass  # the acknowledgement decides whether it landed
                after = self.read_ack(address)
                outcome = classify(before, after, message.opcode)
                if outcome is not SetOutcome.NOT_ADVANCED:
                    return outcome, after
            return SetOutcome.NOT_ADVANCED, after

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
