"""The SET acknowledgement: GET_ACK parsed and classified as the C core does
(tests/golden_vectors/vectors.json), and the confirmed send against a model
of a peripheral that counts its command frames."""

import errno
import json
import re
from collections import deque
from pathlib import Path

import pytest

import crumbs_i2c
from crumbs_i2c import (
    ACK_APPLIED,
    ACK_REFUSED,
    ACK_REJECTED,
    GET_ACK,
    MAX_FRAME,
    REJECT_NO_HANDLER,
    SET_REPLY,
    Ack,
    AckMismatch,
    Controller,
    CrumbsError,
    FrameError,
    Message,
    ReplyMismatch,
    SetOutcome,
    classify,
    decode,
    decode_ack,
    encode,
)

ROOT = Path(__file__).parents[3]
VECTORS = json.loads((ROOT / "tests" / "golden_vectors" / "vectors.json").read_text())
PAD = b"\xff" * MAX_FRAME
TYPE = 0x07
APPLY, REFUSE, UNHANDLED = 0x01, 0x02, 0x03


@pytest.mark.parametrize("case", VECTORS["ack"], ids=lambda c: c["received"][:16] or "initial")
def test_decode_ack_reads_what_the_c_peripheral_sends(case):
    reply = decode(bytes.fromhex(case["reply"]))
    assert reply.opcode == GET_ACK
    assert decode_ack(reply) == Ack(case["counter"], case["opcode"], case["status"], case["reason"])


@pytest.mark.parametrize("case", VECTORS["ack_classify"])
def test_classify_matches_the_c_core(case):
    before = Ack(case["before"], 0, ACK_APPLIED, 0)
    counter, opcode, status = case["after"]
    after = Ack(counter, opcode, status, 0)
    if case["result"] >= 0:
        assert classify(before, after, case["opcode"]) == SetOutcome(case["result"])
    elif case["result"] == -9:
        with pytest.raises(AckMismatch):
            classify(before, after, case["opcode"])
    else:
        assert case["result"] == -1
        with pytest.raises(CrumbsError) as e:
            classify(before, after, case["opcode"])
        assert not isinstance(e.value, AckMismatch)


def test_the_vectors_cover_every_outcome():
    assert {c["status"] for c in VECTORS["ack"]} == {0, 1, 2, 3}
    assert {c["result"] for c in VECTORS["ack_classify"]} == {0, 1, 2, 3, -1, -9}


def test_decode_ack_refuses_what_is_not_an_acknowledgement():
    with pytest.raises(FrameError):
        decode_ack(Message(TYPE, GET_ACK, b"\x01\x02\x03"))
    with pytest.raises(ReplyMismatch):
        decode_ack(Message(TYPE, 0x00, b"\x01\x02\x03\x04"))
    # Trailing bytes are how the layout is extended.
    assert decode_ack(Message(TYPE, GET_ACK, b"\x01\x02\x03\x04\x05")) == Ack(1, 2, 3, 4)


def test_the_ack_constants_match_the_header():
    header = (ROOT / "src" / "crumbs.h").read_text()

    def define(name: str) -> int:
        match = re.search(rf"#define {name} (0x[0-9A-Fa-f]+|\d+)u?\b", header)
        assert match is not None, name
        return int(match.group(1), 0)

    assert crumbs_i2c.GET_ACK == define("CRUMBS_CMD_GET_ACK")
    assert crumbs_i2c.ACK_NONE == define("CRUMBS_ACK_NONE")
    assert crumbs_i2c.ACK_APPLIED == define("CRUMBS_ACK_APPLIED")
    assert crumbs_i2c.ACK_REFUSED == define("CRUMBS_ACK_REFUSED")
    assert crumbs_i2c.ACK_REJECTED == define("CRUMBS_ACK_REJECTED")
    assert crumbs_i2c.REJECT_NO_HANDLER == define("CRUMBS_ACK_REJECT_NO_HANDLER")
    assert crumbs_i2c.REJECT_TYPE == define("CRUMBS_ACK_REJECT_TYPE")
    assert crumbs_i2c.ACK_MIN_VERSION == define("CRUMBS_ACK_MIN_VERSION")
    for outcome in SetOutcome:
        assert outcome.value == define(f"CRUMBS_SET_{outcome.name}")


class AckPeripheral:
    """A peripheral as protocol.md describes GET_ACK: every frame but
    SET_REPLY is counted, with the opcode and what became of it. Opcode
    APPLY is applied, REFUSE refused with reason 0x2A, anything else
    rejected. ``ack=False`` models one built before GET_ACK, which answers
    every read with its version reply."""

    def __init__(self, *, ack: bool = True):
        self.ack = ack
        self.staged = 0x00
        self.state = Ack(0, 0, 0, 0)
        # Scripted faults, in order: "drop" loses the next command write
        # while it reports success, "fail-write" delivers the next command
        # write and then raises.
        self.faults: deque[str] = deque()
        # Reads, counted from 0, that fail as a device that did not answer.
        self.failed_reads: set[int] = set()
        self.reads = 0

    def write(self, frame: bytes) -> None:
        message = decode(frame)
        if message.opcode == SET_REPLY:
            self.staged = message.data[0]
            return
        if self.faults and self.faults[0] == "drop":
            self.faults.popleft()
            return
        status, reason = {APPLY: (ACK_APPLIED, 0), REFUSE: (ACK_REFUSED, 0x2A)}.get(
            message.opcode, (ACK_REJECTED, REJECT_NO_HANDLER)
        )
        self.state = Ack((self.state.counter + 1) % 256, message.opcode, status, reason)
        if self.faults and self.faults[0] == "fail-write":
            self.faults.popleft()
            raise OSError(errno.EREMOTEIO, "Remote I/O error")

    def read(self, count: int) -> bytes:
        self.reads += 1
        if self.reads - 1 in self.failed_reads:
            raise OSError(errno.EREMOTEIO, "Remote I/O error")
        if self.ack and self.staged == GET_ACK:
            reply = Message(TYPE, GET_ACK, bytes(self.state))
        else:
            reply = Message(TYPE, 0x00, b"\x40\x06\x01\x00\x00")
        return (encode(reply) + PAD)[:count]


