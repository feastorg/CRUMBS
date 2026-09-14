# Basic Peripheral

**Level 2** - Prerequisite: Complete [hello_peripheral](../hello_peripheral/) first

## What's New

- Multiple command types (store, clear)
- Query responses (GET operations)
- Version info pattern

Listens at I2C address `0x10` (`DEVICE_ADDR` in `config.h`); `basic_controller` targets the same address.

## Commands

- **0x01:** Store data (up to 10 bytes)
- **0x02:** Clear stored data

## Queries (via SET_REPLY)

- **0x00:** Version info (5 bytes)
- **0x80:** Get stored data

## Key Learning

- Switch statement for command routing
- Storing peripheral state
- Building query responses

**Compare:** See [basic_peripheral_noncrumbs](../basic_peripheral_noncrumbs/) to see what CRUMBS provides

**Next:** [advanced_controller](../advanced_controller/) for serial parsing and scanning
