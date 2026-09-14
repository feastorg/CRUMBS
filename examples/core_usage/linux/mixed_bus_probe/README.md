# mixed_bus_probe (Linux)

A command-line tool for a bus that mixes CRUMBS peripherals with register
devices: a candidate scan, and raw register reads and writes through the
`crumbs_i2c_dev_*` helpers. Built by the root CMake as
`build-linux/crumbs_mixed_bus_probe`.

```text
crumbs_mixed_bus_probe <i2c-dev> scan <addr_csv> [strict|non-strict]      default strict
crumbs_mixed_bus_probe <i2c-dev> read-u8  <addr> <reg>     <len> [repeat|norepeat]
crumbs_mixed_bus_probe <i2c-dev> read-u16 <addr> <reg16>   <len> [repeat|norepeat]
crumbs_mixed_bus_probe <i2c-dev> read-ex  <addr> <reg_csv> <len> [repeat|norepeat]
crumbs_mixed_bus_probe <i2c-dev> write-u8  <addr> <reg>     <data_csv>
crumbs_mixed_bus_probe <i2c-dev> write-u16 <addr> <reg16>   <data_csv>
crumbs_mixed_bus_probe <i2c-dev> write-ex  <addr> <reg_csv> <data_csv>
```

Numbers take `0x` hex or decimal; CSV lists are comma-separated with no
spaces (up to 32 candidates, 64 bytes); `len` is 1–64; a 16-bit register is
sent big-endian; reads default to a repeated start (`repeat`, `rs`, `1`;
`norepeat`, `0` for a STOP between phases). Exit 0 on success, 1 on an I/O
failure (`ERROR: sensor read failed (<rc>)`), 2 on bad arguments.

```sh
crumbs_mixed_bus_probe /dev/i2c-1 scan 0x10,0x11,0x12
crumbs_mixed_bus_probe /dev/i2c-1 read-u8 0x76 0xD0 1        # Bosch chip ID: 58 = BMP280, 60 = BME280
crumbs_mixed_bus_probe /dev/i2c-1 read-ex 0x1E 0x20,0x08 6 repeat
```