class Bus:
    def __init__(self, device: AckPeripheral):
        self.device = device
        self.log: list[str] = []

    def write(self, address: int, data: bytes) -> None:
        self.log.append("stage" if data[1] == SET_REPLY else "set")
        self.device.write(data)

    def read(self, address: int, count: int) -> bytes:
        self.log.append("read")
        return self.device.read(count)


def controller(device: AckPeripheral) -> tuple[Controller, Bus]:
    bus = Bus(device)
    return Controller(bus, sleep=lambda _: None), bus


def test_read_ack_stages_get_ack_and_parses_the_reply():
    device = AckPeripheral()
    c, bus = controller(device)
    c.send(0x10, Message(TYPE, APPLY))
    assert c.read_ack(0x10) == Ack(1, APPLY, ACK_APPLIED, 0)
    assert bus.log == ["set", "stage", "read"]


def test_read_ack_of_an_old_peripheral_is_a_reply_mismatch():
    c, bus = controller(AckPeripheral(ack=False))
    with pytest.raises(ReplyMismatch):
        c.read_ack(0x10)
    assert bus.log == ["stage", "read"] * 3


def test_send_confirmed_applied():
    device = AckPeripheral()
    c, bus = controller(device)
    assert c.send_confirmed(0x10, Message(TYPE, APPLY)) == (
        SetOutcome.APPLIED,
        Ack(1, APPLY, ACK_APPLIED, 0),
    )
    assert bus.log == ["stage", "read", "set", "stage", "read"]


def test_send_confirmed_resends_a_lost_frame():
    device = AckPeripheral()
    device.faults.append("drop")
    c, bus = controller(device)
    outcome, ack = c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert outcome is SetOutcome.APPLIED
    assert ack.counter == 1
    assert bus.log == ["stage", "read"] + ["set", "stage", "read"] * 2


def test_send_confirmed_stops_at_its_bound():
    device = AckPeripheral()
    device.faults.extend(["drop"] * 5)
    c, bus = controller(device)
    outcome, ack = c.send_confirmed(0x10, Message(TYPE, APPLY), sends=2)
    assert outcome is SetOutcome.NOT_ADVANCED
    assert ack.counter == 0
    assert bus.log.count("set") == 2


def test_a_refused_or_rejected_set_is_not_resent():
    c, bus = controller(AckPeripheral())
    assert c.send_confirmed(0x10, Message(TYPE, REFUSE)) == (
        SetOutcome.REFUSED,
        Ack(1, REFUSE, ACK_REFUSED, 0x2A),
    )
    assert c.send_confirmed(0x10, Message(TYPE, UNHANDLED))[0] is SetOutcome.REJECTED
    assert bus.log.count("set") == 2


def test_a_failed_ack_read_is_read_again_not_answered_with_a_resend():
    device = AckPeripheral()
    device.failed_reads = {1, 2}  # read 0 is the baseline
    c, bus = controller(device)
    outcome, ack = c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert outcome is SetOutcome.APPLIED
    assert ack.counter == 1
    assert bus.log == ["stage", "read", "set"] + ["stage", "read"] * 3


def test_an_unreadable_ack_after_the_send_is_raised():
    device = AckPeripheral()
    device.failed_reads = {1, 2, 3}
    c, bus = controller(device)
    with pytest.raises(OSError):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert bus.log.count("set") == 1
    assert device.state.counter == 1


def test_nothing_is_sent_without_a_baseline():
    c, bus = controller(AckPeripheral(ack=False))
    with pytest.raises(ReplyMismatch):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert "set" not in bus.log


def test_a_write_error_is_settled_by_the_ack():
    device = AckPeripheral()
    device.faults.append("fail-write")
    c, bus = controller(device)
    assert c.send_confirmed(0x10, Message(TYPE, APPLY))[0] is SetOutcome.APPLIED
    assert bus.log.count("set") == 1


def test_another_command_counted_is_an_ack_mismatch():
    device = AckPeripheral()
    c, _ = controller(device)
    original = device.write

    def write(frame: bytes) -> None:
        original(frame)
        if decode(frame).opcode == APPLY:
            original(encode(Message(TYPE, REFUSE)))

    device.write = write  # type: ignore[method-assign]
    with pytest.raises(AckMismatch) as e:
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert e.value.after.opcode == REFUSE
    assert e.value.opcode == APPLY


def test_send_confirmed_refuses_what_it_cannot_confirm():
    c, bus = controller(AckPeripheral())
    with pytest.raises(ValueError):
        c.send_confirmed(0x10, Message(TYPE, APPLY), sends=0)
    with pytest.raises(ValueError):
        c.send_confirmed(0x10, Message(0x00, SET_REPLY, bytes((GET_ACK,))))
    assert bus.log == []
