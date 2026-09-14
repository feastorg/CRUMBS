# Advanced Controller

**Level 3: Production Patterns**

This example demonstrates a production-ready controller with CSV parsing, I2C scanning, and comprehensive error handling.

**For a simpler version, see [basic_controller](../basic_controller/)**

## Features

- CSV command parsing from serial input
- I2C bus scanning
- Multi-byte message support
- Error reporting and diagnostics

## Usage

Open Serial Monitor (115200 baud) and send commands:

### Send Message

Format: `address,type_id,opcode,byte0,byte1,...`

Example:

```
0x10,1,1,0xAA,0xBB,0xCC
```

### Scan I2C Bus

```
SCAN
```

**Note:** CRUMBS scanning can distinguish between CRUMBS-enabled peripherals and regular I²C devices. Try with [basic_peripheral_noncrumbs](../basic_peripheral_noncrumbs/) (non-CRUMBS) on the same bus to see the difference.

## Key Learning

- Serial input parsing
- I2C device discovery
- Production error handling
- Complex message construction

**Previous:** [basic_controller](../basic_controller/) for simple key commands
