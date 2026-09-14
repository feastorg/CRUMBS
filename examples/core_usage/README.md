# Core usage

Direct use of the library: contexts, callbacks, `crumbs_controller_send`,
SET_REPLY and decoding. No handler tables and no families.

| | Arduino IDE | PlatformIO | Linux |
| --- | --- | --- | --- |
| Peripheral | `hello_peripheral`, `basic_peripheral`, `basic_peripheral_noncrumbs` | `simple_peripheral` | — (controller only) |
| Controller | `hello_controller`, `basic_controller`, `advanced_controller`, `mixed_bus_controller`, `mixed_bus_controller_vendor` | `simple_controller` | `simple_controller`, `mixed_bus_probe`, `mixed_bus_lab_validation` |

Start with the `hello_*` pair, then `basic_*`, then `advanced_controller`
against `basic_peripheral`. Every CRUMBS peripheral here listens at `0x10`;
every Arduino IDE sketch blinks `LED_BUILTIN` every 500 ms so you can see it
is alive; serial is 115200 baud everywhere. Each directory's README has the exact commands
and output.
