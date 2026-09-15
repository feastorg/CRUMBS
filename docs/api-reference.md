# API Reference

Every public symbol, grouped by task, one line each. The headers are the
reference: each entry's Doxygen comment carries the full contract, and CI
fails if a public symbol lacks one or is missing from this page. Return codes
are collected at the end.

| Header | Include when |
| --- | --- |
| `crumbs.h` | always — context, codec, dispatch, controller, scanners, raw I²C helpers |
| `crumbs_message_helpers.h` | building or reading payloads |
| `crumbs_ops.h` | defining a family: identity, payload codecs, controller-side wrappers |
| `crumbs_arduino.h` / `crumbs_linux.h` | the HAL for your platform |
| `crumbs_message.h`, `crumbs_i2c.h`, `crumbs_crc.h`, `crumbs_version.h` | pulled in by `crumbs.h` |

## Context and lifecycle — `crumbs.h`

| Symbol | |
| --- | --- |
| `crumbs_context_t` | One per role. Address, role, declared type, CRC statistics, callbacks, requested opcode, handler tables. Size depends on `CRUMBS_MAX_HANDLERS`. |
| `crumbs_role_t` | `CRUMBS_ROLE_CONTROLLER` (0) or `CRUMBS_ROLE_PERIPHERAL` (1). |
| `crumbs_init(ctx, role, address)` | Zero the counters and callbacks; keep `address` only for a peripheral. Handler arrays are left as they are, counts reset. |
| `crumbs_set_callbacks(ctx, on_message, on_request, user_data)` | Install or clear (`NULL`) the fallback callbacks. |
| `crumbs_set_type_id(ctx, type_id)` | Declare the peripheral's type; frames for other non-zero types are then dropped. `0` clears. |
| `crumbs_context_size()` | `sizeof(crumbs_context_t)` as the library was compiled — compare with the sketch's `sizeof` at startup to catch a `CRUMBS_MAX_HANDLERS` mismatch. |
| `crumbs_message_cb_t` | `on_message(ctx, msg)`: every validated non-SET_REPLY frame, before any handler. Peripheral only. |
| `crumbs_request_cb_t` | `on_request(ctx, reply)`: build the reply for `ctx->requested_opcode` when no reply handler matches. |
| `CRUMBS_MAX_HANDLERS` | Entries per handler table, default 16; `0` compiles both tables out. Must be set as a build flag, identically for library and sketch. |
| `CRUMBS_TYPE_ID_ANY` | `0x00`: the wildcard type in a frame, and "no declared type" for a context. |

## Messages and framing

| Symbol | |
| --- | --- |
| `crumbs_message_t` | `type_id`, `opcode`, `data_len`, `data[27]`, `crc8`. 31 bytes. The decoder fills `crc8`; the encoder does not. — `crumbs_message.h` |
| `CRUMBS_MAX_PAYLOAD` | 27. — `crumbs_message.h` |
| `CRUMBS_MESSAGE_MAX_SIZE` | 31: the largest frame, and the read length the controller uses. — `crumbs_message.h` |
| `crumbs_encode_message(msg, buffer, buffer_len)` | Serialise with CRC. Returns `4 + data_len`, or 0 for `NULL` args, `data_len > 27`, or a short buffer. — `crumbs.h` |
| `crumbs_decode_message(buffer, buffer_len, msg, ctx)` | Validate and parse an exact-length frame. `0`, `-1` structural, `-2` CRC. `ctx` may be `NULL`; otherwise its statistics update. — `crumbs.h` |
| `crumbs_frame_length(buffer, buffer_len, *frame_len)` | Header-declared length of a possibly padded read, for callers that decode raw reads themselves. `-1` for `NULL` args, fewer than 4 bytes, `data_len > 27`, or a buffer shorter than its header declares. — `crumbs.h` |
| `crumbs_crc8(data, len)` → `crumbs_crc8_t` | CRC-8/SMBUS (poly `0x07`, init 0). Returns 0 for `NULL` or empty input. — `crumbs_crc.h` |

### Payload helpers — `crumbs_message_helpers.h`

All `static inline`. Multi-byte integers are little-endian; floats are 4 native
bytes. Every add fails atomically with `-1` when the payload would exceed 27
bytes; every read fails with `-1` when `offset + width > len`.

