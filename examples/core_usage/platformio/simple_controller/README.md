# simple_controller (PlatformIO)

A send-only serial shell: type a frame as CSV, it goes on the bus. Pair with
`simple_peripheral`, whose monitor shows what arrived.

- Serial 115200, one line per frame: `addr,type_id,opcode[,byte…]`, each
  field via `strtol(…, 0)` (`0x10` or `16`), up to 27 payload bytes.
  `help` prints the format with the example `0x10,1,1,0x12,0x34`.
- Prints `Sent message successfully` or `Send failed rc=<n>`; malformed
  input prints `Invalid input - expected CSV`, `Missing type_id` or
  `Missing opcode`.
- There is no read path; see `basic_controller` for decoding replies.

```sh
pio run -e nanoatmega328new -t upload && pio device monitor
```
