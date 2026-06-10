# Examples

CRUMBS examples are organized into a **three-tier learning path** from basics to production patterns to real-world applications.

## Three-Tier Structure

### Tier 1: Core Usage (`examples/core_usage/`)

**Audience:** New users learning CRUMBS fundamentals  
**Purpose:** Understand protocol, encoding, I2C communication

| Platform   | Examples                                                                                                     |
| ---------- | ------------------------------------------------------------------------------------------------------------ |
| Arduino    | [hello_peripheral](../examples/core_usage/arduino/hello_peripheral/) — minimal peripheral                    |
|            | [hello_controller](../examples/core_usage/arduino/hello_controller/) — minimal controller                    |
|            | [basic_peripheral](../examples/core_usage/arduino/basic_peripheral/) — multi-command with state              |
|            | [basic_controller](../examples/core_usage/arduino/basic_controller/) — key command interface                 |
|            | [advanced_controller](../examples/core_usage/arduino/advanced_controller/) — production patterns             |
|            | [mixed_bus_controller](../examples/core_usage/arduino/mixed_bus_controller/) — CRUMBS + raw sensor validation |
|            | [mixed_bus_controller_vendor](../examples/core_usage/arduino/mixed_bus_controller_vendor/) — CRUMBS + vendor libs on one bus |
|            | [basic_peripheral_noncrumbs](../examples/core_usage/arduino/basic_peripheral_noncrumbs/) — non-CRUMBS device |
| PlatformIO | [simple_peripheral](../examples/core_usage/platformio/simple_peripheral/)                                    |
|            | [simple_controller](../examples/core_usage/platformio/simple_controller/)                                    |
| Linux      | [simple_controller](../examples/core_usage/linux/simple_controller/)                                         |
|            | [mixed_bus_probe](../examples/core_usage/linux/mixed_bus_probe/) — generic raw mixed-bus probe tool |
|            | [mixed_bus_lab_validation](../examples/core_usage/linux/mixed_bus_lab_validation/) — bench-specific validation pass (optional BMP/BME) |

**Start here:** Learn message structure, encoding/decoding, basic request-reply patterns.

### Tier 2: Handler Usage (`examples/handlers_usage/`)

**Audience:** Users ready for production callback patterns  
**Purpose:** Learn handler registration and SET_REPLY query mechanism

| Platform   | Examples                                                                  |
| ---------- | ------------------------------------------------------------------------- |
| PlatformIO | [mock_peripheral](../examples/handlers_usage/platformio/mock_peripheral/) |
|            | [mock_controller](../examples/handlers_usage/platformio/mock_controller/) |
| Linux      | [mock_controller](../examples/handlers_usage/linux/mock_controller/)      |

**Key concepts:**

- Handler registration with `crumbs_register_handler()` for SET ops
- Reply handler registration with `crumbs_register_reply_handler()` for GET ops
- SET_REPLY pattern for querying data
- `on_request` callback as fallback for GET operations

See [handlers_usage/README.md](../examples/handlers_usage/README.md) for detailed handler pattern documentation.

### Tier 3: Usage Families (`examples/families_usage/`)

**Audience:** Users building multi-device I²C systems  
**Purpose:** Complete device family implementations with canonical operation headers

| Family    | Devices                                                                 | Controllers                               | Purpose                                                                                                             |
| --------- | ----------------------------------------------------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| **LHWIT** | Calculator (0x03)<br>LED Array (0x01)<br>Servo (0x02)<br>Display (0x04) | Discovery Controller<br>Manual Controller | Low Hardware Implementation Test - demonstrates function-style, state-query, position-control, and display patterns |

**Key concepts:**

- Canonical operation headers shared between peripherals and controllers
- Multi-device coordination on single I²C bus
- Discovery vs manual addressing patterns
- Four interaction patterns: function-style (Calculator), state-query (LED), position-control (Servo), display-control (Display)

See [families_usage/README.md](../examples/families_usage/README.md) for overview and [lhwit-family.md](lhwit-family.md) for comprehensive guide.

