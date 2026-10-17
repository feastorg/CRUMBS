"""The opcode 0x00 version reply: parsed as crumbs_build_version_reply()
builds it (docs/protocol.md), and asked for over a fake bus."""

import re
from pathlib import Path

import pytest

from crumbs_i2c import (
    MAX_FRAME,
    SET_REPLY,
    TYPE_ID_ANY,
    Controller,
    Message,
    ReplyMismatch,
    Version,
    decode,
    decode_version,
    encode,
)

ROOT = Path(__file__).parents[3]
PAD = b"\xff" * MAX_FRAME
TYPE = 0x11


def crumbs_version() -> int:
    header = (ROOT / "src" / "crumbs_version.h").read_text()
    match = re.search(r"#define CRUMBS_VERSION (\d+)", header)
    assert match is not None
    return int(match.group(1))


def version_reply(type_id: int, major: int, minor: int, patch: int, version: int) -> Message:
    """The frame crumbs_build_version_reply() builds: crumbs_msg_add_u16()
    of CRUMBS_VERSION, little-endian, then the three module bytes."""
    return Message(type_id, 0x00, version.to_bytes(2, "little") + bytes((major, minor, patch)))


def test_decode_version_reads_what_the_c_peripheral_builds():
    version = crumbs_version()
    reply = version_reply(TYPE, 1, 2, 3, version)
    assert decode_version(reply) == Version(version, 1, 2, 3)


def test_the_crumbs_version_is_little_endian():
    # 1600 is 0x0640: low byte first.
    assert decode_version(Message(TYPE, 0x00, b"\x40\x06\x01\x00\x00")).crumbs_version == 1600


def test_decode_version_refuses_what_is_not_a_version_reply():
    with pytest.raises(ReplyMismatch):
        decode_version(Message(TYPE, 0x80, b"\x40\x06\x01\x00\x00"))
    with pytest.raises(ReplyMismatch):
        decode_version(Message(TYPE, 0x00, b"\x40\x06\x01\x00"))
    # Trailing bytes are how the layout is extended.
    assert decode_version(Message(TYPE, 0x00, b"\x40\x06\x01\x02\x03\x04")) == Version(
        1600, 1, 2, 3
    )


class Device:
    """A peripheral that answers opcode 0x00 with its version reply and any
    other staged opcode with an empty reply. ``pad_reads`` reads come back
    all 0xFF first, as a reply that lost a clock edge."""

    def __init__(self, type_id: int = TYPE, pad_reads: int = 0):
        self.type_id = type_id
        self.staged = 0x00
        self.pad_reads = pad_reads
        self.log: list[tuple[str, bytes | int]] = []

    def write(self, address: int, data: bytes) -> None:
        self.log.append(("write", data))
        message = decode(data)
        if message.opcode == SET_REPLY:
            self.staged = message.data[0]

    def read(self, address: int, count: int) -> bytes:
        self.log.append(("read", count))
        if self.pad_reads:
            self.pad_reads -= 1
            return PAD[:count]
        if self.staged == 0x00:
            reply = version_reply(self.type_id, 2, 5, 7, 1600)
        else:
            reply = Message(self.type_id, self.staged, b"")
        return (encode(reply) + PAD)[:count]


def controller(device: Device) -> Controller:
    return Controller(device, sleep=lambda _: None)


def test_get_version_stages_opcode_0x00_and_parses_the_reply():
    device = Device()
    device.staged = 0x80
    assert controller(device).get_version(0x10) == Version(1600, 2, 5, 7)
    assert device.log == [
        ("write", encode(Message(TYPE_ID_ANY, SET_REPLY, b"\x00"))),
        ("read", MAX_FRAME),
    ]


def test_get_version_checks_the_type_when_asked():
    device = Device(type_id=0x22)
    assert controller(device).get_version(0x10, type_id=0x22) == Version(1600, 2, 5, 7)
    with pytest.raises(ReplyMismatch) as e:
        controller(device).get_version(0x10, type_id=TYPE)
    assert e.value.wrong_device


def test_get_version_is_retried_as_a_query_is():
    device = Device(pad_reads=2)
    assert controller(device).get_version(0x10) == Version(1600, 2, 5, 7)
    assert [kind for kind, _ in device.log] == ["write", "read"] * 3
