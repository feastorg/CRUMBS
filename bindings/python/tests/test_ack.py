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
    ACK_MIN_VERSION,
    ACK_REFUSED,
    ACK_REJECTED,
    GET_ACK,
    MAX_FRAME,
    REJECT_NO_HANDLER,
    SET_REPLY,
    Ack,
    AckMismatch,
    AckUnknown,
    Controller,
    CrumbsError,
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
    before = Ack(*case["before"])
    after = Ack(*case["after"])
    if case["result"] >= 0:
        assert classify(before, after, case["opcode"]) == SetOutcome(case["result"])
    elif case["result"] == -9:
        with pytest.raises(AckMismatch):
            classify(before, after, case["opcode"])
    elif case["result"] == -10:
        with pytest.raises(AckUnknown):
            classify(before, after, case["opcode"])
    else:
        assert case["result"] == -1
        with pytest.raises(CrumbsError) as e:
            classify(before, after, case["opcode"])
        assert not isinstance(e.value, (AckMismatch, AckUnknown))


@pytest.mark.parametrize("case", VECTORS["ack_decode"], ids=lambda c: c["data"] or "empty")
def test_decode_ack_accepts_what_the_c_core_accepts(case):
    reply = Message(TYPE, GET_ACK, bytes.fromhex(case["data"]))
    if case["result"] == 0:
        assert decode_ack(reply) == Ack(*reply.data[:4])
    else:
        assert case["result"] == -7
        with pytest.raises(ReplyMismatch):
            decode_ack(reply)


def test_the_vectors_cover_every_outcome():
    assert {c["status"] for c in VECTORS["ack"]} == {0, 1, 2, 3}
    assert {c["result"] for c in VECTORS["ack_classify"]} == {0, 1, 2, 3, -1, -9, -10}
    assert {c["result"] for c in VECTORS["ack_decode"]} == {0, -7}


