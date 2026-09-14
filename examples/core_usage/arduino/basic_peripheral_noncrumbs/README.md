# basic_peripheral_noncrumbs

A plain `Wire` device with no CRUMBS code at all, to put next to CRUMBS
peripherals when testing scans and mixed buses.

- Address `69` (`0x45`, `config.h`).
- Prints `non-CRUMBS peripheral: received bytes=<n>` for any write and
  answers any read with the eight ASCII bytes `NOCRUMBS`, which no CRUMBS
  decoder accepts (`data_len` would be `0x43`).
- Boot waits for the serial port, then prints
  `Starting non-CRUMBS peripheral at address 0x45`.

Build: Arduino IDE, or `arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/basic_peripheral_noncrumbs`.
