# Mixed Bus Controller Vendor (Arduino)

This example validates one shared I2C bus with three device classes at once:

- CRUMBS peripherals (`basic_peripheral`)
- Atlas EZO command/response devices (pH + DO) via `ezo-driver`
- Bosch BMP/BME280 register sensors via SparkFun BME280 library

The existing `mixed_bus_controller` example remains the low-level raw-register reference. This vendor example shows the same mixed-bus concept using real external device libraries.

## Dependencies (External)

Install these in your Arduino libraries environment:

1. `CRUMBS`
2. `ezo-driver` (your repo)
3. `SparkFun BME280 Arduino Library`

Notes:

- `ezo-driver` is expected to be available as an Arduino library (local clone/copy is fine).
- SparkFun BME280 can be installed via Arduino Library Manager.
- This example intentionally does not vendor those libraries into CRUMBS.

## Behavior

At startup:

1. initializes CRUMBS controller context,
2. initializes EZO transport/devices on the same `Wire` bus,
3. initializes configured BMP/BME sensors,
4. runs one validation pass.

Then every `STATUS_INTERVAL_MS` (default 5000 ms) it runs a status pass.

Each pass prints:

1. CRUMBS scan and per-device query reply
2. EZO pH read status/value (via `ezo_ph_*` typed helpers)
3. EZO DO output config + read status/value(s) (via `ezo_do_*` typed helpers)
4. Bosch sensor measurement lines for each configured address

Per-device failures are non-fatal: one missing/failed device does not stop the rest of the pass.

## Config Knobs

See `config.h`:

- `kCrumbsCandidates[]`
- `EZO_PH_ADDR`, `EZO_DO_ADDR`
- `kBoschSensorAddrs[]`, `BOSCH_SENSOR_COUNT`
- `STATUS_INTERVAL_MS`, `HEARTBEAT_INTERVAL_MS`

Defaults:

- CRUMBS candidates: `0x10`, `0x11`, `0x12`
- EZO pH: `0x63`
- EZO DO: `0x61`
- Bosch sensors: `0x76`, `0x77`

## Topology Profiles

### Minimal

- 1x controller running `mixed_bus_controller_vendor`
- 1x CRUMBS peripheral running `basic_peripheral` at `0x10`
- 1x EZO pH at `0x63`
- 1x EZO DO at `0x61`
- 1x BMP/BME280 at `0x76`

Set `BOSCH_SENSOR_COUNT = 1` for this profile.

### Max Validation

- 1x controller running `mixed_bus_controller_vendor`
- 3x CRUMBS peripherals at `0x10`, `0x11`, `0x12`
- 1x EZO pH at `0x63`
- 1x EZO DO at `0x61`
- 2x BMP/BME280 at `0x76`, `0x77`

## Wiring Checklist

- Shared `SDA`, `SCL`, and `GND` across all devices.
- One pull-up network for SDA/SCL (many breakout boards already include pull-ups).
- Every I2C address on the bus must be unique.
- EZO devices must already be in I2C mode.

## Flash Sequence for CRUMBS Peripherals

Use `basic_peripheral` for all CRUMBS nodes:

1. Flash board #1 with `DEVICE_ADDR = 0x10`
2. Flash board #2 with `DEVICE_ADDR = 0x11`
3. Flash board #3 with `DEVICE_ADDR = 0x12`
4. Flash this vendor controller sketch on the controller board

## Expected Serial Patterns

Validation pass once at startup:

```text
=== Validation pass (once at startup) ===
CRUMBS scan result: 3
  addr=0x10 type=0x01
  addr=0x11 type=0x01
  addr=0x12 type=0x01
CRUMBS addr=0x10 type=0x01 reply_op=0x00 len=5 data=...
CRUMBS addr=0x11 type=0x01 reply_op=0x00 len=5 data=...
CRUMBS addr=0x12 type=0x01 reply_op=0x00 len=5 data=...
EZO pH addr=0x63 read_rc=0 read_name=EZO_OK status=EZO_STATUS_SUCCESS ph=...
EZO DO addr=0x61 output_mask=0x.. read_rc=0 read_name=EZO_OK status=EZO_STATUS_SUCCESS present_mask=0x.. mg_l=... sat_pct=...
Bosch addr=0x76 temp_c=... pressure_pa=... pressure_hpa=... humidity_pct=... model_hint=...
Bosch addr=0x77 temp_c=... pressure_pa=... pressure_hpa=... humidity_pct=... model_hint=...
```

Periodic pass every `STATUS_INTERVAL_MS`:

```text
=== Status pass ===
...same line families repeated...
```

## Negative Checks

- Wrong EZO address: EZO line shows error/status, CRUMBS and Bosch lines still continue.
- Missing Bosch sensor on one configured address: that line prints `init=fail`, others still continue.
- Duplicate I2C addresses are invalid and can cause contention/corrupted behavior.

## Primary References

- SparkFun BME280 Arduino library:
  - https://github.com/sparkfun/SparkFun_BME280_Arduino_Library
  - https://raw.githubusercontent.com/sparkfun/SparkFun_BME280_Arduino_Library/master/src/SparkFunBME280.h
- Atlas EZO pH + protocol references:
  - https://atlas-scientific.com/embedded-solutions/ezo-ph-circuit/
  - https://files.atlas-scientific.com/pH_EZO_Datasheet.pdf
  - https://files.atlas-scientific.com/EZO-PRS-Datasheet.pdf
