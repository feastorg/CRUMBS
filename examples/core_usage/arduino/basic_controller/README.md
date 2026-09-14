# Basic Controller

**Level 2** - Prerequisite: Complete [hello_controller](../hello_controller/) first

## What's New

- Multiple command types (store/clear/query)
- Two-step query pattern (SET_REPLY + read)
- Decoding replies

Talks to the peripheral at `0x10` (`TARGET_ADDR` in `config.h`), the `basic_peripheral` default.

## Commands

- **s** - Store data (sends 3 bytes)
- **c** - Clear stored data
- **v** - Query version info
- **d** - Query stored data

## Upload & Test

1. Wire Arduino to basic_peripheral (SDA/SCL + GND)
2. Upload this sketch
3. Open Serial Monitor (115200 baud)
4. Try commands: s, c, v, d

## Key Learning

- Multiple command types
- SET_REPLY mechanism for queries
- Decoding query responses

**Next:** [advanced_controller](../advanced_controller/) for CSV parsing and scanning
