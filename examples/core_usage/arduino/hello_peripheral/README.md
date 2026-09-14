# hello_peripheral

The smallest CRUMBS peripheral: count the frames it receives, hand the count
back when read. Pair with `hello_controller`.

- Address `0x10`, type `0x01` (`config.h`).
- Every received frame runs `on_message`, which prints `RX cmd=<opcode>` and
  increments a counter.
- Every read runs `on_request`, which replies `type 0x01, opcode 0x00,
  data = [counter]` whatever opcode was requested.
- Boot checks `crumbs_context_size()` against `sizeof(crumbs_context_t)` and
  halts with `FATAL: CRUMBS_MAX_HANDLERS mismatch! See crumbs.h.` if the
  library and sketch disagree; otherwise prints `Hello peripheral at 0x10`.

Build: open in the Arduino IDE, or
`arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/hello_peripheral`.
