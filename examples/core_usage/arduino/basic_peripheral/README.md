# basic_peripheral

A peripheral with two commands and two queries, dispatched by hand in
`on_message` / `on_request`. Pair with `basic_controller` or
`advanced_controller`.

- Address `0x10`, type `0x01`, module version 1.0.0 (`config.h`).
- SET `0x01` stores up to 10 payload bytes; SET `0x02` clears them. Each
  frame prints `RX: cmd=<op> len=<n>` and `Data stored` / `Data cleared` /
  `Unknown command`.
- GET `0x00` replies the 5-byte version convention
  (`CRUMBS_VERSION` little-endian, then 1, 0, 0), packed by hand; GET `0x80`
  replies the stored bytes. The reply's opcode is whatever was requested.
- Boot prints `Basic peripheral ready at 0x10`.

Build: Arduino IDE, or `arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/basic_peripheral`.
