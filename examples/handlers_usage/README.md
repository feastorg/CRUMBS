# Handler usage

The handler tables and the ops-header pattern, on a "mock" device whose only
hardware is the built-in LED: `mock_ops.h` is the contract, `mock_peripheral`
implements it, and two controllers (PlatformIO and Linux) drive it through a
`crumbs_device_t`.

## The mock device (`mock_ops.h`)

Type `0x10`, address `0x10`.

| Opcode | Name | Payload / reply |
| --- | --- | --- |
| `0x01` | `MOCK_OP_ECHO` | SET: up to 27 raw bytes, stored |
| `0x02` | `MOCK_OP_SET_HEARTBEAT` | SET: `[period_ms:u16]` |
| `0x03` | `MOCK_OP_TOGGLE` | SET: heartbeat on/off |
| `0x80` | `MOCK_OP_GET_ECHO` | reply: the stored bytes |
| `0x81` | `MOCK_OP_GET_STATUS` | reply: `[state:u8][period_ms:u16]` |
| `0x82` | `MOCK_OP_GET_INFO` | reply: ASCII `MockDev v1.0` (also the answer to opcode `0x00`) |

Wrappers: `mock_send_echo(dev, data, len)`, `mock_send_heartbeat(dev,
period_ms)`, `mock_send_toggle(dev)`, `mock_get_echo(dev, &mock_echo_result_t)`,
`mock_get_status(dev, &mock_status_result_t)`, `mock_get_info(dev,
&mock_info_result_t)`. When the heartbeat is on, the peripheral pulses
`LED_BUILTIN` for 50 ms every `period_ms` (default 500, off at boot).

The peripheral registers the three SET opcodes with
`crumbs_register_handler()` and answers the GETs from an `on_request`
callback that switches on `requested_opcode`; the LHWIT family shows the
alternative, `crumbs_register_reply_handler()`.

## Controllers

Both accept the same commands: `echo <hex bytes>` (`DE AD BE EF`), `heartbeat
<ms>`, `toggle`, `status` → `Status: Heartbeat: ENABLED|DISABLED, Period: <n>
ms`, `getecho` → `Echo data: …`, `info` → `Device info: MockDev v1.0`, `scan`,
`help`. The PlatformIO one reads them from the serial monitor (prompt `> `,
target fixed at `0x10`; its `scan` is a plain address ping). The Linux one,
`build-linux/crumbs_mock_controller [i2c-dev]`, runs the CRUMBS scan over
`0x03`–`0x77` and adds `quit`.

## Build

```sh
pio run -d examples/handlers_usage/platformio/mock_peripheral -e nanoatmega328new -t upload
pio run -d examples/handlers_usage/platformio/mock_controller -e nanoatmega328new -t upload
```

The projects add `-I ../..` so `mock_ops.h` resolves and pin the published
library; the peripheral builds with `-DCRUMBS_MAX_HANDLERS=8`. The Linux
controller is built by the root CMake with the Linux HAL.
