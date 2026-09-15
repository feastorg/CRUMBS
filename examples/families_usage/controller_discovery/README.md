# LHWIT discovery controller (Linux)

Finds LHWIT devices on the bus, checks each one's version against the
headers it was built with, and drives the compatible ones. The command
grammar is in the [family README](../lhwit_family/README.md#shell).

```sh
build-linux/crumbs_controller_discovery [/dev/i2c-1]
```

Built by the root CMake with `CRUMBS_ENABLE_LINUX_HAL=ON`, or on its own from its directory (`cmake -S examples/families_usage/controller_discovery -B build`,
which builds the library alongside; add `-DCRUMBS_BUILD_IN_TREE=OFF` to use an installed CRUMBS instead).

## What `scan` does

Sweeps `0x08`–`0x77` with the non-strict CRUMBS probe
(`crumbs_linux_scan_for_crumbs_with_types`, 100 ms timeout, up to 16
devices), names each by `type_id`, then sends SET_REPLY `0x00` and reads the
version reply. A device is bound only if its reply parses, its CRUMBS
version is at least 0.10.0 and its module major matches and minor is at
least the header's:

```text
Scanning I2C bus for CRUMBS devices (0x08-0x77)...

Found 1 device(s):
--------------------------------------------
[0x20] LED
       CRUMBS: v0.14.0 (controller: v0.14.0)
       Module: v1.0.0 (expected: v1.0.x)
       OK Compatible
--------------------------------------------
Usable: 1/1 devices
```

Failure lines in the same slot: `X CRUMBS version too old`, `X Module major
version mismatch`, `X Module minor version too old`, `! Version query
failed`, `! Invalid version format`. `list` shows what is bound, with
`OK` or `INCOMPATIBLE`; an incompatible device can still be addressed with
`@addr`, which prints `Device at 0x.. is incompatible. Run 'scan' again after
updating firmware.` Index selectors (`led 0 …`) count compatible devices
only.

The version reply is read with `crumbs_linux_read_message()`, which is
deprecated in favour of `crumbs_controller_read()`; this is its last use in
the examples.

## Compatibility rules

From `lhwit_ops.h`: `lhwit_check_crumbs_compat()` requires `CRUMBS_VERSION ≥
1000`; `lhwit_check_module_compat()` returns `-1` on a major mismatch and
`-2` when the peripheral's minor is below the expected one. A family bumps
major for incompatible opcode or payload changes and minor for additions, so
a newer peripheral serves an older controller but not the reverse.
