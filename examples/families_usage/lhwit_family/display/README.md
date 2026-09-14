# Display peripheral

Type `0x04`, address `0x40`, a 5641AS four-digit common-cathode 7-segment
display multiplexed from `loop()`.

## Hardware

Arduino Nano and the `Simple5641AS` library (`adrian200223/Simple5641AS`,
in `platformio.ini`). Pins as `src/main.cpp` defines them:

| Segment | a | b | c | d | e | f | g | dp |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Pin | D9 | D13 | D4 | D6 | D7 | D10 | D3 | D5 |

| Digit | 1 (left) | 2 | 3 | 4 |
| --- | --- | --- | --- | --- |
| Pin | D8 | D11 | D12 | D2 |

The display is refreshed every 2 ms with 500 µs per digit.

## Protocol (`../display_ops.h`)

| Opcode | Name | Payload | Effect |
| --- | --- | --- | --- |
| `0x01` | `DISPLAY_OP_SET_NUMBER` | `[number:u16][decimal_pos:u8]` | show `number`; `decimal_pos` 1–4 lights that digit's point, 0 none |
| `0x02` | `DISPLAY_OP_SET_SEGMENTS` | `[seg0][seg1][seg2][seg3]` | raw segments per digit, bit 7 = `a` … bit 0 = `dp` |
| `0x03` | `DISPLAY_OP_SET_BRIGHTNESS` | `[level:u8]` | stored and reported; the library has no brightness control, so no visible effect |
| `0x04` | `DISPLAY_OP_CLEAR` | — | blank the display; number and decimal position are kept |
| `0x00` | version | reply 5 bytes | |
| `0x80` | `DISPLAY_OP_GET_VALUE` | reply `[number:u16][decimal_pos:u8][brightness:u8]` | |

Numbers render with leading zeros (`42` shows `0042`); keep them at 9999 or
below — the library has no handling for more digits. Digit `0` in segment
form is `0xFC`.

Wrappers: `display_send_set_number(dev, number, decimal_pos)`,
`display_send_set_segments(dev, segments[4])`,
`display_send_set_brightness(dev, level)`, `display_send_clear(dev)`,
`display_get_value(dev, &display_value_result_t)`.

## Serial

115200 baud (boot waits up to 2 s for the port). Prints a banner with type,
address and versions, then `Display peripheral ready`; each command echoes,
e.g. `Display: 1234 (decimal on digit 2)`, `Brightness: 7`, `Display cleared`.

## Build

```sh
pio run -e nanoatmega328new -t upload
```

`-DCRUMBS_MAX_HANDLERS=6` in `platformio.ini`.
