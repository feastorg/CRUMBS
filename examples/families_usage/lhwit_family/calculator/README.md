# Calculator peripheral

Type `0x03`, address `0x10`, no hardware. A SET performs an operation and
stores the result; GETs read the result and a 12-entry history.

## Protocol (`../calculator_ops.h`)

| Opcode | Name | Payload | Effect |
| --- | --- | --- | --- |
| `0x01`–`0x04` | `CALC_OP_ADD` `SUB` `MUL` `DIV` | `[a:u32][b:u32]` | unsigned 32-bit, wrapping; `DIV` by zero stores `0xFFFFFFFF` |
| `0x00` | version | reply 5 bytes | |
| `0x80` | `CALC_OP_GET_RESULT` | reply `[result:u32]` | last result |
| `0x81` | `CALC_OP_GET_HIST_META` | reply `[count:u8][write_pos:u8]` | entries stored (≤ 12) and the next slot |
| `0x82`–`0x8D` | `CALC_OP_GET_HIST_0`…`11` | reply 16 bytes `[op:4 chars][a:u32][b:u32][result:u32]` | slot `n`; empty reply if the slot is unused |

History is a ring: slot index is physical, not chronological. `op` is
`"ADD"`, `"SUB"`, `"MUL"` or `"DIV"` NUL-padded. The twelve history opcodes
are answered by an `on_request` fallback rather than twelve reply handlers,
which is what keeps the project inside `-DCRUMBS_MAX_HANDLERS=8`.

Wrappers: `calc_send_add/sub/mul/div(dev, &calc_operands_t)`,
`calc_get_result(dev, &calc_result_t)`, `calc_get_hist_meta(dev,
&calc_hist_meta_t)`, `calc_get_hist_entry(dev, idx, &calc_hist_entry_t)`.

## Serial

115200 baud. Boot prints `=== CRUMBS Calculator Peripheral ===`, `I2C
Address: 0x10` and `Ready`; the only other output is `DIV: Error (div by
zero)`.

## Build

```sh
pio run -e nanoatmega328new -t upload
```
