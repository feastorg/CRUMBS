# Examples

Every example, by directory. Arduino IDE sketches build with the tree as the
library; PlatformIO projects pin the published package; Linux programs build
with the root CMake and the Linux HAL
([platform-setup.md](../docs/platform-setup.md)).

## Learning order

1. `core_usage/arduino/hello_peripheral` + `hello_controller` — one SET, one
   read, no decoding.
2. `core_usage/arduino/basic_peripheral` + `basic_controller` — dispatch on
   opcode, the version reply, decoding replies.
3. `core_usage/arduino/advanced_controller` — scan, CRC statistics, a serial
   line protocol; pairs with `basic_peripheral`.
4. `handlers_usage/` — the handler tables and the ops-header pattern over a
   `crumbs_device_t`.
5. `families_usage/` — a complete family with reply handlers, version checks
   and two Linux controllers.

The `mixed_bus_*` examples show CRUMBS peripherals sharing a bus with Bosch
and Atlas Scientific sensors.

## Catalog

| Directory | Kind | Role | Shows | CI |
| --- | --- | --- | --- | --- |
| `core_usage/arduino/hello_peripheral` | sketch | peripheral | `on_message`/`on_request`, the `crumbs_context_size()` guard | — |
| `core_usage/arduino/hello_controller` | sketch | controller | `crumbs_controller_send`, SET_REPLY + raw read | — |
| `core_usage/arduino/basic_peripheral` | sketch | peripheral | opcode switch, `requested_opcode`, version reply | — |
| `core_usage/arduino/basic_controller` | sketch | controller | `crumbs_decode_message` on replies | — |
| `core_usage/arduino/advanced_controller` | sketch | controller | `crumbs_controller_scan_for_crumbs`, CRC stats, CSV serial input | — |
| `core_usage/arduino/basic_peripheral_noncrumbs` | sketch | non-CRUMBS | a `Wire` device that answers `NOCRUMBS`, for scan tests | — |
| `core_usage/arduino/mixed_bus_controller` | sketch | controller | candidate scan, `crumbs_device_t`, raw BMP/BME280 registers | arduino-cli |
| `core_usage/arduino/mixed_bus_controller_vendor` | sketch | controller | the same with the vendor EZO and SparkFun libraries | arduino-cli |
| `core_usage/platformio/simple_peripheral` | PlatformIO | peripheral | fixed reply, message dump | platformio |
| `core_usage/platformio/simple_controller` | PlatformIO | controller | CSV send-only shell | platformio |
| `core_usage/linux/simple_controller` | CMake | controller | send + read, `scan [strict]` | build-and-test (+ smoke run) |
| `core_usage/linux/mixed_bus_probe` | CMake | tool | candidate scan and raw register I/O from the command line | build-and-test |
| `core_usage/linux/mixed_bus_lab_validation` | CMake | bench check | pass/fail sweep of the lab bus | build-and-test |
| `handlers_usage/platformio/mock_peripheral` | PlatformIO | peripheral | `crumbs_register_handler`, `on_request` for GETs | platformio |
| `handlers_usage/platformio/mock_controller` | PlatformIO | controller | `mock_ops.h` wrappers over `crumbs_device_t` | platformio |
| `handlers_usage/linux/mock_controller` | CMake | controller | the same shell on Linux | build-and-test |
| `families_usage/lhwit_family/{led,servo,calculator,display}` | PlatformIO | peripherals | reply handlers, `crumbs_build_version_reply`, `on_request` fallback | platformio |
| `families_usage/controller_discovery` | CMake | controller | scan with types, version compatibility, device binding | build-and-test |
| `families_usage/controller_manual` | CMake | controller | fixed device table | build-and-test |

"—" means not compiled in CI; those sketches compile with
`arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" <dir>`.
