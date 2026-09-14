# Device families

A family is a shared header that fixes a device class's `type_id`, opcodes
and payload layouts, plus the controller-side functions that speak it; how to
write one is in [docs/create-a-family.md](../../docs/create-a-family.md).
This directory holds the reference family and the two Linux controllers that
drive it.

| Directory | What |
| --- | --- |
| [`lhwit_family/`](lhwit_family/README.md) | Four Arduino Nano peripherals (LED array, servo, calculator, 7-segment display) and their `*_ops.h` headers |
| [`controller_discovery/`](controller_discovery/README.md) | Linux shell that scans the bus, version-checks what it finds and binds the compatible devices |
| [`controller_manual/`](controller_manual/README.md) | The same shell over a fixed device list in `config.h` — the shape of a deployed controller |

Build the peripherals with PlatformIO from each device directory and the
controllers with the root CMake and the Linux HAL
([platform-setup.md](../../docs/platform-setup.md#linux)).
