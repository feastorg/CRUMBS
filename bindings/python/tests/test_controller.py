"""The controller over a scripted bus: what goes on the wire, and what it
does with what comes back."""

from collections import deque

import pytest

from crumbs_i2c import (
    MAX_FRAME,
    Controller,
    FrameError,
    Message,
    ReplyMismatch,
    encode,
)

PAD = b"\xff" * MAX_FRAME


class FakeBus:
    """Records each transaction; each read returns the next scripted reply,
    padded like a fixed-size i2c-dev read, or raises it."""

    def __init__(self, replies: dict[int, list[bytes | Exception]] | None = None):
        self.log: list[tuple[str, int, bytes | int]] = []
        self.replies = {a: deque(r) for a, r in (replies or {}).items()}

    def write(self, address: int, data: bytes) -> None:
        self.log.append(("write", address, data))

    def read(self, address: int, count: int) -> bytes:
        self.log.append(("read", address, count))
        queue = self.replies.get(address)
        if not queue:
            raise OSError(121, "Remote I/O error")
        reply = queue.popleft()
        if isinstance(reply, Exception):
            raise reply
        return (reply + PAD)[:count]


def controller(bus: FakeBus, **kwargs) -> tuple[Controller, list[float]]:
    sleeps: list[float] = []
    return Controller(bus, sleep=sleeps.append, **kwargs), sleeps


def test_send_writes_one_frame():
    bus = FakeBus()
    c, sleeps = controller(bus)
    c.send(0x10, Message(0x01, 0x02, b"\x05\x06"))
    assert bus.log == [("write", 0x10, encode(Message(0x01, 0x02, b"\x05\x06")))]
    assert sleeps == []


def test_query_stages_the_reply_waits_and_reads_a_full_frame():
    reply = Message(0x01, 0x80, b"\x2a")
    bus = FakeBus({0x10: [encode(reply)]})
    c, sleeps = controller(bus)
    assert c.query(0x10, 0x01, 0x80) == reply
    # SET_REPLY goes with the wildcard type, as the C getters send it.
    assert bus.log == [
        ("write", 0x10, bytes.fromhex("00fe0180") + bytes([encode(Message(0, 0xFE, b"\x80"))[-1]])),
        ("read", 0x10, MAX_FRAME),
    ]
    assert sleeps == [0.010]


def test_a_padded_read_is_trimmed_to_its_frame():
    reply = Message(0x01, 0x80, b"")
    bus = FakeBus({0x10: [encode(reply)]})
    c, _ = controller(bus)
    assert c.read(0x10) == reply


def test_a_corrupt_first_read_is_read_again():
    """The Pi's controller ignores clock stretching and reads 0xFF while the
    peripheral is still building its reply."""
    reply = Message(0x01, 0x80, b"\x01\x02")
    bus = FakeBus({0x10: [PAD, encode(reply)[:-1] + b"\x00", encode(reply)]})
    c, sleeps = controller(bus)
    assert c.query(0x10, 0x01, 0x80) == reply
    assert [entry[0] for entry in bus.log] == ["write", "read", "read", "read"]
    assert sleeps == [0.010, 0.010, 0.010]


def test_reads_give_up_after_read_attempts():
    bus = FakeBus({0x10: [PAD, PAD]})
    c, _ = controller(bus, read_attempts=2)
    with pytest.raises(FrameError):
        c.query(0x10, 0x01, 0x80)
    assert [entry[0] for entry in bus.log] == ["write", "read", "read"]


def test_a_bus_error_is_not_retried():
    bus = FakeBus({0x10: [OSError(121, "Remote I/O error"), encode(Message(1, 0x80))]})
    c, _ = controller(bus)
    with pytest.raises(OSError):
        c.query(0x10, 0x01, 0x80)


@pytest.mark.parametrize(
    ("reply", "type_id"),
    [
        (Message(0x01, 0x81, b""), 0x01),  # another opcode's staged reply
        (Message(0x02, 0x80, b""), 0x01),  # another device type
    ],
)
def test_the_wrong_reply_is_a_mismatch_and_not_retried(reply, type_id):
    bus = FakeBus({0x10: [encode(reply), encode(Message(0x01, 0x80))]})
    c, _ = controller(bus)
    with pytest.raises(ReplyMismatch) as e:
        c.query(0x10, type_id, 0x80)
    assert e.value.reply == reply
    assert [entry[0] for entry in bus.log] == ["write", "read"]


def test_the_wildcard_type_accepts_any_device_type():
    reply = Message(0x07, 0x80, b"")
    c, _ = controller(FakeBus({0x10: [encode(reply)]}))
    assert c.query(0x10, 0x00, 0x80) == reply


def test_scan_reads_each_address_and_writes_nothing():
    bus = FakeBus(
        {
            0x10: [encode(Message(0x01, 0x00, b"\x78\x05\x01\x02\x03"))],
            0x11: [PAD],  # a device that answers, but not with a frame
            0x20: [encode(Message(0x03, 0x00, b""))],
        }
    )
    c, _ = controller(bus)
    assert c.scan(range(0x08, 0x78)) == {0x10: 0x01, 0x20: 0x03}
    assert all(entry[0] == "read" for entry in bus.log)
    assert len(bus.log) == 0x78 - 0x08


def test_scan_defaults_to_the_device_address_range():
    bus = FakeBus()
    c, _ = controller(bus)
    assert c.scan() == {}
    assert [entry[1] for entry in bus.log] == list(range(0x08, 0x78))


def test_read_attempts_must_be_positive():
    with pytest.raises(ValueError):
        Controller(FakeBus(), read_attempts=0)
