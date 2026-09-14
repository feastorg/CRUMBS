# hello_controller

Sends a frame to `hello_peripheral` and reads its counter back.

- Target `0x10`, type `0x01` (`config.h`).
- Serial (115200) prints `Commands: s=send, r=request`, then reads one
  character at a time:
  - `s` — `crumbs_controller_send` of opcode `0x01` with payload `AA BB`,
    then `Sent!`. The peripheral prints `RX cmd=1`.
  - `r` — SET_REPLY for opcode `0x00`, a 10 ms delay, then a raw
    `crumbs_arduino_read` of up to 32 bytes; prints `Received <n> bytes`
    if anything came back. The bytes are not decoded — that is
    `basic_controller`'s job.

Build: Arduino IDE, or `arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/hello_controller`.
