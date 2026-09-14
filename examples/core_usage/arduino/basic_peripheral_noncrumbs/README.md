# Basic Peripheral (Non-CRUMBS)

**Comparison Example** - See what CRUMBS replaces

## Purpose

This is a **raw I²C peripheral** without CRUMBS, implementing the same functionality as [basic_peripheral](../basic_peripheral/) using direct Wire library calls.

**Why we include this example:**

1. **Comparison** - See what CRUMBS provides:
   - Automatic message framing
   - CRC validation
   - Payload handling
   - Error checking

2. **Discovery testing** - Demonstrates that CRUMBS bus scanning (e.g., in [advanced_controller](../advanced_controller/)) can correctly distinguish between CRUMBS devices and regular I²C peripherals

## What it does

- Listens at I2C address 0x45
- Receives raw bytes via Wire.onReceive()
- No message structure or validation
- No CRC checking

## Key Learning

See the difference in complexity:

- **This example:** Manual byte handling, no structure
- **basic_peripheral:** Structured messages with automatic validation

## When to use raw I²C

- Talking to non-CRUMBS devices
- Understanding protocol overhead
- Learning I²C fundamentals

## Recommendation

Start with [hello_peripheral](../hello_peripheral/) to learn CRUMBS first, then review this for comparison.