---

## Learning Path

1. **Start with Tier 1:** Understand protocol basics
   - Flash [hello_peripheral](../examples/core_usage/arduino/hello_peripheral/) and [hello_controller](../examples/core_usage/arduino/hello_controller/)
   - Study encoding/decoding, observe serial output
   - Expand to [basic_peripheral](../examples/core_usage/arduino/basic_peripheral/) and [basic_controller](../examples/core_usage/arduino/basic_controller/) for multi-command patterns
2. **Move to Tier 2:** Learn production patterns
   - Study [mock_peripheral](../examples/handlers_usage/platformio/mock_peripheral/) handler registration
   - Test [mock_controller](../examples/handlers_usage/platformio/mock_controller/) SET_REPLY queries
   - Adapt pattern for your device type

3. **Apply in Tier 3:** Build multi-device systems
   - Study [LHWIT family](../examples/families_usage/lhwit_family/) canonical headers
   - Test [controller_discovery](../examples/families_usage/controller_discovery/) for flexible addressing
   - Test [controller_manual](../examples/families_usage/controller_manual/) for production systems
   - Use as template for your device families

---

## Platform Coverage

| Platform   | Peripheral Examples           | Controller Examples           |
| ---------- | ----------------------------- | ----------------------------- |
| Arduino    | 3 Core                        | 5 Core                        |
| PlatformIO | 1 Core + 1 Handler + 3 Family | 1 Core + 1 Handler            |
| Linux      | —                             | 3 Core + 1 Handler + 2 Family |

---

## Core Patterns

### Controller Example (Simple Pattern)

Send commands via serial interface and request data from peripherals:

#### Usage

1. Upload to Arduino
2. Open Serial Monitor (115200 baud)
3. Send: `address,type_id,opcode,byte0,byte1,...` (comma-separated bytes)
4. Request: `request=address`

#### Example Commands

```cpp
8,1,1,0,0,128,63,0,0,0,0     // Send to address 8 (first 4 bytes = 1.0f in little-endian)
request=8                    // Request data from address 8
```

See [simple_controller](../examples/core_usage/platformio/simple_controller/) and [simple_peripheral](../examples/core_usage/platformio/simple_peripheral/) for complete examples.

---

### Handler-Based Peripheral (Production Pattern)

Instead of using a switch statement inside `on_message`, register individual handler functions for each command type. This pattern is cleaner when you have many commands.

#### Arduino Peripheral with Handlers

```cpp
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

#define MY_TYPE_ID   0x10
#define CMD_ECHO     0x01
#define CMD_PRINT    0x02
#define CMD_TOGGLE   0x03
#define CMD_GET_ECHO 0x81  // GET: retrieve last echoed data

static crumbs_context_t ctx;
static uint8_t g_echo_buffer[27];
static uint8_t g_echo_len = 0;
static uint8_t g_state    = 0;

void handler_echo(crumbs_context_t *ctx, uint8_t cmd,
                  const uint8_t *data, uint8_t len, void *user) {
    memcpy(g_echo_buffer, data, len);
    g_echo_len = len;
}

void handler_print(crumbs_context_t *ctx, uint8_t cmd,
                   const uint8_t *data, uint8_t len, void *user) {
    for (uint8_t i = 0; i < len; i++) Serial.write(data[i]);
    Serial.println();
}

void handler_toggle(crumbs_context_t *ctx, uint8_t cmd,
                    const uint8_t *data, uint8_t len, void *user) {
    g_state = !g_state;
}

void reply_version(crumbs_context_t *ctx, crumbs_message_t *reply, void *user) {
    (void)ctx; (void)user;
    crumbs_build_version_reply(reply, MY_TYPE_ID, 1, 0, 0);
}

void reply_get_echo(crumbs_context_t *ctx, crumbs_message_t *reply, void *user) {
    (void)ctx; (void)user;
    crumbs_msg_init(reply, MY_TYPE_ID, CMD_GET_ECHO);
    crumbs_msg_add_bytes(reply, g_echo_buffer, g_echo_len);
}

void setup() {
    crumbs_arduino_init_peripheral(&ctx, 0x08);

    // Register SET handlers
    crumbs_register_handler(&ctx, CMD_ECHO,   handler_echo,   NULL);
    crumbs_register_handler(&ctx, CMD_PRINT,  handler_print,  NULL);
    crumbs_register_handler(&ctx, CMD_TOGGLE, handler_toggle, NULL);

    // Register GET reply handlers
    crumbs_register_reply_handler(&ctx, 0x00,         reply_version,  NULL);
    crumbs_register_reply_handler(&ctx, CMD_GET_ECHO, reply_get_echo, NULL);
}

void loop() { }
```

