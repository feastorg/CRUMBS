# CRUMBS Roadmap

Work that is planned is tracked in [GitHub issues](https://github.com/feastorg/CRUMBS/issues). This file holds only what is *not* being worked on: directions that may be worth taking someday, and decisions not to take a direction, with the reasoning so they are not re-litigated by accident.

**Note:** No commitment is made regarding timing or whether any specific item will be implemented.

---

## ESP32 Platform Support

**Goal:** Enable ESP32 for IoT/gateway applications (likely works immediately via Arduino Wire compatibility)

**Value:** High — ESP32 is extremely popular with built-in WiFi/BLE for gateway use cases  
**Feasibility:** Very high — Arduino Wire API compatible, minimal validation needed  
**Effort:** Low — Primarily documentation and testing

**Done:** CI builds every PlatformIO example (core, handlers, and all four lhwit peripherals) for `esp32dev`; the default I²C pins are in [platform-setup.md](platform-setup.md#wiring) and every project ships an `esp32dev` env.

**Remaining:**

- Hardware validation: ESP32-DevKitC, ESP32-S3 at 100kHz–1MHz
- Documentation: dual-core thread safety, gateway patterns
- Example: minimal WiFi bridge

---

## Performance & Benchmarking

**Goal:** Quantitative performance data for validation and regression testing

**Value:** High — Validates design claims, enables regression detection, provides concrete metrics for users  
**Feasibility:** High — Measures existing functionality, no new features required  
**Effort:** Medium — Benchmark suite development and cross-platform testing

**Deliverables:**

- Benchmark suite: encode/decode, handler dispatch, CRC-8, memory footprint
- `docs/performance.md` with platform results (AVR, ESP32, Linux)
- Target: 1000 msg/sec @ 100kHz I²C on Arduino Nano; <3KB flash, <200B RAM
- CI integration: automated benchmarks, regression tracking

---

## Error Recovery Patterns

**Goal:** Document robust error handling for production systems

**Value:** High — Critical for production deployments, helps users build reliable systems  
**Feasibility:** High — Documentation-focused, no core library changes  
**Effort:** Medium — Comprehensive guide and reference implementation

**Deliverables:**

- `docs/error-recovery.md` – Retry strategies, exponential backoff, circuit breaker patterns, timeout configuration
- `examples/patterns/robust_controller/` – Reference implementation
- Optional helper functions for common patterns

---

## Python Controller Tools

**Goal:** Rapid prototyping and automation with Python (controller-only)

**Value:** Medium-High — Lowers barrier to entry, enables rapid prototyping and integration  
**Feasibility:** High — Controller-only scope, well-defined protocol  
**Effort:** Medium-High — Package development, CLI tools, testing infrastructure

**Deliverables:**

- PyPI package: `pip install crumbs-i2c` (encode/decode, CRC-8, I²C via smbus2)
- CLI tools: `crumbs-scan`, `crumbs-send`, `crumbs-query`
- `docs/python-quickstart.md` with integration examples (pandas, Flask)
- pytest suite with LHWIT validation

**Example:**

```python
from crumbs_controller import CrumbsController
bus = CrumbsController("/dev/i2c-1")
devices = bus.scan()  # {0x10: 0x03, 0x20: 0x01}
bus.send(0x20, type_id=0x01, opcode=0x01, data=[0xFF])
```

---

## Linux Peripheral Support

**Status:** I²C target mode (what the kernel calls slave mode) requires kernel driver development ([technical details](https://github.com/feastorg/linux-wire/blob/main/docs/slave-mode-investigation.md)). Alternative transports needed for Linux peripheral support.

**Value:** Medium — Enables Raspberry Pi as peripheral, but microcontrollers better suited for this role  
**Feasibility:** Low (I²C) / Medium (Serial) — Kernel driver required OR new transport layer  
**Effort:** High — Either kernel module (beyond scope) or transport abstraction (substantial architecture change)

### Alternative Approaches

#### Option A: Serial/UART (Most Feasible)

- User-space support via `/dev/ttyS*`, `/dev/ttyUSB*`, USB CDC/ACM
- No kernel development required
- **Implementation:** Transport abstraction layer (`crumbs_serial.c`) with same message format
- **Use case:** Raspberry Pi as USB/serial CRUMBS peripheral

#### Option B: Network/TCP

- WiFi/Ethernet connectivity for wireless CRUMBS networks
- Standard sockets API, full user-space support
- **Trade-off:** Network overhead vs wireless capability
- **Use case:** Raspberry Pi Zero W as wireless bridge

#### Option C: Custom Kernel Module

- Full I²C target functionality with event-driven architecture
- **Verdict:** Beyond CRUMBS core library scope (requires kernel expertise, distribution complexity)

### Implementation Path (if pursuing Serial)

**Files:**

- `src/transports/crumbs_serial.c` - Transport layer abstraction
- `src/hal/linux/crumbs_linux_serial.c` - Linux serial HAL
- `examples/core_usage/linux/serial_peripheral/`

**Documentation:**

- `docs/transport-layers.md` - I²C vs Serial architecture
- Update `docs/platform-setup.md` with serial setup

**Success Criteria:**

- Same CRUMBS handlers work on both I²C and serial transports
- Raspberry Pi peripheral via USB CDC/ACM

---

## Reference Family Expansion

**Goal:** Additional reference implementations beyond LHWIT (LED/Servo/Calculator/Display)

**Value:** Low-Medium — More examples helpful but LHWIT already demonstrates patterns  
**Feasibility:** Medium — Requires hardware, full implementation stack  
**Effort:** High — Each family needs peripheral + controller + docs + hardware validation

**Context:** A "family" is a device category with shared type_id, opcodes, and handler patterns (e.g., LHWIT defines how LED modules communicate).

**Potential Families:**

- **Sensors:** Temperature (DHT22, BMP280), humidity, pressure – periodic polling patterns
- **Motor Control:** DC/stepper motors – motion control, homing sequences
- **Protocol Bridges:** UART/SPI over I²C – translation and buffering patterns

**Note:** Community-driven based on actual user requests. Each family requires significant effort (peripheral + controller + docs + hardware).

---

## Possible Ideas

Unranked. Each is a thought that has come up more than once, kept so it is not lost; none has a demonstrated need yet.

- **Device classes.** Pre-defined `type_id` / opcode sets for common device kinds (in the spirit of USB device classes), so third parties can build compatible hardware without coordinating. Not a library requirement; would live alongside the reference families.
- **Handler lookup strategy.** Handler dispatch is a linear scan bounded by `CRUMBS_MAX_HANDLERS`. A build-time option for a sorted table with binary search would only pay off at handler counts the current bound does not reach. Wait for a measured need.

---

## Explicitly Out of Scope

### Gateway Services

Building REST/MQTT/WebSocket services is **application territory**, not library scope. Different users need different protocols. Better approach: document integration patterns, provide minimal examples, let community build gateways.

**Alternative:** `docs/integration-patterns.md` showing how to use CRUMBS in Flask routes, MQTT publishers, etc.

### Multi-Family Bus Sharing

"One family per bus" stays. A controller is compiled for one family and understands only that family's vocabulary; that gives compile-time checked command usage with no runtime negotiation or dynamic dispatch. Multi-family systems use multiple I²C buses or a gateway.

**Why not lift it:** the only real blocker is that `type_id` is a per-family namespace (the lhwit LED and the BREAD RLHT module are both `0x01`), but resolving that moves dispatch from compile time to runtime, costs the controller every family's headers and parsers in flash, and makes the identity read a required discovery round trip. That trades away the design's main safety claim for flexibility nobody has needed yet.

**Extension path, if it is ever needed:** append a family identifier to the opcode `0x00` identity reply. It rides in a reply that already exists, and existing parsers read fixed low offsets past a minimum length, so trailing bytes are backward compatible. Spending a payload byte on family in every frame was considered and rejected. Full analysis: [#38](https://github.com/feastorg/CRUMBS/issues/38).

**Trigger to revisit:** a real deployment reaching for a second bus or a gateway purely to satisfy this rule. A `type_id` check on receive ([#37](https://github.com/feastorg/CRUMBS/issues/37)) is a prerequisite either way.

### Enhanced Discovery Protocols

Current discovery (version query via opcode 0x00) is sufficient. Capability negotiation, hierarchical addressing, and multicast add protocol complexity without demonstrated user need.

The same applies to further reserved opcodes that have been floated — a CAPABILITIES bitmap (multi-frame, low-power, streaming), CONFIG_GET/CONFIG_SET, and STATS counters. Only SET_REPLY (`0xFE`) is reserved today.

**Alternative:** Wait for actual pain points; if needed, module families can extend version query payload.
