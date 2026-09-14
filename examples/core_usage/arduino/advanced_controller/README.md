# advanced_controller

A serial line protocol for sending arbitrary frames, reading replies,
scanning the bus and watching CRC statistics. Pair with `basic_peripheral`
(the only core example that answers opcode `0x80`) and put
`basic_peripheral_noncrumbs` on the bus to see a scan ignore it.

Serial (115200), one command per line:

| Line | Does |
| --- | --- |
| `<addr>,<type_id>,<opcode>[,<byte>…]` | encode and send; each field is parsed with `strtol(…, 0)`, so `0x10` or `16`, and only integers — a `75.0` truncates to 75. Prints the parsed message and `Controller: Message sent based on serial input.` |
| `request=<addr>` | `Wire.requestFrom(addr, 31)`, decode, print `type_id`, `opcode`, `data_len`, `data`, `crc8` — or `Controller: Failed to decode response.` No SET_REPLY is sent first, so the peripheral answers with whatever it last staged. |
| `scan` / `scan strict` | `crumbs_controller_scan_for_crumbs` over `0x03`–`0x77` (`config.h`), 50 ms per address; prints the addresses found. |

Boot prints the format and the three example lines `0x10,1,1,0x12,0x34`, `request=0x10` and `scan strict`.
Whenever the CRC counters change it prints
`Controller: CRC status [ctx] errors=<n> lastValid=<true|false>`.

Build: Arduino IDE (three files: the sketch, `serial_io.ino`, `config.h`), or
`arduino-cli compile --fqbn arduino:avr:nano --library "$PWD" examples/core_usage/arduino/advanced_controller`.
