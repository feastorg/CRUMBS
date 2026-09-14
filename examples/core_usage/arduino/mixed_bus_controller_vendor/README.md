# mixed_bus_controller_vendor

`mixed_bus_controller` with the vendors' own libraries for the non-CRUMBS
devices: Atlas Scientific EZO pH (`0x63`) and DO (`0x61`) through
[ezo-driver](https://github.com/feastorg/ezo-driver) and a Bosch BME280
through SparkFun's library. CRUMBS candidates `0x10`–`0x12` as before.
Built in CI with ezo-driver v0.5.1 and SparkFun BME280 v2.0.11 pinned by
tag and commit.

## Output

Same skeleton (validation pass, then a status pass every 5 s, CRUMBS lines
identical to `mixed_bus_controller`), plus:

- `EZO pH addr=0x63` followed by ` init=not_ready`, ` startup_settle=pending`
  (first second), ` send_rc=… send_name=…`, or ` read_rc=… read_name=…
  status=… ph=<x.xxx>`. The driver sends `r` and waits the time it reports.
- `EZO DO addr=0x61` — queries the output configuration (`O,?`) once until
  it succeeds, then reads; prints ` output_mask=…`, ` present_mask=…` and
  ` mg_l=…` / ` sat_pct=…` as enabled.
- `Bosch addr=0x76` followed by ` init=fail` or ` temp_c=… pressure_pa=…
  pressure_hpa=… humidity_pct=<n|NA> model_hint=<BMP280_or_no_humidity|BME280>`.

## Build

```sh
git clone --depth 1 --branch v0.5.1 https://github.com/feastorg/ezo-driver third_party/ezo-driver
git clone --depth 1 --branch v2.0.11 https://github.com/sparkfun/SparkFun_BME280_Arduino_Library third_party/SparkFun_BME280
arduino-cli compile --fqbn arduino:avr:nano --warnings more --library "$PWD" \
  --library "$PWD/third_party/ezo-driver" --library "$PWD/third_party/SparkFun_BME280" \
  examples/core_usage/arduino/mixed_bus_controller_vendor
```

In the Arduino IDE install the two libraries by the same names.
