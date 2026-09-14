# LHWIT manual controller (Linux)

Drives a fixed list of LHWIT devices with no scan and no version check —
the shape of a deployed controller that knows its bus. The command grammar
is in the [family README](../lhwit_family/README.md#shell).

```sh
build-linux/crumbs_controller_manual [/dev/i2c-1]
```

Built by the root CMake with `CRUMBS_ENABLE_LINUX_HAL=ON`, or on its own
against an installed CRUMBS (`cmake -S examples/families_usage/controller_manual -B build -DCRUMBS_BUILD_IN_TREE=OFF`).

## Configuration

`config.h` holds `DEVICE_CONFIG[]`, an array of `{type_id, address}`; the
default is the family's four devices at `0x10`, `0x20`, `0x30`, `0x40`. Add
a line per extra device. At start it prints `Loaded 4 device(s) from
config.h`; `list` shows `[0] Calculator at 0x10 (Type 0x03, Index 0)` and so
on, and a selector that misses says `Device at 0x.. not found (run 'list' to
see devices)`.

## Output

Confirmations name the target, unlike the discovery controller's:

```text
lhwit> calculator 0 add 42 8
OK: add(42, 8) sent to 0x10. Use 'calculator result' to get answer.
lhwit> led 0 set_all 0x0F
OK: LEDs at 0x20 set to 0x0F
lhwit> servo 0 set_pos 0 90
OK: Servo 0 at 0x30 position set to 90deg
```

Queries print the same lines as the discovery controller with `at 0x..`
added, e.g. `LED state at 0x20: 0x0F (1111)`.

Binding a device is three lines (`main.c`): a `crumbs_device_t` with the
context, the address and the Linux HAL's write, read and delay functions.
That is the whole integration surface for a family.