def test_decode_ack_refuses_what_is_not_an_acknowledgement():
    with pytest.raises(ReplyMismatch):
        decode_ack(Message(TYPE, GET_ACK, b"\x01\x02\x03"))
    with pytest.raises(ReplyMismatch):
        decode_ack(Message(TYPE, 0x00, b"\x01\x02\x03\x04"))
    # An echoing old peripheral: right opcode, a status that is none of the four.
    with pytest.raises(ReplyMismatch):
        decode_ack(Message(TYPE, GET_ACK, b"\x10\x11\x12\x13\x14\x15"))
    # Status none only with every other byte zero.
    with pytest.raises(ReplyMismatch):
        decode_ack(Message(TYPE, GET_ACK, b"\x01\x00\x00\x00"))
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
    every read with its version reply; ``echo=True`` one that answers by
    echoing the requested opcode with a status block of its own, as
    basic_peripheral's on_request does; ``echo_record=True`` one that
    answers GET_ACK with [5, 0x10, 1, 0], a valid record, and anything else
    with its version reply. The version reply carries ``version``, by
    default ACK_MIN_VERSION with ``ack`` and 1500 (0.15.0) without.
    ``reset_after`` restarts the device (a brown-out) after that many
    command writes."""

    def __init__(
        self,
        *,
        ack: bool = True,
        echo: bool = False,
        echo_record: bool = False,
        version: int | None = None,
        reset_after: int = 0,
    ):
        self.ack = ack
        self.echo = echo
        self.echo_record = echo_record
        if version is None:
            version = ACK_MIN_VERSION if ack and not echo_record else 1500
        self.version = version
        self.reset_after = reset_after
        self.commands = 0
        self.applied = 0
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
        self.applied += message.opcode == APPLY
        self.commands += 1
        if self.reset_after and self.commands == self.reset_after:
            self.state = Ack(0, 0, 0, 0)
            self.staged = 0x00
        if self.faults and self.faults[0] == "fail-write":
            self.faults.popleft()
            raise OSError(errno.EREMOTEIO, "Remote I/O error")

    def read(self, count: int) -> bytes:
        self.reads += 1
        if self.reads - 1 in self.failed_reads:
            raise OSError(errno.EREMOTEIO, "Remote I/O error")
        if self.echo:
            reply = Message(TYPE, self.staged, bytes(range(0x10, 0x16)))
        elif self.echo_record and self.staged == GET_ACK:
            reply = Message(TYPE, GET_ACK, bytes((5, 0x10, ACK_APPLIED, 0)))
        elif self.ack and self.staged == GET_ACK:
            reply = Message(TYPE, GET_ACK, bytes(self.state))
        else:
            reply = Message(TYPE, 0x00, self.version.to_bytes(2, "little") + b"\x01\x00\x00")
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
    assert bus.log == ["stage", "read"] * 2 + ["set", "stage", "read"]


def test_send_confirmed_resends_a_lost_frame():
    device = AckPeripheral()
    device.state = Ack(1, REFUSE, ACK_REFUSED, 0x2A)  # it has counted a command
    device.faults.append("drop")
    c, bus = controller(device)
    outcome, ack = c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert outcome is SetOutcome.APPLIED
    assert ack.counter == 2
    assert bus.log == ["stage", "read"] * 2 + ["set", "stage", "read"] * 2


def test_send_confirmed_stops_at_its_bound():
    device = AckPeripheral()
    device.state = Ack(1, REFUSE, ACK_REFUSED, 0x2A)
    device.faults.extend(["drop"] * 5)
    c, bus = controller(device)
    outcome, ack = c.send_confirmed(0x10, Message(TYPE, APPLY), sends=2)
    assert outcome is SetOutcome.NOT_ADVANCED
    assert ack.counter == 1
    assert bus.log.count("set") == 2


def test_a_lost_first_frame_to_a_fresh_device_is_not_resent():
    """Lost, and applied-then-restarted, read the same from a device that
    has counted nothing."""
    device = AckPeripheral()
    device.faults.append("drop")
    c, bus = controller(device)
    with pytest.raises(AckUnknown):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert bus.log.count("set") == 1


def test_a_reset_after_apply_from_a_zero_baseline_is_not_resent():
    device = AckPeripheral(reset_after=1)
    c, bus = controller(device)
    with pytest.raises(AckUnknown):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert device.applied == 1
    assert bus.log.count("set") == 1


def test_a_reset_after_apply_at_the_wrapped_counter_is_not_resent():
    device = AckPeripheral(reset_after=1)
    device.state = Ack(0, REFUSE, ACK_REFUSED, 0x2A)  # 256 commands in
    c, bus = controller(device)
    with pytest.raises(AckUnknown):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert device.applied == 1
    assert bus.log.count("set") == 1


def test_an_echoing_old_peripheral_is_sent_nothing():
    device = AckPeripheral(echo=True)
    c, bus = controller(device)
    with pytest.raises(ReplyMismatch):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert device.applied == 0
    assert "set" not in bus.log


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
    device.failed_reads = {2, 3}  # read 0 is the version, read 1 the baseline
    c, bus = controller(device)
    outcome, ack = c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert outcome is SetOutcome.APPLIED
    assert ack.counter == 1
    assert bus.log == ["stage", "read"] * 2 + ["set"] + ["stage", "read"] * 3


def test_an_unreadable_ack_after_the_send_is_raised():
    device = AckPeripheral()
    device.failed_reads = {2, 3, 4}
    c, bus = controller(device)
    with pytest.raises(OSError):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert bus.log.count("set") == 1
    assert device.state.counter == 1


def test_nothing_is_sent_without_a_baseline():
    c, bus = controller(AckPeripheral(ack=False, version=ACK_MIN_VERSION))
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


def test_send_confirmed_checks_the_version():
    """An older device whose echo of GET_ACK happens to form a valid record
    reads as an acknowledgement whose counter never moves. Its version reply
    tells it apart, and it is sent nothing."""
    device = AckPeripheral(echo_record=True)
    c, bus = controller(device)
    with pytest.raises(ReplyMismatch):
        c.send_confirmed(0x10, Message(TYPE, APPLY))
    assert device.applied == 0
    assert bus.log == ["stage", "read"]


def test_send_confirmed_refuses_what_it_cannot_confirm():
    c, bus = controller(AckPeripheral())
    with pytest.raises(ValueError):
        c.send_confirmed(0x10, Message(TYPE, APPLY), sends=0)
    with pytest.raises(ValueError):
        c.send_confirmed(0x10, Message(0x00, SET_REPLY, bytes((GET_ACK,))))
    assert bus.log == []
