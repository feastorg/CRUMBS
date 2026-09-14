# simple_controller (Linux)

Send one frame to a peripheral and read its reply, or scan the bus.
Built by the root CMake with the Linux HAL as `build-linux/crumbs_simple_linux_controller`;
CI also builds it out of tree against an installed CRUMBS and smoke-runs
the `--help` and `scan` forms.

```sh
crumbs_simple_linux_controller [i2c-device] [peripheral-addr]   # default /dev/i2c-1 0x10
crumbs_simple_linux_controller scan [strict]                    # /dev/i2c-1, 0x03-0x77
crumbs_simple_linux_controller --help
```

The send form writes `type 1, opcode 1` with three native floats
(`12.34, 5.0, 9.87`), prints `Message sent (12 payload bytes).`, then reads
without sending a SET_REPLY — so the peripheral answers with whatever it last
staged, opcode `0x00` on a fresh boot — and prints the decoded reply and the
CRC statistics. The scan
form prints `Found <n> CRUMBS device(s):` and each address.

A bus that cannot be opened prints
`ERROR: crumbs_linux_init_controller failed (-2) for device /dev/i2c-1`
(after linux-wire's own `open:` message) and exits 1.

The read uses `crumbs_linux_read_message()`, which is deprecated in favour
of `crumbs_controller_read()`.