| Symbol | |
| --- | --- |
| `crumbs_msg_init(msg, type_id, opcode)` | Zero the message and set the header. |
| `crumbs_msg_add_u8`, `crumbs_msg_add_u16`, `crumbs_msg_add_u32`, `crumbs_msg_add_i8`, `crumbs_msg_add_i16`, `crumbs_msg_add_i32`, `crumbs_msg_add_float` `(msg, value)` | Append one value. |
| `crumbs_msg_add_bytes(msg, data, len)` | Append raw bytes; `len == 0` is a no-op. |
| `crumbs_msg_read_u8`, `crumbs_msg_read_u16`, `crumbs_msg_read_u32`, `crumbs_msg_read_i8`, `crumbs_msg_read_i16`, `crumbs_msg_read_i32`, `crumbs_msg_read_float` `(data, len, offset, *out)` | Read one value from a payload. |
| `crumbs_msg_read_bytes(data, len, offset, out, count)` | Copy raw bytes out. |
| `crumbs_build_version_reply(reply, type_id, major, minor, patch)` | The opcode `0x00` reply: `CRUMBS_VERSION` (u16) then the three module bytes. Returns `0`, or `-1` for `NULL`. |

## Peripheral — `crumbs.h`

| Symbol | |
| --- | --- |
| `crumbs_peripheral_handle_receive(ctx, buffer, len)` | Decode → type check → SET_REPLY intercept → `on_message` → matching handler. `0`, `-1`, `-2`, or `CRUMBS_RX_TYPE_MISMATCH`. The HAL calls this from its receive callback. |
| `crumbs_peripheral_build_reply(ctx, out_buf, out_buf_len, *out_len)` | Reply handler for the requested opcode, else `on_request`, else nothing (`*out_len = 0`, returns 0). `-1` bad args, `-2` encode failed. The HAL calls this from its request callback. |
| `crumbs_register_handler(ctx, opcode, fn, user_data)` | Add or replace the SET handler for `opcode`; `fn == NULL` removes it. `-1` when the table is full, `ctx` is `NULL`, or tables are compiled out. |
| `crumbs_unregister_handler(ctx, opcode)` | Same as registering `NULL`. |
| `crumbs_register_reply_handler(ctx, opcode, fn, user_data)` | Same contract for the reply-handler table; `fn == NULL` removes. |
| `crumbs_handler_fn` | `fn(ctx, opcode, data, data_len, user_data)`. `data` is never `NULL`. |
| `crumbs_reply_fn` | `fn(ctx, reply, user_data)`: fill `reply`; an untouched reply goes out as `00 00 00 00`. |
| `CRUMBS_CMD_SET_REPLY` | `0xFE`. Never dispatched to user code. |
| `CRUMBS_RX_TYPE_MISMATCH` | `-8`: frame carried another non-zero type. Not a CRC error. |

## Controller — `crumbs.h`, `crumbs_ops.h`

| Symbol | |
| --- | --- |
| `crumbs_controller_send(ctx, target_addr, msg, write_fn, write_ctx)` | Encode and write one frame. `-1` `NULL` args, `-2` wrong role, `-3` encode failed, otherwise the HAL write's return verbatim (0 = success; a HAL's own negative codes can collide with those three). |
| `crumbs_controller_read(ctx, target_addr, *out_msg, read_fn, read_ctx)` | Read 31 bytes, trim, decode. `-1` short read or bad header, `-2` CRC, `0` with `out_msg` filled. Accepts any identity. |
| `crumbs_controller_read_expect(ctx, target_addr, expect_type_id, expect_opcode, *out_msg, read_fn, read_ctx)` | `crumbs_controller_read` plus an identity check; `CRUMBS_RX_REPLY_MISMATCH` with `out_msg` still filled. `expect_type_id == CRUMBS_TYPE_ID_ANY` skips the type half; the opcode is always compared. |
| `CRUMBS_RX_REPLY_MISMATCH` | `-7`. |
| `crumbs_device_t` | `ctx`, `addr`, `write_fn`, `read_fn`, `delay_fn`, `io`: one bound target for the wrappers and raw helpers. |
| `crumbs_ops_can_send(dev)` / `crumbs_ops_can_get(dev)` | Whether `dev` has what a send (context + write) or a get (also read + delay) needs. |
| `CRUMBS_DEFINE_FAMILY(PREFIX, type_id, OPS)` | From an `X(NAME, value)` list: `PREFIX_TYPE_ID` and one enum constant per opcode. Fails the build if the type is not `0x01`–`0xFF`, an opcode is above `0xFF` or is `0xFE`, or two opcodes share a value. |
| `CRUMBS_DEFINE_PAYLOAD(name, wire_bytes, FIELDS)` | From an `X(type, name)` list (`u8 u16 u32 i8 i16 i32 float`, wire order): `name_t`, `name_wire_size`, `name_pack(msg, *v)` (appends; `-1` if it would exceed 27) and `name_unpack(data, len, *v)` (`-1` if `len < name_wire_size`; longer accepted). Fails the build if the list does not sum to `wire_bytes` or `wire_bytes` exceeds 27. `name_unpack` is a `parse_fn` for `CRUMBS_DEFINE_GET_OP`. |
| `CRUMBS_STATIC_ASSERT(cond, msg)` | `static_assert` / `_Static_assert`, usable at file scope in C11 and C++11. |
| `CRUMBS_DEFINE_SEND_OP(family, name, type_id, opcode, param_decl, pack_stmt)` | Defines `family_send_name(dev, param)`: one SET with one packed parameter. |
| `CRUMBS_DEFINE_SEND_OP_0(family, name, type_id, opcode)` | Defines `family_send_name(dev)`: a payload-less SET. |
| `CRUMBS_DEFINE_GET_OP(family, name, type_id, opcode, result_t, parse_fn)` | Defines `family_query_name(dev)` and `family_get_name(dev, *out)`: SET_REPLY, `delay_fn(CRUMBS_DEFAULT_QUERY_DELAY_US)`, `read_expect`, `parse_fn`. `-1` unbound device or `NULL` `out`, else the send/read code, `-7` on identity mismatch, else `parse_fn`'s return. |
| `CRUMBS_DEFAULT_QUERY_DELAY_US` | 10 000 µs between the SET_REPLY write and the read. — `crumbs_i2c.h` |

## Discovery

| Symbol | |
| --- | --- |
| `crumbs_controller_scan_for_crumbs(ctx, start, end, strict, write_fn, read_fn, io_ctx, found[], max_found, timeout_us)` | Read each address and count it if the bytes decode as a frame; non-strict (with `ctx` and `write_fn` given) also writes `00 00 00 00` to addresses whose read did not decode and retries — a peripheral dispatches that as an opcode-`0x00` SET. Returns the count (stops at `max_found`), `-1` bad args. `timeout_us` goes to every read. — `crumbs.h` |
| `crumbs_controller_scan_for_crumbs_with_types(…, found[], types[], max_found, timeout_us)` | Same, also recording each reply's `type_id`. — `crumbs.h` |
| `crumbs_controller_scan_for_crumbs_candidates(ctx, candidates[], count, strict, …, found[], types[], max_found, timeout_us)` | Same probe over an explicit list; duplicates skipped; `-1` when it reaches a candidate above `0x7F` (earlier hits are already in `found[]`). — `crumbs.h` |
| `crumbs_arduino_scan(wire, start, end, strict, found[], max_found)` | Address-only: strict = one-byte read, non-strict = address ACK. Counts past `max_found`. — `crumbs_arduino.h` |
| `crumbs_linux_scan(i2c, start, end, strict, found[], max_found)` | Address-only: strict = one-byte read (driver-owned counts as present), non-strict = SMBus Quick Write. Stops at `max_found`; `-2` if the adapter cannot Quick-Write. — `crumbs_linux.h` |
| `crumbs_linux_scan_for_crumbs(ctx, i2c, start, end, strict, found[], max_found, timeout_us)` | The core read-probe scan wired to the Linux HAL. — `crumbs_linux.h` |
| `crumbs_linux_scan_for_crumbs_with_types(…, found[], types[], max_found, timeout_us)` | With types. — `crumbs_linux.h` |

## Raw I²C devices — `crumbs.h`

For non-CRUMBS devices sharing the bus. Each takes a `crumbs_device_t` and
returns a `CRUMBS_I2C_DEV_*` code.

| Symbol | |
| --- | --- |
| `crumbs_i2c_dev_write(dev, data, len)` | One write; `len == 0` succeeds without touching the bus. |
| `crumbs_i2c_dev_read(dev, data, len, timeout_us)` | One read; short read is an error. |
| `crumbs_i2c_dev_write_then_read(dev, tx, tx_len, rx, rx_len, timeout_us, require_repeated_start, write_read_fn)` | Write then read, through `write_read_fn` when given (repeated start possible), else two transactions. |
| `crumbs_i2c_dev_read_reg_ex(dev, reg, reg_len, out, out_len, …)` / `crumbs_i2c_dev_write_reg_ex(dev, reg, reg_len, data, data_len)` | Register access with an arbitrary-width register address; a write with both parts is staged in a buffer of `CRUMBS_I2C_DEV_MAX_WRITE` (64 unless the library is built with another value). |
| `crumbs_i2c_dev_read_reg_u8` / `crumbs_i2c_dev_write_reg_u8` | 8-bit register address. |
| `crumbs_i2c_dev_read_reg_u16be` / `crumbs_i2c_dev_write_reg_u16be` | 16-bit big-endian register address. |
| `CRUMBS_I2C_DEV_OK`, `CRUMBS_I2C_DEV_E_INVALID`, `CRUMBS_I2C_DEV_E_WRITE`, `CRUMBS_I2C_DEV_E_READ`, `CRUMBS_I2C_DEV_E_SHORT_READ`, `CRUMBS_I2C_DEV_E_NO_REPEATED_START`, `CRUMBS_I2C_DEV_E_SIZE` | `0`, `-1` … `-6` in that order. |

## Transport hooks — `crumbs_i2c.h`

The function-pointer types a HAL implements and the core calls.

| Symbol | |
| --- | --- |
| `crumbs_i2c_write_fn` | `(user_ctx, addr, data, len)` → 0 on success; anything else is passed through by the core. |
| `crumbs_i2c_read_fn` | `(user_ctx, addr, buffer, len, timeout_us)` → bytes read, negative on error. `crumbs_controller_read` passes `timeout_us = 0`; the scanners and `crumbs_i2c_dev_read` forward the caller's value. |
| `crumbs_i2c_write_read_fn` | `(user_ctx, addr, tx, tx_len, rx, rx_len, timeout_us, require_repeated_start)` → bytes read, negative on error; `crumbs_i2c_dev_write_then_read` reports any negative as `CRUMBS_I2C_DEV_E_READ` except `-5`, which it passes through. |
| `crumbs_i2c_scan_fn` | `(user_ctx, start, end, strict, found, max_found)`: the shape of the HAL address scanners. |
| `crumbs_delay_fn` | `(us)`: blocking delay. |
| `crumbs_platform_millis_fn` | `(void)` → milliseconds. |
| `CRUMBS_ELAPSED_MS(start, now)` / `CRUMBS_TIMEOUT_EXPIRED(start, now, timeout_ms)` | Wrap-safe millisecond arithmetic for callers polling with a `millis` function. |

## Arduino HAL — `crumbs_arduino.h`

Uses the global `Wire`; one context per sketch. On AVR the clock is set to
`CRUMBS_DEFAULT_TWI_FREQ` (100 kHz) at init.

| Symbol | |
| --- | --- |
| `crumbs_arduino_init_controller(ctx)` | `crumbs_init` + `Wire.begin()`. No callbacks are attached. |
| `crumbs_arduino_init_peripheral(ctx, address)` | `crumbs_init` + `Wire.begin(address)` + receive/request callbacks that call the two peripheral functions above, from the TWI interrupt on AVR. |
| `crumbs_arduino_wire_write(wire_or_NULL, addr, data, len)` | A `crumbs_i2c_write_fn`. `0`, the positive `Wire.endTransmission()` code, `-1` `NULL` data, `-2` short write. |
| `crumbs_arduino_read(wire_or_NULL, addr, buffer, len, timeout_us)` | A `crumbs_i2c_read_fn`: `requestFrom` then drain; `timeout_us` is a poll deadline, `0` takes what is buffered. `-1` bad args, else bytes read. |
| `crumbs_arduino_write_then_read(wire_or_NULL, addr, tx, tx_len, rx, rx_len, timeout_us, require_repeated_start)` | A `crumbs_i2c_write_read_fn`; repeated start via `endTransmission(false)`. `-1` bad args, `-2` write failed, `-6` longer than the `Wire` buffer, else bytes read. |
| `crumbs_arduino_millis()` / `crumbs_arduino_delay_us(us)` | `millis()` / `delayMicroseconds()`. |

## Linux HAL — `crumbs_linux.h`

Controller only, over linux-wire ≥ 0.1.3 and i2c-dev. `timeout_us` values are
stored on the bus handle and not enforced.

| Symbol | |
| --- | --- |
| `crumbs_linux_i2c_t` | Wraps one `lw_i2c_bus`. |
| `crumbs_linux_init_controller(ctx, i2c, device_path, timeout_us)` | `crumbs_init` + open `/dev/i2c-N`. `-1` bad args, `-2` open failed (`errno` set). |
| `crumbs_linux_close(i2c)` | Close the bus. |
| `crumbs_linux_i2c_write(i2c, addr, data, len)` | A `crumbs_i2c_write_fn`. `-1` bad args or closed, `-2` address select failed, `-3` write failed, `-4` short write. |
| `crumbs_linux_read(i2c, addr, buffer, len, timeout_us)` | A `crumbs_i2c_read_fn`. `-1` bad args or closed, `-2` address select failed, `-3` read failed, else bytes read. |
| `crumbs_linux_write_then_read(i2c, addr, tx, tx_len, rx, rx_len, timeout_us, require_repeated_start)` | A `crumbs_i2c_write_read_fn`; repeated start via one `I2C_RDWR` ioctl. Same codes as the two above. |
| `crumbs_linux_read_message(i2c, addr, ctx, *out_msg)` | Deprecated: read + trim + decode in one call. Use `crumbs_controller_read`. |
| `crumbs_linux_millis()` / `crumbs_linux_delay_us(us)` | `CLOCK_MONOTONIC` / `nanosleep`. |

## Statistics, debug, version

| Symbol | |
| --- | --- |
| `crumbs_get_crc_error_count(ctx)` | Frames rejected for CRC since init or reset. — `crumbs.h` |
| `crumbs_last_crc_ok(ctx)` | 1 if the last decode through this context passed, 0 after any decode failure or after init. — `crumbs.h` |
| `crumbs_reset_crc_stats(ctx)` | Count to 0, `last_crc_ok` to 1. — `crumbs.h` |
| `CRUMBS_DBG(fmt, ...)` | Core trace; a no-op unless `CRUMBS_DEBUG` is defined and routed through `CRUMBS_DEBUG_PRINT`. — `crumbs.h` |
| `CRUMBS_VERSION_MAJOR`, `CRUMBS_VERSION_MINOR`, `CRUMBS_VERSION_PATCH`, `CRUMBS_VERSION_STRING` | Library version components. — `crumbs_version.h` |
| `CRUMBS_VERSION` | `major × 10000 + minor × 100 + patch`, for `#if` comparisons and the version reply. — `crumbs_version.h` |

## Return codes

| Range | Meaning |
| --- | --- |
| `0` | Success; scanners return a count ≥ 0, and a HAL read returns its byte count, so `0` there means nothing was read. |
| `-1` … `-6` | Function-specific: `-1` bad argument / wrong role / structural decode failure; `-2` CRC (codec), wrong role (`send`), address select (Linux); `-3` encode failed (`send`), read/write failed (Linux), read callback failed (helpers); `-4` short read/write; `-5` no repeated start; `-6` too long for a buffer. The tables above give each function's own set. |
| `CRUMBS_RX_REPLY_MISMATCH` (`-7`), `CRUMBS_RX_TYPE_MISMATCH` (`-8`) | Protocol-level, deliberately below every transport code so a getter's caller can tell them apart. New `CRUMBS_RX_*` codes continue downward. |
| positive | From an Arduino write: the `Wire.endTransmission()` code. |
