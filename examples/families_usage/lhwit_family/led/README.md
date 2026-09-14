# LED array peripheral

Type `0x01`, address `0x20`, four LEDs. The simplest LHWIT device: SET
commands change output state, GET queries report it.

## Hardware

Arduino Nano; LEDs on D4, D5, D6, D7 (index 0–3) through 220 Ω to ground;
A4/A5 to the bus. `LED_PINS[]` and `NUM_LEDS` in `src/main.cpp`.

## Protocol (`../led_ops.h`)

| Opcode | Name | Payload | Effect |
| --- | --- | --- | --- |
| `0x01` | `LED_OP_SET_ALL` | `[mask:u8]` | bits 0–3 drive the four LEDs; blink is not disturbed |
| `0x02` | `LED_OP_SET_ONE` | `[idx:u8][state:u8]` | one LED; `idx ≥ 4` ignored; non-zero = on |
| `0x03` | `LED_OP_BLINK` | `[idx:u8][enable:u8][period_ms:u16]` | toggle `idx` every `period_ms / 2`; needs all 4 bytes |
| `0x00` | version | reply `[CRUMBS_VERSION:u16][1][0][0]` | |
| `0x80` | `LED_OP_GET_STATE` | reply `[states:u8]` | the mask from `SET_ALL`/`SET_ONE`, not the pin level while blinking |
| `0x81` | `LED_OP_GET_BLINK` | reply 12 bytes: `[enable:u8][period_ms:u16]` × 4 | |

Blink is serviced from `loop()` every 10 ms, so periods below that resolve
to it; nothing enforces a minimum. Defaults: all off, blink disabled,
period 1000 ms.

Controller wrappers: `led_send_set_all(dev, mask)`, `led_send_set_one(dev,
idx, state)`, `led_send_blink(dev, idx, enable, period_ms)`,
`led_get_state(dev, &led_state_result_t)`, `led_get_blink(dev,
&led_blink_result_t)`.

## Serial

115200 baud. Prints `=== CRUMBS LED Array Peripheral ===`, `I2C Address: 0x20`
and `Ready` at boot; the handlers print nothing.

## Build

```sh
pio run -e nanoatmega328new -t upload
```

`-DCRUMBS_MAX_HANDLERS=8` and `-I ..` are set in `platformio.ini`.
