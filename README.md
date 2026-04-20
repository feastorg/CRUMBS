# CRUMBS

[![CI](https://github.com/feastorg/CRUMBS/actions/workflows/ci.yml/badge.svg)](https://github.com/feastorg/CRUMBS/actions/workflows/ci.yml)
[![Pages](https://github.com/feastorg/feastorg.github.io/actions/workflows/pages.yml/badge.svg)](https://feastorg.github.io/crumbs/)
[![Doc-check](https://github.com/feastorg/CRUMBS/actions/workflows/doccheck.yml/badge.svg)](https://github.com/feastorg/CRUMBS/actions/workflows/doccheck.yml)

CRUMBS (Communications Router and Unified Message Broker System) is a small, portable C-based protocol for controller/peripheral I²C messaging. The project ships a C core (encoding/decoding, CRC) and thin platform HALs for Arduino and Linux so the same protocol works on microcontrollers and native hosts.

> _So you've mastered bits and bytes, maybe toiled with nibbles and words-get ready to indulge in some serious crumb crunching!_

![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)
![Arduino](https://img.shields.io/badge/Arduino-00979D?logo=arduino&logoColor=white)
[![Language](https://img.shields.io/badge/language-C%20%7C%20C%2B%2B-00599C)](./)
[![CMake](https://img.shields.io/badge/build-CMake-064F8C)](https://cmake.org/)

[![License: AGPL-3.0-or-later](https://img.shields.io/badge/License-AGPL--3.0--or--later-blue.svg)](./LICENSE)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/cameronbrooks11/library/CRUMBS.svg)](https://registry.platformio.org/libraries/cameronbrooks11/CRUMBS)

## Features

- **Variable-Length Message Format**: 4–31 byte frames with opaque byte payloads (0–27 bytes)
- **Controller/Peripheral Architecture**: One controller, multiple addressable devices
- **Per-Command Handler Dispatch**: Register SET handlers with `crumbs_register_handler()` per opcode
- **Reply Handler Dispatch**: Register GET handlers with `crumbs_register_reply_handler()` per opcode
- **Family Receiver API**: `_get_*` wrappers (query + delay + read + parse) in ops headers
- **Bound Device Handle**: `crumbs_device_t` bundles context, address, and I/O per device
- **Message Builder/Reader Helpers**: Type-safe payload construction via `crumbs_message_helpers.h`
- **Event-Driven Communication**: Callback-based message handling (`on_message`, `on_request`)
- **CRC-8 Protection**: Integrity check on every message
- **CRUMBS-aware Discovery**: Core scanner helper to find devices that speak CRUMBS

## Quick Start

```c
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

crumbs_context_t ctx;
crumbs_arduino_init_controller(&ctx);

crumbs_message_t m;
crumbs_msg_init(&m, 0x01, 0x01);     // type_id=1, opcode=1
crumbs_msg_add_float(&m, 25.5f);     // Payload: float
crumbs_msg_add_u8(&m, 0x01);         // Payload: byte

crumbs_controller_send(&ctx, 0x08, &m, crumbs_arduino_wire_write, NULL);
```

Upload [hello examples](examples/core_usage/arduino/hello_peripheral/) to two Arduinos, wire I²C + GND, Serial Monitor (115200): `s`/`r`. See [examples/](examples/).

## Installation

**PlatformIO (Recommended):**

```ini
[env]
lib_deps = cameronbrooks11/CRUMBS@^0.12.0
```

**Arduino IDE:** Tools → Manage Libraries → "CRUMBS" → Install, or copy to `~/Arduino/libraries/`

**Linux/Native:** CMake build (see [CONTRIBUTING.md](CONTRIBUTING.md))

Arduino and PlatformIO builds use the CRUMBS core with no extra dependencies. Linux HAL builds require `linux-wire` for I2C bus access.

## Hardware Requirements

- Arduino or compatible microcontroller
- I²C bus with 4.7kΩ pull-up resistors on SDA/SCL
- Unique addresses (0x08–0x77) for each peripheral
- **Level shifter if mixing 5V/3.3V devices**

**Wiring:** SDA↔SDA, SCL↔SCL, GND↔GND (+ 4.7kΩ pull-ups to VCC)

## Documentation

Documentation is available in the [docs](docs/) directory:

**Getting Started:**

- [Platform Setup](docs/platform-setup.md) — Installation for Arduino, PlatformIO, and Linux
- [Examples](docs/examples.md) — Three-tier learning path

**Reference:**

- [API Reference](docs/api-reference.md) — Complete C API, handlers, message helpers, platform HALs
- [Protocol Specification](docs/protocol.md) — Wire format, versioning, CRC-8
- [Architecture](docs/architecture.md) — Design philosophy and system architecture
- [Create a Family](docs/create-a-family.md) — Guide to authoring custom device families

**Developer:**

- [CONTRIBUTING.md](CONTRIBUTING.md) — How to build, test, and contribute
- [Documentation Index](docs/index.md) — Complete documentation catalog

## Examples

Examples are organized into three progressive tiers:

- **Tier 1** (`examples/core_usage/`) - Basic protocol learning for beginners
- **Tier 2** (`examples/handlers_usage/`) - Handler patterns with mock device
- **Tier 3** (`examples/families_usage/`) - Production patterns (Roadmap-03)

See [examples/README.md](examples/README.md) for complete platform coverage and [docs/examples.md](docs/examples.md) for detailed documentation.

## Troubleshooting

**No communication:** Check 4.7kΩ pull-ups, common ground, addresses. Level shifter for 5V↔3.3V.

**CRC errors:** Shorten wires (<30cm), `Wire.setClock(50000)`, add 0.1µF caps.

**Handlers:** `build_flags = -DCRUMBS_MAX_HANDLERS=16` in platformio.ini.

Details: [platform-setup.md](docs/platform-setup.md)

---

## License

AGPL-3.0-or-later - see [LICENSE](LICENSE) file for details.

