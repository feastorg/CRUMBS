# Mixed Bus Controller (Arduino)

This controller validates one mixed I2C bus with:

- CRUMBS peripherals running `basic_peripheral`
- register-based non-CRUMBS sensors (BMP280/BME280)

Behavior is intentionally simple:

1. At startup, runs one **validation pass**.
2. Then every `STATUS_INTERVAL_MS` (default 5000 ms), runs a **status pass**.

Each pass:

1. scans only `kCrumbsCandidates[]`,
2. queries each discovered CRUMBS device (`SET_REPLY` -> opcode `0x00`),
3. reads each configured sensor in `kSensorAddrs[]`:
   - chip ID from `0xD0`,
   - raw sample bytes from `0xF7` onward (6 bytes BMP280, 8 bytes BME280).

## Topology Profiles

### Minimal (default in `config.h`)

- 1x Arduino controller (`mixed_bus_controller`)
- 1x Arduino CRUMBS peripheral (`basic_peripheral`) at `0x10`
- 1x BMP/BME280 at `0x76`
- `SENSOR_COUNT = 1`

### Max Validation

- 1x Arduino controller (`mixed_bus_controller`)
- 3x Arduino CRUMBS peripherals (`basic_peripheral`) at `0x10`, `0x11`, `0x12`
- 2x BMP/BME280 sensors at `0x76`, `0x77`
- `SENSOR_COUNT = 2`

## Wiring Checklist

- All devices share `SDA`, `SCL`, and `GND`.
- Ensure one pull-up network for SDA/SCL (many breakouts already include pull-ups).
- CRUMBS peripheral addresses must be unique on the bus.
- Sensor addresses must not collide with CRUMBS addresses.

## Flash Sequence for CRUMBS Peripherals

Use `basic_peripheral` for every CRUMBS node:

1. Set `DEVICE_ADDR = 0x10` in `basic_peripheral/config.h`, flash board #1.
2. Set `DEVICE_ADDR = 0x11`, flash board #2.
3. Set `DEVICE_ADDR = 0x12`, flash board #3.
4. Flash `mixed_bus_controller` on controller board.

## Controller Config Knobs

`mixed_bus_controller/config.h` is the full test-topology config:

- `kCrumbsCandidates[]`: CRUMBS addresses to probe (`0x10,0x11,0x12` default)
- `kSensorAddrs[]`: supported sensor addresses (`0x76,0x77` default)
- `SENSOR_COUNT`: number of entries from `kSensorAddrs[]` to read
- `STATUS_INTERVAL_MS`: periodic status interval (default 5000 ms)

## Expected Serial Output Patterns

### Minimal Scenario

```text
=== Validation pass (once at startup) ===
CRUMBS scan result: 1
  addr=0x10 type=0x01
CRUMBS addr=0x10 type=0x01 reply_op=0x00 len=5 data=0x.. 0x.. 0x01 0x00 0x00 crumbs_ver=0x.... module=1.0.0
Sensor addr=0x76 chip_rc=0 chip_id=0x58 model=BMP280 raw_rc=0 raw=0x.. 0x.. 0x.. 0x.. 0x.. 0x..

=== Status pass ===
CRUMBS scan result: 1
  addr=0x10 type=0x01
CRUMBS addr=0x10 type=0x01 reply_op=0x00 len=5 data=...
Sensor addr=0x76 chip_rc=0 chip_id=0x58 model=BMP280 raw_rc=0 raw=...
```

(`chip_id=0x60 model=BME280` is also valid; raw length is 8 bytes for BME280.)

### Max Scenario

```text
=== Validation pass (once at startup) ===
CRUMBS scan result: 3
  addr=0x10 type=0x01
  addr=0x11 type=0x01
  addr=0x12 type=0x01
CRUMBS addr=0x10 type=0x01 reply_op=0x00 len=5 data=...
CRUMBS addr=0x11 type=0x01 reply_op=0x00 len=5 data=...
CRUMBS addr=0x12 type=0x01 reply_op=0x00 len=5 data=...
Sensor addr=0x76 chip_rc=0 chip_id=0x58 model=BMP280 raw_rc=0 raw=...
Sensor addr=0x77 chip_rc=0 chip_id=0x60 model=BME280 raw_rc=0 raw=...
```

## Negative Sanity Checks

- Duplicate CRUMBS address (for example two peripherals at `0x10`) is invalid and can cause bus contention.
- If `kCrumbsCandidates[]` omits active peripherals, discovery count drops, but sensor reads still run.
- If sensor `chip_rc` or `raw_rc` is non-zero, check sensor power/address wiring and pull-ups.

## References (Primary Sources)

- BMP280 datasheet (Bosch): https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf
- BME280 datasheet (Bosch): https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf
