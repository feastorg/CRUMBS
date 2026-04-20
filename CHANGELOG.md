# CHANGELOG

All notable changes to CRUMBS are documented in this file.

---

## [Unreleased]

## [0.12.2] - 2026-04-20

### Fixed

- Added `crumbs::crumbs` alias target immediately after `add_library(crumbs ...)` so that consumers using `add_subdirectory` can reference `crumbs::crumbs` consistently, matching the namespaced target provided by the installed package config.

### Added

- **Raw I2C helper APIs** (`src/crumbs.h`, `src/core/crumbs_i2c_helpers.c`)
  - `crumbs_i2c_dev_write`, `crumbs_i2c_dev_read`, `crumbs_i2c_dev_write_then_read`
  - register helpers: `read_reg_ex` / `write_reg_ex`, plus `u8` and `u16be` wrappers
  - standardized `CRUMBS_I2C_DEV_E_*` return codes
- **Candidate-address CRUMBS scanner**
  - `crumbs_controller_scan_for_crumbs_candidates(...)` for mixed-bus safe probing without broad sweeps
- **HAL combined transfer support**
  - `crumbs_linux_write_then_read(...)`
  - `crumbs_arduino_write_then_read(...)`
- **Mixed-bus validation examples**
  - Linux: `examples/core_usage/linux/mixed_bus_probe/`
  - Arduino: `examples/core_usage/arduino/mixed_bus_controller/`
  - includes CRUMBS candidate scanning + non-CRUMBS sensor register I/O flow
- **Coverage for new helpers**
  - `tests/test_i2c_helpers.c`
- **Core Arduino example consistency**
  - added simple `LED_BUILTIN` heartbeat across core Arduino examples
  - standardized `config.h` usage in core Arduino examples
- **ESP32 CI build validation**
  - `[env:esp32dev]` coverage added across PlatformIO examples and CI workflow steps
- `docs/platform-setup.md` ESP32 PlatformIO snippet with I2C pin guidance

### Fixed

- Linux mixed-bus controller parser and CLI path issues for `read-ex`/`write-ex` handling
- Linux HAL portability by defining `_DEFAULT_SOURCE` for `usleep()` under `-std=c11`
- handlers_usage mock controller examples updated to `crumbs_device_t` APIs and query helpers (Linux + PlatformIO), resolving CI compile failures
- `LED_BUILTIN` fallback for ESP32 examples (`#ifndef LED_BUILTIN ... #define LED_BUILTIN 2`)
- API reference scan behavior documentation corrections

### Changed

- **Mixed-bus Arduino controller flow** (`examples/core_usage/arduino/mixed_bus_controller/`)
  - startup validation pass + periodic status pass (default 5s)
  - CRUMBS query reply printout for discovered devices
  - BMP/BME chip-id + raw sample byte printout per configured sensor
- **Core docs and examples updated for mixed-bus usage**
  - explicit min/max topology guidance
  - unique address guidance for CRUMBS peripherals
  - candidate-address scanning pattern documented
- **License posture aligned to AGPL**
  - repository license is GNU AGPL v3
  - metadata/docs references updated to AGPL-3.0-or-later
- **Repository line ending policy added**
  - `.gitattributes` now enforces LF for text and CRLF for Windows scripts to reduce cross-platform churn
- **Code quality**
  - read-helper bounds checks use explicit `size_t` casts in `crumbs_message_helpers.h` to avoid conversion/sign-compare warning noise
- Post-0.11.0 version/docs sweep:
  - stale version references corrected across examples/docs
  - README badge/layout cleanup and `.gitignore` log pattern expansion

---

## [0.11.0] - Family Receiver API

### Added

- **`crumbs_controller_read()`** (`src/crumbs.h`, `src/core/crumbs_core.c`)
  - Symmetric counterpart to `crumbs_controller_send()`
  - Reads a single CRUMBS frame from a peripheral using any `crumbs_i2c_read_fn`
  - Returns `-1` for reads shorter than the minimum valid frame (4 bytes)

- **`crumbs_delay_fn` typedef** (`src/crumbs_i2c.h`)
  - `void (*crumbs_delay_fn)(uint32_t us)` — platform-agnostic microsecond delay callback
  - Used by `_get_*` wrapper functions to insert the required settle time between write and read

- **`CRUMBS_DEFAULT_QUERY_DELAY_US`** (`src/crumbs_i2c.h`)
  - Default 10 ms delay constant for the SET_REPLY two-step query pattern

- **Platform delay implementations**
  - `crumbs_linux_delay_us(uint32_t us)` (`src/crumbs_linux.h`, `src/hal/linux/crumbs_i2c_linux.c`) — wraps `usleep()`
  - `crumbs_arduino_delay_us(uint32_t us)` (`src/crumbs_arduino.h`) — wraps `delayMicroseconds()`, static inline

- **Result structs and `_get_*` functions for all four lhwit_family ops headers**
  - `led_ops.h`: `led_state_result_t`, `led_blink_result_t`, `led_get_state()`, `led_get_blink()`
  - `servo_ops.h`: `servo_pos_result_t`, `servo_speed_result_t`, `servo_get_pos()`, `servo_get_speed()`
  - `calculator_ops.h`: `calc_result_t`, `calc_hist_meta_t`, `calc_hist_entry_t`, `calc_get_result()`, `calc_get_hist_meta()`, `calc_get_hist_entry()`
  - `display_ops.h`: `display_value_result_t`, `display_query_value()`, `display_get_value()`
  - All `_get_*` functions follow the unified pattern: `query → delay → crumbs_controller_read → parse`

- **Receiver API for `mock_ops.h`** (`examples/handlers_usage/mock_ops.h`)
  - `mock_echo_result_t`, `mock_status_result_t`, `mock_info_result_t`
  - `mock_get_echo()`, `mock_get_status()`, `mock_get_info()` static inline wrappers

- **`docs/create-a-family.md`** — new guide for authoring custom CRUMBS families
  - Eight-step walkthrough using a toy thermometer peripheral as a running example
  - Covers: type ID selection, ops header structure, SET/SET_REPLY op pairs, result structs, `_get_*` pattern, Linux and Arduino usage, helper reference tables, and a completeness checklist

- **`src/crumbs_ops.h`** — new helper macro header for family ops-header authors
  - `CRUMBS_DEFINE_GET_OP(family, name, type_id, opcode, result_t, parse_fn)` — generates the `_query_*` (internal) + `_get_*` (public) function pair for a standard 1:1 opcode→result GET
  - `CRUMBS_DEFINE_SEND_OP(family, name, type_id, opcode, param_decl, pack_stmt)` — generates a `_send_*` wrapper for a single-parameter SET
  - `CRUMBS_DEFINE_SEND_OP_0(family, name, type_id, opcode)` — zero-parameter variant (e.g. `display_send_clear`)
  - Multi-parameter SETs and parameterized GET queries must still be written as normal `static inline` functions; the lhwit_family headers serve as reference implementations

- **`crumbs_register_reply_handler()`** (`src/crumbs.h`, `src/core/crumbs_core.c`)
  - New per-opcode reply dispatch for peripheral GET ops — symmetric counterpart to `crumbs_register_handler()` for SET ops
  - New typedef `crumbs_reply_fn`: `void (*)(crumbs_context_t*, crumbs_message_t*, void*)` — same shape as `crumbs_handler_fn` but for reply builders
  - `crumbs_peripheral_build_reply()` updated to check the reply handler table first; falls through to `on_request` callback if no matching handler is registered (fully backward-compatible)
  - Reply handler table added to `crumbs_context_s` using the same `CRUMBS_MAX_HANDLERS`-sized parallel arrays as the SET handler table
  - `crumbs_init()` updated to zero `reply_handler_count`
  - `test_reply_handler.c` added: 12 tests covering registration, dispatch, fall-through, priority, overwrite, unregister, table-full, and user_data forwarding

### Added (continued)

- **`CMakePresets.json`** — three configure/build/test presets for the root project
  - `default`: Debug build, `build/`, tests + examples on, Linux HAL off
  - `release`: Release build, `build-release/`, same flags
  - `linux`: Debug build with `CRUMBS_ENABLE_LINUX_HAL=ON`, `build-linux/`
  - Enables `cmake --preset default` / `cmake --build --preset default` / `ctest --preset default` without specifying `-B` or `-C`

### Changed (continued)

- **`lhwit_family` peripheral examples converted to `crumbs_register_reply_handler()`**
  - `led/src/main.cpp`: `on_request` switch replaced with three static reply handler functions (`reply_handler_version`, `reply_handler_get_state`, `reply_handler_get_blink`) registered individually
  - `servo/src/main.cpp`: same pattern — three handlers (`reply_handler_version`, `reply_handler_get_pos`, `reply_handler_get_speed`)
  - `display/src/main.cpp`: two handlers (`reply_handler_version`, `reply_handler_get_value`); `Serial` debug output moved into the handler functions
  - `calculator/src/main.cpp`: three common GETs registered individually (`reply_handler_version`, `reply_handler_get_result`, `reply_handler_get_hist_meta`); the 12 `CALC_OP_GET_HIST_0..11` opcodes retained as an `on_request_hist` fallback to avoid exceeding `CRUMBS_MAX_HANDLERS` (4 SET + 15 GET = 19 > default limit of 16); inline comment explains the trade-off and illustrates the mixed-model pattern

### Fixed

- **`display/src/main.cpp` `on_request` default case** — missing `crumbs_msg_init` fallback added
  - Previously the `default:` branch only printed to Serial, leaving the reply struct uninitialised on any unknown opcode
  - Now calls `crumbs_msg_init(reply, DISPLAY_TYPE_ID, ctx->requested_opcode)` before logging, consistent with the led, servo, and calculator peripherals

- **Reply identity validation in all `_get_*` wrappers**
  - All 11 `_get_*` functions (across `led_ops.h`, `servo_ops.h`, `calculator_ops.h`, `display_ops.h`, `mock_ops.h`) now verify that `reply.type_id` and `reply.opcode` match the expected values before parsing
  - Returns `-1` immediately on any mismatch, preventing silent data corruption when a valid CRUMBS frame arrives from the wrong device type or address
  - For `calc_get_hist_entry`, the identity check precedes the `data_len < 16` check
  - Zero runtime cost on the happy path; two comparisons per GET on any error path

### Changed

- **`docs/create-a-family.md` updated to use `crumbs_device_t` and document macro usage**
  - All function signatures in Steps 3, 4, 6 converted from individual `(ctx, addr, write_fn, read_fn, delay_fn, io)` parameters to `const crumbs_device_t *dev`
  - New Step 7 showing `CRUMBS_DEFINE_GET_OP` / `CRUMBS_DEFINE_SEND_OP` applied to the thermometer running example; documents what the macros cover and what must still be written by hand

- **`docs/create-a-family.md` peripheral dispatch section added**
  - New "Peripheral implementation" section with SET handler table pattern, GET `on_request` pattern, and `default:` safe-fallback guidance
  - "Choosing the right mechanism" table clarifying when to use `crumbs_register_handler`, `on_request`, and `on_message`
  - SET/GET dispatch asymmetry acknowledged explicitly; `crumbs_register_reply_handler` noted as planned future fix
  - Checklist updated with three peripheral-side items

- **`hello_peripheral.ino` comment added** explaining that `on_message` is used for brevity only; points to `examples/families_usage/` for the handler-table pattern
  - Linux (Step 8) and Arduino (Step 9) usage examples updated to construct a `crumbs_device_t` and pass `&dev`
  - Identity check (`reply.type_id` + `reply.opcode`) added to the Step 6 `_get_*` example
  - `_query_*` functions marked `@internal` in Step 4; `@internal` rationale added to section intro
  - Checklist updated to include identity check and macro usage items

- **Example controllers updated to use `_get_*` wrappers** (`examples/families_usage/`)
  - `controller_manual/main.c`: replaced all manual `query + crumbs_linux_read_message` blocks with `_get_*` calls; removed now-unused `crumbs_message_t reply` declarations; added `get_blink` and `get_speed` commands
  - `controller_discovery/main.c`: same replacements; scan-phase version reads left unchanged

- **Two new unit tests added** (`tests/test_reply_flow.c`)
  - `test_controller_read_ok`: verifies `crumbs_controller_read` decodes a valid frame
  - `test_controller_read_short`: verifies `-1` is returned for undersized reads

- **`display_ops.h` aligned with all other lhwit_family ops headers** (`examples/families_usage/lhwit_family/display_ops.h`)
  - Added `display_send_set_number()`, `display_send_set_segments()`, `display_send_set_brightness()`, and `display_send_clear()` — direct-send wrappers matching the `led_send_*()` / `servo_send_*()` pattern used by every other ops header
  - The existing `display_build_*` functions are retained but the section header now documents them as low-level message builders for callers that need to inspect or modify a message before sending manually; `display_send_*` is the preferred interface for all standard usage
  - `controller_manual/main.c` and `controller_discovery/main.c` updated to call `display_send_*` directly; now-unused `crumbs_message_t msg` declaration removed from `cmd_display` in both files
  - `lhwit_family/README.md` usage example updated to the single-call `display_send_*` pattern

- **Removed `address` field from `crumbs_message_t`** (`src/crumbs_message.h`, `src/core/crumbs_core.c`)
  - The field was never serialized, never read by any example or handler, and carried no information not already available via `ctx->address` in every callback
  - `crumbs_peripheral_handle_receive` no longer writes `msg.address = ctx->address`
  - `test_handle_receive_sets_address` removed from `tests/test_peripheral_flow.c` (tested a write with no corresponding read)

- **`crumbs_linux_read_message` marked deprecated** (`src/crumbs_linux.h`)
  - Prefer `crumbs_controller_read(ctx, addr, &msg, crumbs_linux_read, io)` for
    portable code; `crumbs_linux_read_message` is retained for compatibility
  - Added explanatory comment at the one surviving call site in
    `controller_discovery/main.c` (raw bus scan before device handles exist)

- **`crumbs_context_size()` guard added to `hello_peripheral`**
  (`examples/core_usage/arduino/hello_peripheral/hello_peripheral.ino`)
  - Catches silent `CRUMBS_MAX_HANDLERS` mismatch between sketch and library
  - Halts with `FATAL: CRUMBS_MAX_HANDLERS mismatch!` on Serial if sizes diverge;
    zero cost on correctly configured projects

- **`crumbs_device_t` bound-device handle** (`src/crumbs.h`)
  - New POD struct `crumbs_device_t { ctx, addr, write_fn, read_fn, delay_fn, io }` that bundles all per-device transport state into one value
  - All five ops headers updated: every wrapper function now accepts `const crumbs_device_t *dev` instead of the previous explicit `(ctx, addr, write_fn, [read_fn, delay_fn,] io)` parameter list
    - `led_ops.h` — 3 send, 2 query (internal), 2 get
    - `servo_ops.h` — 3 send, 2 query (internal), 2 get
    - `calculator_ops.h` — 4 send, 3 query (internal), 3 get
    - `display_ops.h` — 4 send, 1 query (internal), 1 get (build/parse helpers unchanged)
    - `mock_ops.h` — 3 send, 3 query (internal), 3 get
  - Call sites reduced from 4–7 arguments to 1–3 (e.g. `led_send_set_all(dev, mask)`)

- **Example controllers redesigned around `crumbs_device_t`** (`examples/families_usage/`)
  - `device_info_t` now embeds `crumbs_device_t dev`; populated once at startup (manual) or once compat passes (discovery)
  - `resolve_device()` helper replaces the ~30-line `@addr`/index parsing block previously duplicated inside every `cmd_*` function (8 copies eliminated)
  - `find_device()` and `check_device_compat()` removed; their logic is unified in `resolve_device()`
  - All `cmd_*` signatures reduced to `(const crumbs_device_t *dev, const char *rest)`
  - Dispatch in `main()` calls `resolve_device` then passes `&d->dev` to the appropriate `cmd_*`

---

## [0.10.3] - Examples Improvements

### Added

- **Phase 1: Minimal Hello Examples**
  - `hello_peripheral` (33 lines) - Absolute minimal peripheral example
  - `hello_controller` (52 lines) - Minimal controller with send/request pattern
  - Enables 15-minute time-to-first-success for new users (down from 45+ minutes)
  - Both examples compile to <15% flash, <30% RAM on Arduino Nano

- **Phase 2: Basic Examples with Multi-Command Pattern**
  - `basic_peripheral` (72 lines) - Multi-command with state management (store/clear/query)
  - `basic_controller` (118 lines) - Key command interface (s/c/v/d) with two-step SET_REPLY pattern
  - 52% line reduction from previous simple_peripheral (149→72 lines)
  - Clean output with no F() macros or CRC spam

### Changed

- **Phase 2: Renamed Examples for Clarity**
  - `simple_controller` → `advanced_controller` with "Level 3: Production Patterns" marker
  - Kept `simple_peripheral` as diagnostics example (CRC monitoring)

- **Phase 3: Reduced Verbosity in Tier 2/3 Examples**
  - Removed F() macro spam across all handlers_usage examples
  - Removed per-operation Serial output from mock_controller/mock_peripheral
  - Removed arithmetic logging from calculator peripheral (kept div-by-zero error)
  - Removed LED state logging from led peripheral
  - ~50% reduction in Serial output while preserving all functionality

### Documentation

- **Phase 4: Clear Learning Progression**
  - Updated `examples/README.md` with Quick Start referencing hello examples
  - Added prerequisites to Tier 2 (handlers_usage) and Tier 3 (families_usage) READMEs
  - Fixed tier comparison tables (Tier 1: core_usage, Tier 2: handlers_usage, Tier 3: families_usage)
  - Updated `docs/getting-started.md` with examples progression as first Next Step
  - All documentation now reflects 4-level learning path: Hello → Basic → Diagnostics → Advanced

### Success Metrics

- First example size: 149 lines → 33 lines (78% reduction) ✓
- Time to first success: 45 min → 10-15 min (67% improvement) ✓
- Binary sizes: All examples <15% flash, <30% RAM ✓
- Serial verbosity: Reduced 50-70% across all tiers ✓
- Clear learning progression with prerequisites stated ✓

---

## [0.10.2] - Version Reply Helper

### Added

- **Version Reply Helper Function** (`crumbs_build_version_reply()`)
  - New helper in `crumbs_message_helpers.h` for building standard opcode 0x00 replies
  - Reduces boilerplate from 6 lines to 1 line in `on_request` callbacks
  - Enforces consistent version reply format: `[CRUMBS_VERSION:u16][major:u8][minor:u8][patch:u8]`
  - Includes comprehensive Doxygen documentation with usage examples

### Changed

- **All LHWIT Peripherals Updated**
  - Calculator, LED, Servo, and Display peripherals now use `crumbs_build_version_reply()`
  - Consistent version reporting across all reference implementations

### Documentation

- **developer-guide.md**: Added section on local development with PlatformIO symlinks
  - Documents `symlink://../../../../` pattern for testing local library changes
  - Explains relative path calculation and clean build workflow

---

## [0.10.1] - Linux Scan Improvements & Multi-Device Support

### Added

- **Linux Scan Wrappers with Error Suppression**
  - `crumbs_linux_scan_for_crumbs_with_types()`: Platform wrapper that suppresses expected I/O errors during scan
  - `crumbs_linux_scan_for_crumbs()`: Convenience wrapper (addresses only, no types)
  - Wrappers automatically call `lw_set_error_logging()` to eliminate scan noise
  - Clean abstraction: users no longer need to include `linux_wire.h` directly
  - Documentation in `crumbs_linux.h` explains automatic error suppression

### Changed

- **Multi-Device Architecture in LHWIT Controllers**
  - Both Manual and Discovery controllers now support multiple devices of the same type
  - Device selection pattern: `<type> <idx|@addr> <cmd> [args]`
    - Index-based: `led 0 set 255 128 0` (first LED found)
    - Address-based: `led @0x20 set 255 128 0` (LED at specific address)
  - Config format updated: `device_config_t[] = {{type_id, addr}, ...}`
  - Controllers maintain device arrays indexed by order discovered or configured

- **Peripheral Initialization Order Fix**
  - Fixed servo peripheral: `crumbs_arduino_init_peripheral()` now called BEFORE `crumbs_set_callbacks()`
  - Root cause: `crumbs_arduino_init_peripheral()` internally calls `crumbs_init()`, which was wiping callbacks
  - Pattern documented: always initialize peripheral context first, then set callbacks

- **Redundant Initialization Cleanup**
  - Removed redundant `crumbs_init()` calls in controllers
  - `crumbs_linux_init_controller()` already calls `crumbs_init()` internally
  - Applied to Manual Controller, Discovery Controller, and all Linux examples

### Examples

- **LHWIT Controllers Updated**
  - `controller_manual/`: Multi-device config with device selection by index or address
  - `controller_discovery/`: Uses new Linux scan wrapper, supports multiple devices
  - Both demonstrate robust device interaction with improved error handling

- **All Linux Examples Updated**
  - `core_usage/linux/simple_controller/`: Uses `crumbs_linux_scan_for_crumbs_with_types()`
  - `handlers_usage/linux/mock_controller/`: Uses `crumbs_linux_scan_for_crumbs_with_types()`
  - No longer include `linux_wire.h` directly

- **LHWIT Peripherals Fixed**
  - `servo/`: Fixed initialization order bug that prevented detection during scan
  - Verified: Both LED and Servo now detected correctly in hardware testing

### Documentation

- **controller_discovery/README.md**: Updated to reflect new Linux wrapper usage and multi-device support

---

## [0.10.0] - SET_REPLY Mechanism

### Added

- **SET_REPLY Command (0xFE)**: Library-handled opcode for reply staging
  - `CRUMBS_CMD_SET_REPLY` constant defined in `crumbs.h`
  - Library auto-intercepts 0xFE and stores target opcode
  - NOT dispatched to user handlers or `on_message` callbacks
  - Wire format: `[type_id][0xFE][0x01][target_opcode][CRC8]`

- **Requested Opcode Field** (`ctx->requested_opcode`)
  - New `uint8_t requested_opcode` field in `crumbs_context_s` (+1 byte)
  - Initialized to 0 in `crumbs_init()` (by convention: device info)
  - User's `on_request` callback switches on this value

- **Scan with Types** (`crumbs_controller_scan_for_crumbs_with_types()`)
  - Same as `crumbs_controller_scan_for_crumbs()` but returns `type_id` array
  - Enables device type discovery in a single scan
  - Optional `types` parameter (may be NULL)

- **New Tests**
  - `tests/test_set_reply.c`: Unit tests for SET_REPLY mechanism (8 tests)
  - `tests/test_reply_flow.c`: Integration tests for query flow (6 tests)
  - Added `scan_with_types` tests to `test_scan_fake.c`

- **Documentation**
  - `docs/versioning.md`: New document for opcode 0x00 convention
  - `docs/protocol.md`: Added Reserved Opcodes section
  - `docs/developer-guide.md`: Added SET_REPLY Pattern section
  - `docs/handler-guide.md`: Updated `on_request` example with case 0x00
  - `docs/examples.md`: Reorganized to reflect three-tier learning path

### Examples

- **Three-Tier Reorganization**: Examples restructured into progressive learning path
  - `examples/core_usage/`: Basic protocol learning (Tier 1)
  - `examples/handlers_usage/`: Production callback patterns (Tier 2)
  - `examples/families_usage/`: Real device families (Tier 3, planned)

- **Mock Handler Examples** (Type ID 0x10): Demonstrating handler registration and SET_REPLY
  - PlatformIO: `mock_peripheral/`, `mock_controller/` (Nano + ESP32)
  - Linux: `mock_controller/`
  - Shared `mock_ops.h` header with SET operations (ECHO, SET_HEARTBEAT, TOGGLE) and GET operations (GET_ECHO, GET_STATUS, GET_INFO)

- **LHWIT Family** (Low Hardware Implementation Test): Four-device reference family demonstrating interaction patterns
  - Peripherals (PlatformIO, Arduino Nano): Calculator (Type 0x03), LED Array (Type 0x01), Servo Controller (Type 0x02), Display (Type 0x04)
  - Controllers (Linux): Discovery Controller (scan-based), Manual Controller (config-based)
  - Canonical operation headers: `calculator_ops.h`, `led_ops.h`, `servo_ops.h`, `display_ops.h`, `lhwit_ops.h`
  - Comprehensive guide: `docs/lhwit-family.md` (hardware setup, wiring, testing procedures)
  - Demonstrates function-style (Calculator), state-query (LED), position-control (Servo), and display-control (Display) patterns

### Memory Impact

- +1 byte per `crumbs_context_t` (for `requested_opcode` field)

---

## [0.9.5] - Foundation Revision

### Added

- **Version Header** (`src/crumbs_version.h`): Runtime version query and conditional compilation
  - `CRUMBS_VERSION_MAJOR`, `CRUMBS_VERSION_MINOR`, `CRUMBS_VERSION_PATCH`
  - `CRUMBS_VERSION_STRING` - String representation ("0.9.5")
  - `CRUMBS_VERSION` - Integer version (905 = major*10000 + minor*100 + patch)

- **Platform Timing API**: Foundation for multi-frame timeout detection (v0.11.0)
  - `crumbs_platform_millis_fn` typedef in `crumbs_i2c.h`
  - `crumbs_arduino_millis()` - Arduino millisecond timer
  - `crumbs_linux_millis()` - Linux CLOCK_MONOTONIC millisecond timer
  - `CRUMBS_ELAPSED_MS(start, now)` - Wraparound-safe elapsed time calculation
  - `CRUMBS_TIMEOUT_EXPIRED(start, now, timeout_ms)` - Wraparound-safe timeout check

- **Platform Timing Documentation**: See `docs/developer-guide.md` for usage examples

### Changed

- **BREAKING**: Renamed `crumbs_msg.h` → `crumbs_message_helpers.h` for clarity
  - Eliminates confusion with `crumbs_message.h` (structure definition)
  - All includes and documentation updated

- **BREAKING**: Renamed `command_type` field → `opcode` in `crumbs_message_t`
  - Semantically correct terminology for protocol operations
  - Updated all references across codebase, examples, tests, and documentation

---

## [0.9.4] - Message Builder/Reader Helpers

### Added

- **Message Builder/Reader API** (`src/crumbs_message_helpers.h`): Zero-overhead inline helpers for structured payload construction and reading
  - `crumbs_msg_init(msg, type_id, opcode)`: Initialize a message with header fields
  - `crumbs_msg_add_u8/u16/u32()`: Append unsigned integers (little-endian)
  - `crumbs_msg_add_i8/i16/i32()`: Append signed integers (little-endian)
  - `crumbs_msg_add_float()`: Append 32-bit float (native byte order)
  - `crumbs_msg_add_bytes()`: Append raw byte arrays
  - `crumbs_msg_read_u8/u16/u32()`: Read unsigned integers from payload
  - `crumbs_msg_read_i8/i16/i32()`: Read signed integers from payload
  - `crumbs_msg_read_float()`: Read 32-bit float from payload
  - `crumbs_msg_read_bytes()`: Read raw byte arrays from payload
  - All functions return 0 on success, -1 on bounds overflow
  - Header-only design: `static inline` functions for zero call overhead

- **Contract Header Pattern**: Demonstrated in `examples/handlers_usage/mock_ops.h`
  - Shared header defining TYPE_ID, opcodes, and helper functions
  - Controller and peripheral both include the same contract
  - Demonstrates the "copy and customize" pattern for device families

- **Configurable Handler Table Size** (`CRUMBS_MAX_HANDLERS`):
  - Compile-time option to control handler dispatch table size
  - Default: 16 (good balance of RAM vs handler capacity)
  - Set lower values (e.g., 4 or 8) to reduce RAM on constrained devices
  - Set to 0 to disable handler dispatch entirely
  - Uses O(n) linear search (fast for typical handler counts)

### Documentation

- New documentation: `docs/message-helpers.md` — comprehensive guide to crumbs_message_helpers.h
- Updated `docs/api-reference.md` with message helper API
- Updated `docs/index.md` to reflect variable-length payload (was outdated)

### Notes

- This layer composes with the existing handler dispatch system (0.8.x)
- No breaking changes — all existing code continues to work
- Float encoding uses native byte order; document this if crossing architectures

## [0.8.x] - Command Handler Dispatch System v0

### Added

- **Command Handler Dispatch System**: Per-command-type handler registration for structured message processing
  - `crumbs_handler_fn` typedef: `void (*)(crumbs_context_t*, const crumbs_message_t* message, void* user_data)`
  - `crumbs_register_handler(ctx, opcode, fn, user_data)`: Register handler for a command type
  - `crumbs_unregister_handler(ctx, opcode)`: Clear handler for a command type
  - Handlers are invoked after `on_message` callback (if both are set)
  - O(n) dispatch via configurable handler table (default 16 entries)
- **Handler Examples**: See `examples/handlers_usage/` for mock device examples

## [0.7.x] - Variable-Length Payload

### Breaking Changes

This release introduces a **breaking change** to the CRUMBS wire format and C API.
All code using CRUMBS must be updated.

#### Wire Format

| Old Format (31 bytes fixed) | New Format (4–31 bytes variable) |
| --------------------------- | -------------------------------- |
| type_id (1)                 | type_id (1)                      |
| opcode (1)                  | opcode (1)                       |
| data (28) — 7 × float32     | data_len (1) — 0–27              |
| crc8 (1)                    | data (0–27) — raw bytes          |
|                             | crc8 (1)                         |

#### API Changes

| Old Constant/Field         | New Constant/Field                      |
| -------------------------- | --------------------------------------- |
| `CRUMBS_DATA_LENGTH` (7)   | `CRUMBS_MAX_PAYLOAD` (27)               |
| `CRUMBS_MESSAGE_SIZE` (31) | `CRUMBS_MESSAGE_MAX_SIZE` (31)          |
| `float data[7]`            | `uint8_t data_len` + `uint8_t data[27]` |

#### Migration Guide

1. **Update message construction:**

   ```c
   // Old
   msg.data[0] = 1.0f;
   msg.data[1] = 2.0f;

   // New — raw bytes; encode floats manually
   float val = 1.0f;
   msg.data_len = sizeof(float);
   memcpy(msg.data, &val, sizeof(float));
   ```

2. **Update message reading:**

   ```c
   // Old
   float val = msg.data[0];

   // New — decode from bytes
   float val;
   if (msg.data_len >= sizeof(float)) {
       memcpy(&val, msg.data, sizeof(float));
   }
   ```

3. **Update buffer size constants:**

   ```c
   // Old
   uint8_t buf[CRUMBS_MESSAGE_SIZE];

   // New
   uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
   ```

4. **Note encoder return value:**
   - `crumbs_encode_message()` now returns the actual encoded length (4 + data_len), not a fixed size.

### Added

- Variable-length payload support (0–27 bytes per message)
- `data_len` field in `crumbs_message_t` for explicit payload size
- Raw byte payloads — application defines interpretation

### Changed

- Wire format now includes explicit `data_len` field
- CRC computed over header + actual payload bytes only
- Encoder returns variable length based on payload size
- All examples and documentation updated for byte API

### Removed

- Fixed float-based payload (`float data[7]`)
- `CRUMBS_DATA_LENGTH` and `CRUMBS_MESSAGE_SIZE` constants

---

## [0.6.x] - C API Rewrite & Platform Expansion (~November 2025)

Complete reboot of the library as a proper C99 project. The Arduino-sketch
prototype was replaced with a structured cross-platform library.

### Added

- **C99 library structure** with CMake build system, `src/core/`, `src/crc/`,
  `src/hal/arduino/`, `src/hal/linux/` source layout
- **Custom CRC8 nibble algorithm** (`src/crc/crc8_nibble.c/.h`) — replaces
  the AceCRC C++ dependency; `scripts/generate_crc8.py` regenerates the
  lookup tables from scratch
- **Linux HAL** (`src/hal/linux/crumbs_i2c_linux.c`) via `linux-wire`,
  enabling native Linux I²C without Arduino runtime
- **CRUMBS-aware I²C scanner** (`crumbs_controller_scan_for_crumbs()`)
  for device discovery on the bus
- **CMake install / `find_package(crumbs)`** support for out-of-tree projects
  (`cmake/crumbsConfig.cmake.in`, `GNUInstallDirs`)
- **PlatformIO** `library.json` and example project structure; registry
  publish and CI guidance
- **GitHub Actions CI** workflow: CMake build + CTest on Ubuntu and
  native-C example step; Doxygen doc-check step
- **CTest suite** bootstrapped: `test_encode_decode`, `test_scan_fake`,
  `test_crc_failure`
- `basic_peripheral_noncrumbs` sketch for compatibility testing with
  non-CRUMBS I²C devices

### Changed

- Rewritten from C++ Arduino sketches to C99 throughout; removed AceCRC
  and ACE namespace layers
- Examples split into `arduino/` and `linux/` subtrees under
  `examples/core_usage/`
- Source tree cleaned of legacy `src_old/` and `_reference/` directories
- `library.properties` updated to remove AceCRC dependency entry

---

## [Pre-release] — Arduino Prototype (2024-10 to 2025-11)

Not versioned. CRUMBS lived as a pair of Arduino sketches demonstrating
a minimal I²C message protocol before the C library existed.

### 2025-09 to 2025-11: Docs, planning, CRC research

- First documentation pass (index, getting-started, API reference)
- Architecture diagrams and revision planning added
- CRC-8 support researched and added to the Arduino sketch (untested due to
  hardware unavailable at the time)
- Float endianness serialization helpers (`memcpy`-based) added to examples

### 2024-10 to 2025-03: Initial prototype

- Fixed-width 32-byte CRUMBS frame: `[type_id:u8][opcode:u8][data:28 bytes][crc:u8]`
  — `data` was 7 × `float32` (little-endian via `memcpy`)
- Controller and peripheral Arduino sketch pair for the Nano; basic
  send/receive with an `on_message` callback
- CBOR serialization evaluated and removed — too heavy for Nano RAM
- Message trimmed from early experiments to 32-byte default I²C buffer size
- Terminology updated: `master`/`slave` → `controller`/`peripheral` (Mar 2025)
- TWI clock speed moved from a magic number to a `#define`

### Notes

- 5 V Arduino Nanos had I²C level-compatibility issues with 3.3 V I²C devices
  encountered in early hardware testing
- `on_message` callback pattern established here; carried forward unchanged
  into the C rewrite
