"""The frame functions against what the C core does with the same inputs,
from tests/golden_vectors/vectors.json (written by gen_vectors.c)."""

import json
import re
from pathlib import Path

import pytest

import crumbs_i2c
from crumbs_i2c import FrameError, Message, crc8, decode, encode, frame_length

ROOT = Path(__file__).parents[3]
VECTORS = json.loads((ROOT / "tests" / "golden_vectors" / "vectors.json").read_text())


def test_the_vectors_come_from_this_version_of_the_c_library():
    header = (ROOT / "src" / "crumbs_version.h").read_text()
    version = re.search(r'#define CRUMBS_VERSION_STRING "([^"]+)"', header)
    assert version is not None
    assert VECTORS["crumbs_version"] == version.group(1)


def test_the_package_version_is_the_c_librarys():
    from importlib.metadata import version

    assert version("crumbs-i2c") == VECTORS["crumbs_version"]


@pytest.mark.parametrize("case", VECTORS["crc8"], ids=lambda c: c["input"][:16] or "empty")
def test_crc8(case):
    assert crc8(bytes.fromhex(case["input"])) == case["crc"]


@pytest.mark.parametrize("case", VECTORS["encode"])
def test_encode(case):
    message = Message(case["type_id"], case["opcode"], bytes.fromhex(case["data"]))
    assert encode(message).hex() == case["frame"]


@pytest.mark.parametrize("case", VECTORS["decode"], ids=lambda c: c["buffer"][:16] or "empty")
def test_decode(case):
    buffer = bytes.fromhex(case["buffer"])
    if case["result"] == 0:
        expected = Message(case["type_id"], case["opcode"], bytes.fromhex(case["data"]))
        assert decode(buffer) == expected
    else:
        with pytest.raises(FrameError) as e:
            decode(buffer)
        # -2 is the C library's CRC mismatch; -1 every other refusal.
        assert e.value.crc == (case["result"] == -2)


@pytest.mark.parametrize("case", VECTORS["frame_length"], ids=lambda c: c["buffer"][:16] or "empty")
def test_frame_length(case):
    buffer = bytes.fromhex(case["buffer"])
    if case["frame_length"] is None:
        with pytest.raises(FrameError):
            frame_length(buffer)
    else:
        assert frame_length(buffer) == case["frame_length"]


def test_the_vectors_cover_every_outcome():
    results = {c["result"] for c in VECTORS["decode"]}
    assert results == {0, -1, -2}
    assert any(c["frame_length"] is None for c in VECTORS["frame_length"])
    assert any(c["frame_length"] is not None for c in VECTORS["frame_length"])


def test_encode_refuses_what_does_not_fit_a_frame():
    with pytest.raises(ValueError, match="at most 27 bytes"):
        encode(Message(1, 2, bytes(28)))
    with pytest.raises(ValueError, match="type_id must be 0-255"):
        encode(Message(256, 2))
    with pytest.raises(ValueError, match="opcode must be 0-255"):
        encode(Message(1, -1))


def test_the_protocol_constants_match_the_headers():
    headers = (ROOT / "src" / "crumbs.h").read_text() + (
        ROOT / "src" / "crumbs_message.h"
    ).read_text()

    def define(name: str) -> int:
        match = re.search(rf"#define {name} (0x[0-9A-Fa-f]+|\d+)u?\b", headers)
        assert match is not None, name
        return int(match.group(1), 0)

    assert crumbs_i2c.MAX_PAYLOAD == define("CRUMBS_MAX_PAYLOAD")
    assert crumbs_i2c.SET_REPLY == define("CRUMBS_CMD_SET_REPLY")
    assert crumbs_i2c.TYPE_ID_ANY == define("CRUMBS_TYPE_ID_ANY")
    assert crumbs_i2c.MAX_FRAME == 3 + define("CRUMBS_MAX_PAYLOAD") + 1
