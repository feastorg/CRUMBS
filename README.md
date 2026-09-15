# CRUMBS

[![CI](https://github.com/feastorg/CRUMBS/actions/workflows/ci.yml/badge.svg)](https://github.com/feastorg/CRUMBS/actions/workflows/ci.yml)
[![Pages](https://github.com/feastorg/feastorg.github.io/actions/workflows/pages.yml/badge.svg)](https://feastorg.github.io/crumbs/)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/cameronbrooks11/library/CRUMBS.svg)](https://registry.platformio.org/libraries/cameronbrooks11/CRUMBS)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/License-AGPL--3.0--or--later-blue.svg)](./LICENSE)

CRUMBS (Communications Router and Unified Message Broker System) is a small C
library for framed, CRC-checked messages between one I²C controller and any
number of peripherals. A frame is `type_id`, `opcode`, up to 27 payload bytes
and a CRC-8 — 4 to 31 bytes, sized to the AVR `Wire` buffer. The core is
plain C11 and tested on the host; thin HALs put it on Arduino (both roles)
and Linux (controller, over `i2c-dev`).

> _So you've mastered bits and bytes, maybe toiled with nibbles and words —
> get ready to indulge in some serious crumb crunching._

## What it gives you

- A wire format with validation before your code runs: a corrupt or foreign
  frame is a return code, never a callback.
- SET commands and GET queries: a peripheral registers a handler per opcode
  and a reply builder per query; a controller sends, or asks and reads.
- Device *families*: one shared header fixes a device class's type, opcodes
  and payload layouts, and generates the controller-side functions.
- Discovery: find CRUMBS devices on a bus, with their types, and tell them
  from everything else that ACKs.
- No heap, one static context; a minimal peripheral costs about 1.2 KB of
  flash and 160 B of RAM on an Uno.

## Quick start

Peripheral (Arduino):

```c
#include <crumbs_arduino.h>
#include <crumbs_message_helpers.h>

static crumbs_context_t ctx;

static void reply_version(crumbs_context_t *c, crumbs_message_t *r, void *u) {
    (void)c; (void)u;
    crumbs_build_version_reply(r, 0x01, 1, 0, 0);
}

void setup() {
    crumbs_arduino_init_peripheral(&ctx, 0x10);
    crumbs_register_reply_handler(&ctx, 0x00, reply_version, NULL);
}
void loop() {}
```

Controller (Linux):

```c
#include <crumbs_linux.h>

crumbs_context_t ctx; crumbs_linux_i2c_t bus; crumbs_message_t reply;
crumbs_linux_init_controller(&ctx, &bus, "/dev/i2c-1", 0);
crumbs_controller_read(&ctx, 0x10, &reply, crumbs_linux_read, &bus);
/* reply.data: CRUMBS_VERSION (u16 LE), module 1.0.0 */
```

Flash `examples/core_usage/arduino/hello_peripheral` and `hello_controller`
to two boards, wire SDA, SCL and ground, open the controller's serial monitor
at 115200 and press `s` and `r`.

## Install

- **PlatformIO:** `lib_deps = cameronbrooks11/CRUMBS@^0.13.0`
- **Arduino IDE:** clone into `~/Arduino/libraries/CRUMBS` (not in the
  Library Manager)
- **Linux:** CMake, with [linux-wire](https://github.com/feastorg/linux-wire) ≥ 0.1.3

Details, wiring and troubleshooting: [docs/platform-setup.md](docs/platform-setup.md).

## Documentation

| | |
| --- | --- |
| [docs/index.md](docs/index.md) | The map of everything below |
| [docs/protocol.md](docs/protocol.md) | The wire format, normative |
| [docs/architecture.md](docs/architecture.md) | How the pieces fit; dispatch order; memory; HAL differences |
| [docs/api-reference.md](docs/api-reference.md) | Every public symbol by task; the headers hold the contracts |
| [docs/create-a-family.md](docs/create-a-family.md) | Build a device family |
| [examples/](examples/README.md) | Every example, in learning order |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Build, test, checks, releasing |

## License

AGPL-3.0-or-later — see [LICENSE](LICENSE).
