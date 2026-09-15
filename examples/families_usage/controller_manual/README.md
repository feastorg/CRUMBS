# LHWIT manual controller (Linux)

Drives a fixed list of LHWIT devices with no scan and no version check —
the shape of a deployed controller that knows its bus. The command grammar
is in the [family README](../lhwit_family/README.md#shell).

```sh
build-linux/crumbs_controller_manual [/dev/i2c-1]
```

Built by the root CMake with `CRUMBS_ENABLE_LINUX_HAL=ON`, or on its own from its directory (`cmake -S examples/families_usage/controller_manual -B build`,
which builds the library alongside; add `-DCRUMBS_BUILD_IN_TREE=OFF` to use an installed CRUMBS instead).

## Configuration

`config.h` holds `DEVICE_CONFIG[]`, an array of `{type_id, address}`; the
default is the family's four devices at `0x10`, `0x20`, `0x30`, `0x40`. Add
a line per extra device. At start it prints `Loaded 4 device(s) from
config.h`; `list` shows `[0] Calculator at 0x10 (Type 0x03, Index 0)` and so
on, and a selector that misses says `Device at 0x.. not found (run 'list' to
see devices)`.

## Output

Calculator, LED and servo confirmations and query lines name the target,
unlike the discovery controller's; display lines, `Result:` and `History:`
are identical in both:

```text
lhwit> calculator 0 add 42 8
OK: add(42, 8) sent to 0x10. Use 'calculator result' to get answer.
lhwit> led 0 set_all 0x0F
OK: LEDs at 0x20 set to 0x0F
lhwit> servo 0 set_pos 0 90
OK: Servo 0 at 0x30 position set to 90deg
```

Binding a device is one `crumbs_device_t` per entry (`main.c`): the
context, the address, the Linux HAL's write, read and delay functions, and
the bus handle. That is the whole integration surface for a family.