See [handlers_usage](../examples/handlers_usage/) for complete working examples.

---

### SET_REPLY Query Pattern (v0.10.0)

The SET_REPLY mechanism allows controllers to request specific data from peripherals. The library handles `0xFE` automatically and stores the target opcode in `ctx->requested_opcode`.

#### Peripheral with SET_REPLY

```cpp
#include <crumbs.h>
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

#define MY_TYPE_ID       0x42
#define MODULE_VER_MAJ   1
#define MODULE_VER_MIN   0
#define MODULE_VER_PAT   0

static crumbs_context_t ctx;
static uint16_t sensor_value = 0;

void on_request(crumbs_context_t *ctx, crumbs_message_t *reply)
{
    switch (ctx->requested_opcode)
    {
        case 0x00:  // Default: device/version info
            crumbs_msg_init(reply, MY_TYPE_ID, 0x00);
            crumbs_msg_add_u16(reply, CRUMBS_VERSION);
            crumbs_msg_add_u8(reply, MODULE_VER_MAJ);
            crumbs_msg_add_u8(reply, MODULE_VER_MIN);
            crumbs_msg_add_u8(reply, MODULE_VER_PAT);
            break;

        case 0x10:  // Sensor reading
            crumbs_msg_init(reply, MY_TYPE_ID, 0x10);
            crumbs_msg_add_u16(reply, sensor_value);
            break;

        default:  // Unknown opcode
            crumbs_msg_init(reply, MY_TYPE_ID, ctx->requested_opcode);
            break;
    }
}

void setup() {
    crumbs_arduino_init_peripheral(&ctx, 0x10);
    crumbs_set_callbacks(&ctx, NULL, on_request, NULL);
}

void loop() {
    sensor_value = analogRead(A0);  // Update sensor reading
    delay(100);
}
```

#### Controller Querying Sensor Data

```c
#include <crumbs.h>
#include <crumbs_message_helpers.h>

// Step 1: Send SET_REPLY to request sensor data (opcode 0x10)
crumbs_message_t set_reply;
crumbs_msg_init(&set_reply, 0, CRUMBS_CMD_SET_REPLY);
crumbs_msg_add_u8(&set_reply, 0x10);  // target opcode
crumbs_controller_send(&ctx, 0x10, &set_reply, write_fn, io_ctx);

// Step 2: Read the response
uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
int n = read_fn(io_ctx, 0x10, buf, sizeof(buf), 50000);
if (n >= 4)
{
    crumbs_message_t reply;
    if (crumbs_decode_message(buf, n, &reply, NULL) == 0)
    {
        uint16_t sensor;
        crumbs_msg_read_u16(reply.data, reply.data_len, 0, &sensor);
        printf("Sensor value: %u\n", sensor);
    }
}
```

See [handlers_usage](../examples/handlers_usage/) for complete SET_REPLY examples with mock device.

---

## Message Helpers Pattern

The `crumbs_message_helpers.h` header provides type-safe payload building and reading. This pattern is cleaner than manual byte manipulation:

### Controller with Message Helpers

