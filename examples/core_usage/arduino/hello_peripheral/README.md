# Hello Peripheral

**Start here!** This is the absolute simplest CRUMBS peripheral.

## What it does

- Listens at I2C address 0x10
- Prints when it receives messages
- Counts received messages
- Returns counter when queried

## Upload & Test

1. Upload to Arduino Nano
2. Open Serial Monitor (115200 baud)
3. You should see: "Hello peripheral at 0x10"
4. Use hello_controller to send messages

## Key Concepts

- `crumbs_arduino_init_peripheral()` - Setup peripheral
- `on_message()` - Called when message received
- `on_request()` - Called when controller queries us

**Next:** See [basic_peripheral](../basic_peripheral/) for handling different commands
