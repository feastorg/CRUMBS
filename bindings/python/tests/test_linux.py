"""LinuxBus without hardware: the ioctl it makes, and when."""

import fcntl
import os
from pathlib import Path

import pytest

from crumbs_i2c._linux import LinuxBus


@pytest.fixture
def device(tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
    """A regular file in place of /dev/i2c-N, and a record of each ioctl."""
    path = tmp_path / "i2c-1"
    path.write_bytes(b"\x00\x00\x00\x00rest")
    calls: list[tuple[int, int]] = []
    monkeypatch.setattr(fcntl, "ioctl", lambda fd, request, arg: calls.append((request, arg)))
    return path, calls


def test_the_target_is_selected_with_i2c_slave_once_per_change(device):
    path, calls = device
    with LinuxBus(str(path)) as bus:
        assert bus.read(0x10, 4) == b"\x00\x00\x00\x00"
        bus.write(0x10, b"\x01")
        bus.write(0x11, b"\x02")
        bus.read(0x10, 1)
    assert calls == [(0x0703, 0x10), (0x0703, 0x11), (0x0703, 0x10)]


def test_an_address_beyond_seven_bits_is_refused(device):
    path, calls = device
    with LinuxBus(str(path)) as bus, pytest.raises(ValueError, match="7-bit"):
        bus.write(0x80, b"\x00")
    assert calls == []


def test_close_closes_the_device(device):
    path, _ = device
    bus = LinuxBus(str(path))
    fd = bus._fd  # pyright: ignore[reportPrivateUsage]
    bus.close()
    with pytest.raises(OSError):
        os.fstat(fd)
    bus.close()  # twice is fine


def test_a_missing_bus_is_an_oserror(tmp_path: Path):
    with pytest.raises(FileNotFoundError):
        LinuxBus(str(tmp_path / "i2c-9"))
