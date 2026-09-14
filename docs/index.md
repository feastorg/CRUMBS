# CRUMBS documentation

One home per fact. Signatures and return codes live in the headers; the wire
format lives in `protocol.md`; everything else links to them.

## Start here

1. [Platform Setup](platform-setup.md) — install, build, wire and verify on
   Arduino IDE, PlatformIO and Linux.
2. `examples/core_usage/arduino/hello_*` — two boards talking.
3. [Protocol](protocol.md) — what is on the wire.
4. [Create a Family](create-a-family.md) — your own device vocabulary.

## Reference

| Document | Purpose |
| --- | --- |
| [Protocol](protocol.md) | Normative: frame, CRC-8, SET_REPLY, when the reply is built, address/type/opcode spaces, discovery probes |
| [Architecture](architecture.md) | Layers, dispatch order, handler tables, measured memory, HAL differences |
| [API Reference](api-reference.md) | Every public symbol by task, one line each, plus the return-code summary |
| [Create a Family](create-a-family.md) | The `therm` walkthrough: header, generated wrappers, peripheral, controller |
| [Examples](../examples/README.md) | Every example in learning order; each directory has its own README |
| [LHWIT Family](../examples/families_usage/lhwit_family/README.md) | The reference family: four Nano peripherals and two Linux controllers |

## Project

| Document | Purpose |
| --- | --- |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Build, test, the documentation gates, style, commits, CI, releasing |
| [Roadmap](roadmap.md) | Deferred directions and what is out of scope |
| [CHANGELOG.md](../CHANGELOG.md) | Release history |

The in-source Doxygen comments are the API contract; CI fails if a public
symbol lacks one or is missing from the API reference, and if any link on
these pages breaks.
