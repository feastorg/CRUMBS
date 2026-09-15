# Servo peripheral

Type `0x02`, address `0x30`, two servos with speed-limited moves and an
autonomous sweep.

## Hardware

Arduino Nano; servo signal on D9 (index 0) and D10 (index 1); A4/A5 to the
bus. **Power the servos from a separate 5 V supply** with ground common to the
Nano. `SERVO_PIN_0/1`, `NUM_SERVOS` and `UPDATE_INTERVAL_MS` in `src/main.cpp`;
uses the Arduino `Servo` library.

## Protocol (`../servo_ops.h`)

| Opcode | Name | Payload | Effect |
| --- | --- | --- | --- |
| `0x01` | `SERVO_OP_SET_POS` | `[idx:u8][pos:u8]` | target angle, clamped to 180; disables sweep on that servo |
| `0x02` | `SERVO_OP_SET_SPEED` | `[idx:u8][speed:u8]` | degrees per 20 ms tick, clamped to 20; `0` = jump instantly |
| `0x03` | `SERVO_OP_SWEEP` | `[idx:u8][enable:u8][min:u8][max:u8][step:u8]` | bounce between `min` and `max` by `step` per tick; bounds clamped and swapped if reversed, `step` 0→1, >20→20 |
| `0x00` | version | reply 5 bytes | |
| `0x80` | `SERVO_OP_GET_POS` | reply `[pos0:u8][pos1:u8]` | current positions, not targets |
| `0x81` | `SERVO_OP_GET_SPEED` | reply `[speed0:u8][speed1:u8]` | |

Speed `n` moves `n`° every 20 ms, so `1` is the slowest at 50 °/s and `20` is
1000 °/s. Boot state: 90°, speed 0, sweep off. `idx ≥ 2` is rejected.

Wrappers, each taking a pointer to its payload struct:
`servo_send_set_pos(dev, &servo_set_pos_t)`, `servo_send_set_speed(dev,
&servo_set_speed_t)`, `servo_send_sweep(dev, &servo_sweep_t)`,
`servo_get_pos(dev, &servo_pos_result_t)` (`pos0`, `pos1`),
`servo_get_speed(dev, &servo_speed_result_t)` (`speed0`, `speed1`).

## Serial

115200 baud. Boot prints `=== CRUMBS Servo Controller Peripheral ===`, the
address, type, `Servos: 2 (D9, D10)`, a power warning and `Servo controller
peripheral ready!`. Each SET prints one line, e.g.
`SET_POS: Servo 0 target=45 (current=90, speed=0)`, or `SET_POS: Invalid
servo index 2`.

## Build

```sh
pio run -e nanoatmega328new -t upload
```
