# Footprint sketches

The two sketches behind the flash/RAM figures in
[docs/architecture.md](../../docs/architecture.md#memory). `bare_wire` is a
`Wire`-only peripheral of the same shape as `crumbs_min` (receive into a
buffer, answer a read with four bytes, no `Serial`).

```sh
arduino-cli compile --fqbn arduino:avr:uno --library "$PWD" tests/footprint/bare_wire
arduino-cli compile --fqbn arduino:avr:uno --library "$PWD" tests/footprint/crumbs_min
arduino-cli compile --fqbn arduino:avr:uno --library "$PWD" \
  --build-property compiler.c.extra_flags=-DCRUMBS_MAX_HANDLERS=0 \
  --build-property compiler.cpp.extra_flags=-DCRUMBS_MAX_HANDLERS=0 tests/footprint/crumbs_min
```

Not built in CI; the figures in the doc name the toolchain they were taken with.
