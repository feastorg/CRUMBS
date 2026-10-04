"""The controller against a model of a CRUMBS peripheral: what goes on the
wire, and what it does with what comes back."""

import errno
import threading
import time
from collections import deque

import pytest

from crumbs_i2c import (
    MAX_FRAME,
    SET_REPLY,
    Controller,
    CrumbsError,
    FrameError,
    Message,
    ReplyMismatch,
    decode,
    encode,
)

PAD = b"\xff" * MAX_FRAME


class Peripheral:
    """A device as protocol.md describes it: SET_REPLY stages an opcode that
    persists, a read returns the staged opcode's reply padded to the read
    size, and the staged opcode starts at 0x00."""

    def __init__(self, type_id: int = 0x01):
        self.type_id = type_id
        self.staged = 0x00
        # Scripted faults, taken in order: "drop" loses the next write,
        # "pad" makes the next read all 0xFF, an OSError fails the next
        # read with it.
        self.faults: deque[str | OSError] = deque()

    def write(self, frame: bytes) -> None:
        if self.faults and self.faults[0] == "drop":
            self.faults.popleft()
            return
        message = decode(frame)
        if message.opcode == SET_REPLY and message.data:
            self.staged = message.data[0]

    def read(self, count: int) -> bytes:
        if self.faults and self.faults[0] != "drop":
            fault = self.faults.popleft()
            if isinstance(fault, OSError):
                raise fault
            return PAD[:count]
        reply = encode(Message(self.type_id, self.staged, bytes((self.staged,))))
        return (reply + PAD)[:count]


class FakeBus:
    def __init__(self, devices: dict[int, Peripheral] | None = None, pause: float = 0.0):
        self.devices = devices or {}
        self.log: list[tuple[str, int, bytes | int]] = []
        self.pause = pause

    def _device(self, address: int) -> Peripheral:
        if address not in self.devices:
            raise OSError(errno.ENXIO, "No such device or address")
        return self.devices[address]

    def write(self, address: int, data: bytes) -> None:
        self.log.append(("write", address, data))
        time.sleep(self.pause)
        self._device(address).write(data)

    def read(self, address: int, count: int) -> bytes:
        self.log.append(("read", address, count))
        time.sleep(self.pause)
        return self._device(address).read(count)


def controller(bus: FakeBus, **kwargs) -> tuple[Controller, list[float]]:
    sleeps: list[float] = []
    return Controller(bus, sleep=sleeps.append, **kwargs), sleeps


def kinds(bus: FakeBus) -> list[str]:
    return [entry[0] for entry in bus.log]


def test_send_writes_one_frame():
    bus = FakeBus({0x10: Peripheral()})
    c, sleeps = controller(bus)
    c.send(0x10, Message(0x01, 0x02, b"\x05\x06"))
    assert bus.log == [("write", 0x10, encode(Message(0x01, 0x02, b"\x05\x06")))]
    assert sleeps == []


def test_send_is_never_retried():
    bus = FakeBus()
    c, _ = controller(bus)
    with pytest.raises(OSError):
        c.send(0x10, Message(0x01, 0x02))
    assert kinds(bus) == ["write"]


def test_query_stages_the_reply_waits_and_reads_a_full_frame():
    bus = FakeBus({0x10: Peripheral()})
    c, sleeps = controller(bus)
    assert c.query(0x10, type_id=0x01, opcode=0x80) == Message(0x01, 0x80, b"\x80")
    # SET_REPLY goes with the wildcard type, as the C getters send it.
    assert bus.log == [
        ("write", 0x10, encode(Message(0x00, SET_REPLY, b"\x80"))),
        ("read", 0x10, MAX_FRAME),
    ]
    assert sleeps == [0.010]


def test_a_corrupt_reply_retries_the_whole_get():
    """A Pi reads 0xFF while the peripheral is still building its reply."""
    device = Peripheral()
    device.faults.extend(["pad", "pad"])
    bus = FakeBus({0x10: device})
    c, sleeps = controller(bus)
    assert c.query(0x10, type_id=0x01, opcode=0x80).opcode == 0x80
    assert kinds(bus) == ["write", "read"] * 3
    assert sleeps == [0.010] * 3


def test_a_lost_set_reply_is_asked_again():
    """The device stays on the opcode it had; reading again alone would
    return that reply every time."""
    device = Peripheral()
    device.staged = 0x81
    device.faults.append("drop")
    bus = FakeBus({0x10: device})
    c, _ = controller(bus)
    assert c.query(0x10, type_id=0x01, opcode=0x80) == Message(0x01, 0x80, b"\x80")
    assert kinds(bus) == ["write", "read", "write", "read"]


