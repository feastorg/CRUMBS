"""A Linux i2c-dev bus, as the C library's Linux HAL drives it: select the
target with the I2C_SLAVE ioctl, then a plain write or read, each its own
transaction with a STOP after it."""

from __future__ import annotations

import fcntl
import os
from types import TracebackType

# linux/i2c-dev.h
_I2C_SLAVE = 0x0703


class LinuxBus:
    """``/dev/i2c-N``. A CRUMBS exchange never needs a repeated start: a GET
    is a write, a pause, then a read."""

    def __init__(self, path: str = "/dev/i2c-1"):
        self.path = path
        self._fd = os.open(path, os.O_RDWR)
        self._target: int | None = None

    def _select(self, address: int) -> None:
        if not 0x00 <= address <= 0x7F:
            raise ValueError(f"a 7-bit I2C address is 0x00-0x7F, not 0x{address:X}")
        if address != self._target:
            # EBUSY here means a kernel driver owns the address.
            fcntl.ioctl(self._fd, _I2C_SLAVE, address)
            self._target = address

    def write(self, address: int, data: bytes) -> None:
        self._select(address)
        written = os.write(self._fd, data)
        if written != len(data):
            raise OSError(f"wrote {written} of {len(data)} bytes to 0x{address:02X}")

    def read(self, address: int, count: int) -> bytes:
        self._select(address)
        return os.read(self._fd, count)

    def close(self) -> None:
        if self._fd >= 0:
            os.close(self._fd)
            self._fd = -1

    def __enter__(self) -> LinuxBus:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        self.close()