```c
#include <crumbs_message_helpers.h>

// Define command header (see examples/handlers_usage/mock_ops.h for full pattern)
#define SERVO_TYPE_ID    0x02
#define SERVO_CMD_ANGLE  0x01

crumbs_message_t msg;
crumbs_msg_init(&msg, SERVO_TYPE_ID, SERVO_CMD_ANGLE);
crumbs_msg_add_u8(&msg, servo_index);    // Which servo
crumbs_msg_add_u16(&msg, 1500);          // Pulse width in μs

crumbs_controller_send(&ctx, 0x10, &msg, write_fn, write_ctx);
```

### Peripheral Handler with Message Readers

```c
void handle_servo_angle(crumbs_context_t *ctx, uint8_t cmd,
                        const uint8_t *data, uint8_t len, void *user) {
    uint8_t index;
    uint16_t pulse;

    if (crumbs_msg_read_u8(data, len, 0, &index) < 0) return;
    if (crumbs_msg_read_u16(data, len, 1, &pulse) < 0) return;

    servo_set_pulse(index, pulse);
}

void setup() {
    crumbs_arduino_init_peripheral(&ctx, 0x10);
    crumbs_register_handler(&ctx, SERVO_CMD_ANGLE, handle_servo_angle, NULL);
}
```

See [API Reference - Message Helpers](api-reference.md#message-helpers) for complete API documentation.

---

## Common Patterns

### Device Proxy Pattern (Controller-Side)

When sending many commands to the same device, you can bundle the context, address, and I/O function into a struct to reduce repetition:

```c
// Bundle device parameters (user-side pattern, not library API)
typedef struct {
    crumbs_context_t *ctx;
    uint8_t addr;
    crumbs_i2c_write_fn write_fn;
    void *io;
} led_device_t;

// Initialize once
led_device_t led = { &ctx, 0x08, crumbs_arduino_wire_write, NULL };

// Shorter sender wrapper
static inline int led_set_all(led_device_t *dev, uint8_t bitmask) {
    crumbs_message_t msg;
    crumbs_msg_init(&msg, LED_TYPE_ID, LED_CMD_SET_ALL);
    crumbs_msg_add_u8(&msg, bitmask);
    return crumbs_controller_send(dev->ctx, dev->addr, &msg, dev->write_fn, dev->io);
}

// Clean usage
led_set_all(&led, 0x0F);
```

This is an optional convenience pattern. The library's explicit parameter passing remains the primitive for maximum flexibility.

### Multiple Devices

```cpp
uint8_t addresses[] = {0x08, 0x09, 0x0A};
for (int i = 0; i < 3; i++) {
    crumbs_controller_send(&ctx, addresses[i], &msg, crumbs_arduino_wire_write, NULL);
    delay(10);
}
```

### Data Requests

On Arduino, use `Wire.requestFrom()` and the core API helper to decode; e.g. read bytes and call `crumbs_decode_message()`.

On Linux, the HAL helper `crumbs_linux_read_message()` wraps the low-level reads and decoding for you.

### CRC Validation

Use `crumbs_decode_message(buffer, bytesRead, &out, ctx)` which returns 0 on success, -1 for an invalid frame length/content and -2 on CRC mismatch.

### I2C Scanning

```cpp
for (uint8_t addr = 8; addr < 120; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
        Serial.print("Found device at 0x");
        Serial.println(addr, HEX);
    }
}
```

For protocol-aware discovery (find devices that actually speak CRUMBS) use the core helper `crumbs_controller_scan_for_crumbs()` which performs a read-and-decode probe. See the Getting Started guide for short Arduino and Linux examples.

---

## Build Examples

### CMake (Linux)

Linux examples use CMake for building. Example structure:

```bash
cmake --preset linux
cmake --build --preset linux
```

See [simple_controller](../examples/core_usage/linux/simple_controller/) for a complete CMake example.

### PlatformIO

PlatformIO examples support multiple boards (Nano, ESP32):

```bash
cd examples/core_usage/platformio/simple_controller
pio run -e nanoatmega328new
pio run -e esp32dev
```

See [PlatformIO examples](../examples/core_usage/platformio/) for complete projects.
