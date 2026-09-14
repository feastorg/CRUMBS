# simple_peripheral (PlatformIO)

A peripheral that dumps every frame it receives and answers every read with
one fixed frame. Pair with `simple_controller`.

- Address `0x10`. Reply: `type 0x10, opcode 0x42, data DE AD BE EF`,
  whatever opcode was requested.
- Serial 115200: `CRUMBS PlatformIO peripheral (Nano) - init at address 0x10`,
  then per frame `Received message: type_id=<t> cmd=<op> data_len=<n>` and
  one ` data[i]=0x..` line per byte.
- Envs `nanoatmega328new` (default), `nanoatmega328old`, `esp32dev`; the
  project pins the published library.

```sh
pio run -e nanoatmega328new -t upload && pio device monitor
```
