# Handler Usage Examples (Tier 2)

**Audience:** Users comfortable with basics, ready for production patterns  
**Purpose:** Learn callback registration pattern and SET_REPLY query mechanism  
**Prerequisites:** Complete [core_usage hello examples](../core_usage/arduino/hello_peripheral/) and [basic examples](../core_usage/arduino/basic_peripheral/) first

These examples demonstrate two key patterns:

1. **Handler registration** using `crumbs_register_handler()` for SET operations
2. **SET_REPLY queries** using `on_request` callback for GET operations

A mock device is used to focus on the patterns without hardware complexity.

---

## Mock Device Design

**Type ID:** `0x10` (arbitrary mock device)  
**Defined in:** [`mock_ops.h`](mock_ops.h) (shared contract header)

### SET Operations (Handler-based)

| Opcode | Name                  | Description                                |
| ------ | --------------------- | ------------------------------------------ |
| `0x01` | MOCK_OP_ECHO          | Echo: Store data for later retrieval       |
| `0x02` | MOCK_OP_SET_HEARTBEAT | Set LED heartbeat period (in milliseconds) |
| `0x03` | MOCK_OP_TOGGLE        | Toggle: Enable/disable LED heartbeat       |

### GET Operations (via SET_REPLY + on_request)

| Opcode | Name               | Description                                     |
| ------ | ------------------ | ----------------------------------------------- |
| `0x00` | (default)          | Device info (version)                           |
| `0x80` | MOCK_OP_GET_ECHO   | Return stored echo data                         |
| `0x81` | MOCK_OP_GET_STATUS | Return heartbeat state (on/off) and period (ms) |
| `0x82` | MOCK_OP_GET_INFO   | Return device info (default)                    |

---

## Examples

### PlatformIO

| Example                                         | Description                                 |
| ----------------------------------------------- | ------------------------------------------- |
| [mock_peripheral/](platformio/mock_peripheral/) | Registers 3 handlers + on_request callback  |
| [mock_controller/](platformio/mock_controller/) | Interactive CLI - sends commands via serial |

### Linux

| Example                                    | Description                                 |
| ------------------------------------------ | ------------------------------------------- |
| [mock_controller/](linux/mock_controller/) | Interactive CLI for testing mock peripheral |

---

## Key Concepts

### 1. Handler Registration (SET Operations)

Handlers are registered for SET operations (0x01–0x7F). These are called automatically when messages arrive:

```c
// Register handlers for SET commands
crumbs_register_handler(&ctx, MOCK_OP_ECHO, handler_echo, NULL);
crumbs_register_handler(&ctx, MOCK_OP_SET_HEARTBEAT, handler_set_heartbeat, NULL);
crumbs_register_handler(&ctx, MOCK_OP_TOGGLE, handler_toggle, NULL);

// Also set the on_request callback for GET queries
crumbs_set_callbacks(&ctx, NULL, on_request, NULL);
```

Each handler is called automatically when its opcode is received:

```c
void handler_echo(crumbs_context_t *ctx, uint8_t opcode,
                  const uint8_t *data, uint8_t len, void *user) {
    // Store received data for later retrieval via SET_REPLY
    memcpy(g_echo_buffer, data, len);
    g_echo_len = len;
    Serial.println("Echo handler: data stored");
}
```

### 2. SET_REPLY Pattern (GET Operations)

The SET_REPLY mechanism lets controllers query specific data:

**Controller sends (using helper functions from mock_ops.h):**

```c
// Set heartbeat period using helper function
int rc = mock_send_heartbeat(&ctx, PERIPHERAL_ADDR,
                              crumbs_arduino_wire_write, NULL, 1000);

// Query status using helper function
int rc = mock_query_status(&ctx, PERIPHERAL_ADDR,
                           crumbs_arduino_wire_write, NULL);

// Now read and decode the reply
uint8_t raw[32];
int bytes = crumbs_arduino_read(NULL, PERIPHERAL_ADDR, raw, sizeof(raw), 10000);
crumbs_message_t reply;
crumbs_decode_message(raw, bytes, &reply, &ctx);
// reply.data contains: [state:u8, period_ms:u16]
```

**Library automatically:**

1. Intercepts SET_REPLY (0xFE)
2. Stores `data[0]` in `ctx->requested_opcode`
3. Does NOT dispatch to handlers

**Peripheral's on_request callback:**

```c
void on_request(crumbs_context_t *ctx, crumbs_message_t *reply) {
    switch (ctx->requested_opcode) {
        case 0x00:  // Default: device info
        case MOCK_OP_GET_INFO:
            crumbs_msg_init(reply, MOCK_TYPE_ID, MOCK_OP_GET_INFO);
            // Add device info string
            break;

        case MOCK_OP_GET_STATUS:
            crumbs_msg_init(reply, MOCK_TYPE_ID, MOCK_OP_GET_STATUS);
            crumbs_msg_add_u8(reply, g_state);         // Heartbeat enabled?
            crumbs_msg_add_u16(reply, g_heartbeat_ms); // Period in ms
            break;

        case MOCK_OP_GET_ECHO:
            crumbs_msg_init(reply, MOCK_TYPE_ID, MOCK_OP_GET_ECHO);
            for (int i = 0; i < g_echo_len; i++) {
                crumbs_msg_add_u8(reply, g_echo_buffer[i]);
            }
            break;

        default:
            crumbs_msg_init(reply, MOCK_TYPE_ID, ctx->requested_opcode);
            break;
    }
}
```

---

## Interactive Controllers

Both PlatformIO and Linux controllers are **interactive CLI tools**. Instead of running demo loops, they accept serial commands:

**PlatformIO Example:**

```text
> help
> echo DE AD BE EF
> heartbeat 1000
> toggle
> status
> getecho
```

**Linux Example:**

```bash
$ ./crumbs_mock_linux_controller /dev/i2c-1
> scan
> heartbeat 500
> toggle
> status
> info
```

---

## Contract Header Pattern

The `mock_ops.h` header demonstrates the correct pattern for device contracts:

**What's in the header:**

- TYPE_ID constant (identifies device family)
- Opcode definitions (command vocabulary)
- Static inline helper functions (controller convenience)

**What's NOT in the header:**

- I2C addresses (defined locally in each example)
- Implementation details

**Example from mock_ops.h:**

```c
#define MOCK_TYPE_ID 0x10
#define MOCK_OP_ECHO 0x01

// Helper takes addr as parameter (NOT hardcoded!)
static inline int mock_send_echo(crumbs_context_t *ctx,
                                  uint8_t addr,  // <-- Instance-specific
                                  crumbs_i2c_write_fn write_fn,
                                  void *io,
                                  const uint8_t *data,
                                  uint8_t len) {
    crumbs_message_t msg;
    crumbs_msg_init(&msg, MOCK_TYPE_ID, MOCK_OP_ECHO);
    // ... build message ...
    return crumbs_controller_send(ctx, addr, &msg, write_fn, io);
}
```

---

## Learning Path

1. **Understand handlers:** Read `mock_peripheral` code - see how SET operations work
2. **Understand SET_REPLY:** See how `on_request` handles different `requested_opcode` values
3. **Flash and test:** Upload peripheral + controller, try interactive commands
4. **Explore the contract:** Read `mock_ops.h` to see the helper function pattern
5. **Next step:** Move to [families_usage/](../families_usage/) (Tier 3) for real hardware examples