def test_a_device_that_did_not_answer_is_asked_again():
    device = Peripheral()
    device.faults.append(OSError(errno.EREMOTEIO, "Remote I/O error"))
    c, _ = controller(FakeBus({0x10: device}))
    assert c.query(0x10, type_id=0x01, opcode=0x80).opcode == 0x80


def test_query_gives_up_after_its_attempts():
    device = Peripheral()
    device.faults.extend(["pad", "pad"])
    bus = FakeBus({0x10: device})
    c, _ = controller(bus, query_attempts=2)
    with pytest.raises(FrameError):
        c.query(0x10, type_id=0x01, opcode=0x80)
    assert kinds(bus) == ["write", "read"] * 2


def test_another_device_type_is_not_retried():
    bus = FakeBus({0x10: Peripheral(type_id=0x02)})
    c, _ = controller(bus)
    with pytest.raises(ReplyMismatch) as e:
        c.query(0x10, type_id=0x01, opcode=0x80)
    assert e.value.wrong_device
    assert (e.value.type_id, e.value.opcode) == (0x01, 0x80)
    assert e.value.reply == Message(0x02, 0x80, b"\x80")
    assert kinds(bus) == ["write", "read"]


def test_a_bus_fault_that_is_not_the_device_is_not_retried():
    device = Peripheral()
    device.faults.append(OSError(errno.EBADF, "Bad file descriptor"))
    bus = FakeBus({0x10: device})
    c, _ = controller(bus)
    with pytest.raises(OSError) as e:
        c.query(0x10, type_id=0x01, opcode=0x80)
    assert e.value.errno == errno.EBADF
    assert kinds(bus) == ["write", "read"]


def test_the_wildcard_type_accepts_any_device_type():
    c, _ = controller(FakeBus({0x10: Peripheral(type_id=0x07)}))
    assert c.query(0x10, type_id=0x00, opcode=0x80).type_id == 0x07


def test_a_padded_read_is_trimmed_to_its_frame():
    c, _ = controller(FakeBus({0x10: Peripheral()}))
    assert c.read(0x10) == Message(0x01, 0x00, b"\x00")


def test_type_id_and_opcode_are_named():
    c, _ = controller(FakeBus({0x10: Peripheral()}))
    with pytest.raises(TypeError):
        c.query(0x10, 0x01, 0x80)  # type: ignore[misc]


def test_protocol_errors_share_a_base():
    assert issubclass(FrameError, CrumbsError)
    assert issubclass(ReplyMismatch, CrumbsError)
    assert not issubclass(FrameError, ValueError)


def test_threads_sharing_a_controller_never_get_each_others_reply():
    """A GET is a write, a pause and a read; another thread's GET between
    them would stage its own opcode."""
    bus = FakeBus({0x10: Peripheral()}, pause=0.0005)
    c = Controller(bus, query_delay_s=0.0005)
    errors: list[BaseException] = []

    def work(opcode: int) -> None:
        try:
            for _ in range(20):
                assert c.query(0x10, type_id=0x01, opcode=opcode).opcode == opcode
        except BaseException as e:
            errors.append(e)

    threads = [threading.Thread(target=work, args=(op,)) for op in (0x80, 0x81, 0x82)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert errors == []
    # One attempt each: no reply was stolen and then retried.
    assert kinds(bus).count("write") == 60


def test_scan_reads_each_address_and_writes_nothing():
    other = Peripheral()
    other.faults.extend(["pad"])  # answers, but not with a frame
    busy = Peripheral()
    busy.faults.append(OSError(errno.EBUSY, "Device or resource busy"))
    bus = FakeBus({0x10: Peripheral(0x01), 0x11: other, 0x12: busy, 0x20: Peripheral(0x03)})
    c, _ = controller(bus)
    assert c.scan(range(0x08, 0x78)) == {0x10: 0x01, 0x20: 0x03}
    assert set(kinds(bus)) == {"read"}
    assert len(bus.log) == 0x78 - 0x08


def test_scan_raises_a_bus_that_is_broken():
    device = Peripheral()
    device.faults.append(OSError(errno.EBADF, "Bad file descriptor"))
    c, _ = controller(FakeBus({0x10: device}))
    with pytest.raises(OSError):
        c.scan([0x10])


def test_scan_defaults_to_the_device_address_range():
    bus = FakeBus()
    c, _ = controller(bus)
    assert c.scan() == {}
    assert [entry[1] for entry in bus.log] == list(range(0x08, 0x78))


def test_query_attempts_must_be_positive():
    with pytest.raises(ValueError):
        Controller(FakeBus(), query_attempts=0)
