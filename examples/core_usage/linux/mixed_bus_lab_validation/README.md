# mixed_bus_lab_validation (Linux)

The pass/fail check for the lab's shared bus: three CRUMBS slices, two Atlas
Scientific EZO circuits and an optional Bosch sensor. Built by the root
CMake as `build-linux/crumbs_mixed_bus_lab_validation [i2c-dev]`
(default `/dev/i2c-1`); prints `RESULT: PASS` or `RESULT: FAIL` and exits
0/1.

Expected devices (constants at the top of `main.c`): CRUMBS at `0x0A`,
`0x14`, `0x15` — the lab hardware keeps these addresses; the shipped
examples moved to `0x10` — EZO pH at `0x63`, EZO DO at `0x61`, Bosch at
`0x76`/`0x77`.

What decides the result:

1. The bus opens.
2. A strict candidate scan runs and its findings are printed, but they are
   informational: since `4695a30` the criterion is the direct query below,
   not the scan.
3. Every CRUMBS address must answer a SET_REPLY `0x00` query with a frame
   `crumbs_controller_read()` decodes; the reply's identity is not checked.
   Failure prints `CRUMBS validation failed at expected addr=0x..`.
4. Both EZO circuits must answer `R` (after a 1 s wait) with status byte 1.
5. The Bosch sensor never affects the result; it is reported if present.
