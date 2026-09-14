# basic_controller

Drives `basic_peripheral` and decodes what comes back with
`crumbs_decode_message`.

- Target `0x10`, type `0x01` (`config.h`).
- Serial (115200) prints a command list, then single characters:
  - `s` — store `AA BB CC` (opcode `0x01`), prints `Sent: Store data`.
  - `c` — clear (opcode `0x02`), prints `Sent: Clear data`.
  - `v` — SET_REPLY `0x00`, 10 ms, read, decode; prints `Version: 1.0.0`.
  - `d` — SET_REPLY `0x80`; prints `Stored data (<n> bytes): <hex>`.
- A reply that fails to decode prints nothing.

Build: Arduino IDE, or `arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/basic_controller`.
