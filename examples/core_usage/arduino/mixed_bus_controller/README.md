# mixed_bus_controller

One controller, a bus with CRUMBS peripherals and a Bosch BMP280/BME280,
driven with the library only: `crumbs_controller_scan_for_crumbs_candidates`
for the CRUMBS side and the `crumbs_i2c_dev_*` register helpers for the
sensor. Built in CI (`arduino-cli`, Nano).

## Bus

- CRUMBS candidates `0x10`, `0x11`, `0x12` (`kCrumbsCandidates[]` in
  `config.h`): flash `basic_peripheral` at `0x10`, and for more boards set
  `DEVICE_ADDR` to `0x11` and `0x12` before flashing each.
- Bosch sensor at `0x76` or `0x77` (`SDO` low/high). Chip ID register
  `0xD0` (`0x58` BMP280, `0x60` BME280); raw data at `0xF7`, 6 bytes BMP /
  8 bytes BME. On the first pass the controller writes `0xF2 ← 0x01` (BME
  only) and `0xF4 ← 0x27` to start normal-mode sampling.

## Output

Serial 115200. `=== Validation pass (once at startup) ===`, then
`=== Status pass ===` every 5 s. Each pass runs a strict candidate scan —
`CRUMBS scan result: <n>` and `  addr=0x.. type=0x..` per device — then for
each found device a SET_REPLY `0x00` and read:

```text
CRUMBS addr=0x10 type=0x01 reply_op=0x00 len=5 data=<hex> crumbs_ver=0x0578 module=1.0.0
```

(`0x0578` is 1400, library 0.14.0). Failures print `send_rc=` / `read_n=` /
`decode_rc=… raw=…`. Then each sensor address:
`Sensor addr=0x76 chip_rc=0 chip_id=0x60 model=BME280 raw_rc=0 raw=<hex>`.
An absent sensor shows a non-zero `chip_rc`.

Build: `arduino-cli compile --fqbn arduino:avr:nano --warnings more --library "$PWD" examples/core_usage/arduino/mixed_bus_controller`.
