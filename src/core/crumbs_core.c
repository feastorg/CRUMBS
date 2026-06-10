/**
 * @file
 * @brief Core CRUMBS implementation (encode/decode and controller/peripheral helpers).
 */

#include "crumbs.h"

#include <string.h> /* memcpy, memset */

/* ---- Internal constants (file-local) ----------------------------------- */

/** @brief Minimum frame size: type_id + opcode + data_len + crc8 = 4 bytes. */
static const size_t k_min_frame_len = 4u;

/** @brief Header size: type_id + opcode + data_len = 3 bytes. */
static const size_t k_header_len = 3u;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(CRUMBS_MESSAGE_MAX_SIZE == 31u, "CRUMBS_MESSAGE_MAX_SIZE must be 31");
_Static_assert(CRUMBS_MAX_PAYLOAD == 27u, "CRUMBS_MAX_PAYLOAD must be 27");
#endif

/* ---- Public API implementation ---------------------------------------- */

/**
 * @brief Initialize a CRUMBS context structure.
 *
 * This function initializes only the core (fixed-size) fields of the context.
 * Handler arrays are NOT touched to ensure safety when the caller's
 * CRUMBS_MAX_HANDLERS differs from the library's compiled value.
 *
 * For handler dispatch to work correctly, callers should either:
 * - Use static/global context (automatically zero-initialized), or
 * - Explicitly zero-initialize: `crumbs_context_t ctx = {0};`
 *
 * This is particularly important for Arduino/PlatformIO where the library
 * is precompiled with the default CRUMBS_MAX_HANDLERS but user code may
 * define a different value.
 */
void crumbs_init(crumbs_context_t *ctx,
                 crumbs_role_t role,
                 uint8_t address)
{
    if (!ctx)
    {
        return;
    }

    /*
     * Initialize only fixed-size fields. DO NOT use memset(ctx, 0, sizeof(*ctx))
     * because sizeof(*ctx) uses the LIBRARY's CRUMBS_MAX_HANDLERS, which
     * may differ from the CALLER's value if they defined it differently.
     * Writing beyond the caller's struct size causes stack/heap corruption.
     */
    ctx->address = (role == CRUMBS_ROLE_PERIPHERAL) ? address : 0u;
    ctx->role = role;
    ctx->crc_error_count = 0u;
    ctx->last_crc_ok = 0u;
    ctx->on_message = NULL;
    ctx->on_request = NULL;
    ctx->user_data = NULL;
    ctx->requested_opcode = 0u; /* Default: opcode 0 (device info by convention) */

    /*
     * Handler arrays are left untouched. If the context is in static storage,
     * they're already zero. If on stack, caller must have zero-initialized.
     * We reset handler_count to ensure no stale handlers are dispatched.
     */
#if CRUMBS_MAX_HANDLERS > 0
    ctx->handler_count = 0u;
    ctx->reply_handler_count = 0u;
#endif
}

/**
 * @brief Get the size of crumbs_context_t as compiled in the library.
 *
 * This allows runtime detection of CRUMBS_MAX_HANDLERS mismatches between
 * the library and user code (common on Arduino when user defines in sketch
 * instead of build_flags).
 */
size_t crumbs_context_size(void)
{
    return sizeof(crumbs_context_t);
}

/**
 * @brief Install callbacks and user data on an existing context.
 */
void crumbs_set_callbacks(crumbs_context_t *ctx,
                          crumbs_message_cb_t on_message,
                          crumbs_request_cb_t on_request,
                          void *user_data)
{
    if (!ctx)
    {
        return;
    }

    ctx->on_message = on_message;
    ctx->on_request = on_request;
    ctx->user_data = user_data;
}

/**
 * @brief Register a handler for a specific command type.
 *
 * Uses linear search for slot management (O(n) but safe and portable).
 * When CRUMBS_MAX_HANDLERS == 0, handlers are disabled.
 */
int crumbs_register_handler(crumbs_context_t *ctx,
                            uint8_t opcode,
                            crumbs_handler_fn fn,
                            void *user_data)
{
#if CRUMBS_MAX_HANDLERS == 0
    /* Handlers disabled */
    (void)ctx;
    (void)opcode;
    (void)fn;
    (void)user_data;
    return -1;
#else
    /* Linear search table */
    if (!ctx)
    {
        return -1;
    }

    /* Check if command already registered (overwrite) */
    for (uint8_t i = 0; i < ctx->handler_count; i++)
    {
        if (ctx->handler_opcode[i] == opcode)
        {
            if (fn == NULL)
            {
                /* Unregister: remove slot by swapping with last */
                ctx->handler_count--;
                if (i < ctx->handler_count)
                {
                    ctx->handler_opcode[i] = ctx->handler_opcode[ctx->handler_count];
                    ctx->handlers[i] = ctx->handlers[ctx->handler_count];
                    ctx->handler_userdata[i] = ctx->handler_userdata[ctx->handler_count];
                }
            }
            else
            {
                /* Overwrite existing */
                ctx->handlers[i] = fn;
                ctx->handler_userdata[i] = user_data;
            }
            return 0;
        }
    }

    /* Not found - add new handler if fn is non-NULL */
    if (fn == NULL)
    {
        /* Nothing to unregister */
        return 0;
    }

    if (ctx->handler_count >= CRUMBS_MAX_HANDLERS)
    {
        /* Table full */
        return -1;
    }

    uint8_t slot = ctx->handler_count++;
    ctx->handler_opcode[slot] = opcode;
    ctx->handlers[slot] = fn;
    ctx->handler_userdata[slot] = user_data;
    return 0;
#endif
}

/**
 * @brief Unregister a handler for a specific command type.
 */
int crumbs_unregister_handler(crumbs_context_t *ctx,
                              uint8_t opcode)
{
    return crumbs_register_handler(ctx, opcode, NULL, NULL);
}

/**
 * @brief Register a reply handler for a specific GET opcode.
 *
 * Uses identical slot management logic to crumbs_register_handler.
 */
int crumbs_register_reply_handler(crumbs_context_t *ctx,
                                  uint8_t opcode,
                                  crumbs_reply_fn fn,
                                  void *user_data)
{
#if CRUMBS_MAX_HANDLERS == 0
    /* Handlers disabled */
    (void)ctx;
    (void)opcode;
    (void)fn;
    (void)user_data;
    return -1;
#else
    if (!ctx)
    {
        return -1;
    }

    /* Check if opcode already registered (overwrite or unregister). */
    for (uint8_t i = 0; i < ctx->reply_handler_count; i++)
    {
        if (ctx->reply_handler_opcode[i] == opcode)
        {
            if (fn == NULL)
            {
                /* Unregister: remove slot by swapping with last */
                ctx->reply_handler_count--;
                if (i < ctx->reply_handler_count)
                {
                    ctx->reply_handler_opcode[i]   = ctx->reply_handler_opcode[ctx->reply_handler_count];
                    ctx->reply_handlers[i]         = ctx->reply_handlers[ctx->reply_handler_count];
                    ctx->reply_handler_userdata[i] = ctx->reply_handler_userdata[ctx->reply_handler_count];
                }
            }
            else
            {
                /* Overwrite existing */
                ctx->reply_handlers[i]         = fn;
                ctx->reply_handler_userdata[i] = user_data;
            }
            return 0;
        }
    }

    /* Not found - add new handler if fn is non-NULL */
    if (fn == NULL)
    {
        /* Nothing to unregister */
        return 0;
    }

    if (ctx->reply_handler_count >= CRUMBS_MAX_HANDLERS)
    {
        /* Table full */
        return -1;
    }

    uint8_t slot = ctx->reply_handler_count++;
    ctx->reply_handler_opcode[slot]   = opcode;
    ctx->reply_handlers[slot]         = fn;
    ctx->reply_handler_userdata[slot] = user_data;
    return 0;
#endif
}

/**
 * @brief Serialize a crumbs_message_t into a flat byte buffer.
 *
 * Wire format: [type_id, opcode, data_len, data[0..data_len-1], crc8]
 * Returns the encoded length (4 + data_len) on success, or 0 on error.
 */
size_t crumbs_encode_message(const crumbs_message_t *msg,
                             uint8_t *buffer,
                             size_t buffer_len)
{
    if (!msg || !buffer)
    {
        return 0u;
    }

    /* Validate data_len is within bounds. */
    if (msg->data_len > CRUMBS_MAX_PAYLOAD)
    {
        return 0u;
    }

    /* Compute required frame size: header (3) + data_len + crc (1). */
    size_t frame_len = k_header_len + msg->data_len + 1u;
    if (buffer_len < frame_len)
    {
        return 0u;
    }

    size_t index = 0u;

    buffer[index++] = msg->type_id;
    buffer[index++] = msg->opcode;
    buffer[index++] = msg->data_len;

    /* Copy payload bytes. */
    if (msg->data_len > 0u)
    {
        memcpy(&buffer[index], msg->data, msg->data_len);
        index += msg->data_len;
    }

    /* Compute CRC over header + payload (bytes 0 through index-1). */
    crumbs_crc8_t crc = crumbs_crc8(buffer, index);
    buffer[index++] = crc;

    return index; /* 4 + data_len */
}

/**
 * @brief Decode a serialized frame into a crumbs_message_t.
 *
 * Validates frame structure and CRC. Updates CRC state in @p ctx if provided.
 */
int crumbs_decode_message(const uint8_t *buffer,
                          size_t buffer_len,
                          crumbs_message_t *msg,
                          crumbs_context_t *ctx)
{
    if (!buffer || !msg)
    {
        CRUMBS_DBG("decode: NULL buffer or msg\n");
        return -1;
    }

    /* Minimum frame: type_id + opcode + data_len + crc8 = 4 bytes. */
    if (buffer_len < k_min_frame_len)
    {
        CRUMBS_DBG("decode: too short (%u < %u)\n",
                   (unsigned)buffer_len, (unsigned)k_min_frame_len);
        if (ctx)
        {
            ctx->last_crc_ok = 0u;
        }
        return -1; /* too small */
    }

    /* Extract data_len from header. */
    uint8_t data_len = buffer[2];
    if (data_len > CRUMBS_MAX_PAYLOAD)
    {
        CRUMBS_DBG("decode: data_len %u > max %u\n",
                   data_len, CRUMBS_MAX_PAYLOAD);
        if (ctx)
        {
            ctx->last_crc_ok = 0u;
        }
        return -1; /* invalid data_len */
    }

    /* Expected frame length: header (3) + data_len + crc (1). */
    size_t expected_len = k_header_len + data_len + 1u;
    if (buffer_len < expected_len)
    {
        CRUMBS_DBG("decode: truncated (%u < %u)\n",
                   (unsigned)buffer_len, (unsigned)expected_len);
        if (ctx)
        {
            ctx->last_crc_ok = 0u;
        }
        return -1; /* truncated frame */
    }
    if (buffer_len > expected_len)
    {
        CRUMBS_DBG("decode: trailing bytes (%u > %u)\n",
                   (unsigned)buffer_len, (unsigned)expected_len);
        if (ctx)
        {
            ctx->last_crc_ok = 0u;
        }
        return -1; /* extra bytes after complete frame */
    }

    /* CRC is computed over header + payload (bytes 0 through header+data_len-1). */
    size_t crc_span = k_header_len + data_len;
    const crumbs_crc8_t computed = crumbs_crc8(buffer, crc_span);
    const crumbs_crc8_t received = buffer[crc_span];

    if (computed != received)
    {
        CRUMBS_DBG("decode: CRC mismatch (got 0x%02X, expected 0x%02X)\n",
                   received, computed);
        if (ctx)
        {
            ctx->last_crc_ok = 0u;
            ctx->crc_error_count++;
        }
        return -2; /* CRC mismatch */
    }

    /* Populate message fields. address is not transmitted. */
    msg->type_id = buffer[0];
    msg->opcode = buffer[1];
    msg->data_len = data_len;

    if (data_len > 0u)
    {
        memcpy(msg->data, &buffer[k_header_len], data_len);
    }

    msg->crc8 = received;

    CRUMBS_DBG("decode: OK type=0x%02X cmd=0x%02X len=%u\n",
               msg->type_id, msg->opcode, msg->data_len);

    if (ctx)
    {
        ctx->last_crc_ok = 1u;
    }

    return 0;
}

/**
 * @brief Helper used by controllers to send a CRUMBS frame.
 */
int crumbs_controller_send(const crumbs_context_t *ctx,
                           uint8_t target_addr,
                           const crumbs_message_t *msg,
                           crumbs_i2c_write_fn write_fn,
                           void *write_ctx)
{
    if (!ctx || !msg || !write_fn)
    {
        CRUMBS_DBG("tx: invalid ctx/msg/write_fn\n");
        return -1;
    }

    if (ctx->role != CRUMBS_ROLE_CONTROLLER)
    {
        CRUMBS_DBG("tx: not controller role\n");
        return -2;
    }

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t written = crumbs_encode_message(msg, frame, sizeof(frame));
    if (written == 0u)
    {
        CRUMBS_DBG("tx: encode failed\n");
        return -3;
    }

    CRUMBS_DBG("tx: addr=0x%02X %u bytes type=0x%02X cmd=0x%02X\n",
               target_addr, (unsigned)written, msg->type_id, msg->opcode);

    int rc = write_fn(write_ctx, target_addr, frame, written);
    if (rc != 0)
    {
        CRUMBS_DBG("tx: write failed (%d)\n", rc);
    }
    return rc;
}

/**
 * @brief Helper used by controllers to read and decode a CRUMBS frame.
 */
int crumbs_controller_read(crumbs_context_t *ctx,
                           uint8_t target_addr,
                           crumbs_message_t *out_msg,
                           crumbs_i2c_read_fn read_fn,
                           void *read_ctx)
{
    if (!ctx || !out_msg || !read_fn)
    {
        CRUMBS_DBG("rx: invalid ctx/out_msg/read_fn\n");
        return -1;
    }

    if (ctx->role != CRUMBS_ROLE_CONTROLLER)
    {
        CRUMBS_DBG("rx: not controller role\n");
        return -1;
    }

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    int n = read_fn(read_ctx, target_addr, buf, sizeof(buf), 0u);
    if (n < 4)
    {
        CRUMBS_DBG("rx: short read (%d bytes)\n", n);
        return -1;
    }

    CRUMBS_DBG("rx: addr=0x%02X %d bytes\n", target_addr, n);

    return crumbs_decode_message(buf, (size_t)n, out_msg, ctx);
}

/**
 * @brief Peripheral-side handler for raw bytes received by a HAL.
 */
int crumbs_peripheral_handle_receive(crumbs_context_t *ctx,
                                     const uint8_t *buffer,
                                     size_t len)
{
    if (!ctx || ctx->role != CRUMBS_ROLE_PERIPHERAL || !buffer)
    {
        CRUMBS_DBG("rx: invalid ctx/role/buffer\n");
        return -1;
    }

    CRUMBS_DBG("rx: %u bytes [", (unsigned)len);
#ifdef CRUMBS_DEBUG
    for (size_t i = 0; i < len && i < 8; i++)
    {
        CRUMBS_DEBUG_PRINT("%02X ", buffer[i]);
    }
    if (len > 8)
    {
        CRUMBS_DEBUG_PRINT("...");
    }
    CRUMBS_DEBUG_PRINT("]\n");
#endif

    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));

    int rc = crumbs_decode_message(buffer, len, &msg, ctx);
    if (rc != 0)
    {
        CRUMBS_DBG("rx: decode failed (%d)\n", rc);
        return rc;
    }

    /*
     * Intercept SET_REPLY (0xFE) before user callbacks.
     * Store the target opcode and return without dispatching to user.
     */
    if (msg.opcode == CRUMBS_CMD_SET_REPLY)
    {
        if (msg.data_len >= 1)
        {
            ctx->requested_opcode = msg.data[0];
            CRUMBS_DBG("rx: SET_REPLY target=0x%02X\n", ctx->requested_opcode);
        }
        else
        {
            CRUMBS_DBG("rx: SET_REPLY with no payload, ignoring\n");
        }
        return 0; /* Do not dispatch to user handlers */
    }

    /* Invoke general on_message callback if set. */
    if (ctx->on_message)
    {
        CRUMBS_DBG("rx: calling on_message\n");
        ctx->on_message(ctx, &msg);
    }

#if CRUMBS_MAX_HANDLERS > 0
    /* Dispatch to per-command handler if registered (linear search). */
    uint8_t found = 0;
    for (uint8_t i = 0; i < ctx->handler_count; i++)
    {
        if (ctx->handler_opcode[i] == msg.opcode)
        {
            crumbs_handler_fn handler = ctx->handlers[i];
            if (handler)
            {
                CRUMBS_DBG("rx: dispatch cmd 0x%02X (slot %u)\n", msg.opcode, i);
                handler(ctx,
                        msg.opcode,
                        msg.data,
                        msg.data_len,
                        ctx->handler_userdata[i]);
            }
            found = 1;
            break;
        }
    }
    if (!found)
    {
        CRUMBS_DBG("rx: no handler for cmd 0x%02X (searched %u slots)\n",
                   msg.opcode, ctx->handler_count);
    }
#endif /* CRUMBS_MAX_HANDLERS > 0 */

    return 0;
}

/**
 * @brief Build an encoded reply using reply handlers or the on_request callback.
 *
 * Dispatch order:
 *   1. Per-opcode reply handler table (crumbs_register_reply_handler) for
 *      ctx->requested_opcode.
 *   2. on_request callback as fallback (backward-compatible).
 *   3. No reply configured — returns 0 with *out_len = 0.
 */
int crumbs_peripheral_build_reply(crumbs_context_t *ctx,
                                  uint8_t *out_buf,
                                  size_t out_buf_len,
                                  size_t *out_len)
{
    if (out_len)
    {
        *out_len = 0u;
    }

    if (!ctx || ctx->role != CRUMBS_ROLE_PERIPHERAL || !out_buf)
    {
        CRUMBS_DBG("reply: invalid ctx/role/buffer\n");
        return -1;
    }

    crumbs_message_t msg;
    memset(&msg, 0, sizeof(msg));

    int dispatched = 0;

#if CRUMBS_MAX_HANDLERS > 0
    /* Check per-opcode reply handler table first. */
    for (uint8_t i = 0; i < ctx->reply_handler_count; i++)
    {
        if (ctx->reply_handler_opcode[i] == ctx->requested_opcode)
        {
            crumbs_reply_fn handler = ctx->reply_handlers[i];
            if (handler)
            {
                CRUMBS_DBG("reply: dispatch opcode 0x%02X via reply handler (slot %u)\n",
                           ctx->requested_opcode, i);
                handler(ctx, &msg, ctx->reply_handler_userdata[i]);
                dispatched = 1;
            }
            break;
        }
    }
#endif /* CRUMBS_MAX_HANDLERS > 0 */

    if (!dispatched)
    {
        if (!ctx->on_request)
        {
            CRUMBS_DBG("reply: no reply handler or on_request callback\n");
            return 0;
        }

        CRUMBS_DBG("reply: calling on_request\n");
        ctx->on_request(ctx, &msg);
    }

    size_t written = crumbs_encode_message(&msg, out_buf, out_buf_len);
    if (written == 0u)
    {
        CRUMBS_DBG("reply: encode failed\n");
        return -2;
    }

    CRUMBS_DBG("reply: %u bytes type=0x%02X cmd=0x%02X\n",
               (unsigned)written, msg.type_id, msg.opcode);

    if (out_len)
    {
        *out_len = written;
    }
    return 0;
}

/**
 * @brief Probe an address range looking for CRUMBS-capable devices with type IDs.
 *
 * This is the core scan implementation. It attempts to read a CRUMBS frame
 * from each address and validates it via CRC. In non-strict mode, a probe
 * write is sent first to stimulate replies from devices that only respond
 * after being written to.
 *
 * @param types Optional output buffer for type_id from each device (may be NULL).
 */
int crumbs_controller_scan_for_crumbs_with_types(const crumbs_context_t *ctx,
                                                 uint8_t start_addr,
                                                 uint8_t end_addr,
                                                 int strict,
                                                 crumbs_i2c_write_fn write_fn,
                                                 crumbs_i2c_read_fn read_fn,
                                                 void *io_ctx,
                                                 uint8_t *found,
                                                 uint8_t *types,
                                                 size_t max_found,
                                                 uint32_t timeout_us)
{
    if (!read_fn || !found || max_found == 0u)
        return -1; /* invalid args */

    if (start_addr > end_addr)
        return -1;

    uint8_t buf[CRUMBS_MESSAGE_MAX_SIZE];
    size_t count = 0u;

    for (int addr = start_addr; addr <= end_addr; ++addr)
    {
        /* Attempt a direct read first (many peripherals reply on request).
           read_fn returns number of bytes read on success, negative on error. */
        int n = read_fn(io_ctx, (uint8_t)addr, buf, (size_t)CRUMBS_MESSAGE_MAX_SIZE, timeout_us);
        if (n >= (int)k_min_frame_len)
        {
            crumbs_message_t m;
            if (crumbs_decode_message(buf, (size_t)n, &m, NULL) == 0)
            {
                if (count < max_found)
                {
                    found[count] = (uint8_t)addr;
                    if (types)
                        types[count] = m.type_id;
                }
                ++count;
                if (count >= max_found)
                    break;
                continue; /* next address */
            }
        }

        /* In non-strict mode we try to stimulate a reply by sending a small
           CRUMBS frame then attempting to read again. This helps with
           peripherals that only respond after being written to.
           Note: ctx is required for probe writes; if NULL, skip probing. */
        if (!strict && write_fn && ctx)
        {
            crumbs_message_t probe;
            memset(&probe, 0, sizeof(probe));

            /* Send a probe message; ignore any error from send. */
            (void)crumbs_controller_send(ctx, (uint8_t)addr, &probe, write_fn, io_ctx);

            int n2 = read_fn(io_ctx, (uint8_t)addr, buf, (size_t)CRUMBS_MESSAGE_MAX_SIZE, timeout_us);
            if (n2 >= (int)k_min_frame_len)
            {
                crumbs_message_t m2;
                if (crumbs_decode_message(buf, (size_t)n2, &m2, NULL) == 0)
                {
                    if (count < max_found)
                    {
                        found[count] = (uint8_t)addr;
                        if (types)
                            types[count] = m2.type_id;
                    }
                    ++count;
                    if (count >= max_found)
                        break;
                }
            }
        }
    }

    return (int)count;
}

/**
 * @brief Probe an address range looking for CRUMBS-capable devices.
 *
 * This is a convenience wrapper around crumbs_controller_scan_for_crumbs_with_types()
 * that discards the type_id information. Use the _with_types variant if you need
 * both addresses and type information.
 */
int crumbs_controller_scan_for_crumbs(const crumbs_context_t *ctx,
                                      uint8_t start_addr,
                                      uint8_t end_addr,
                                      int strict,
                                      crumbs_i2c_write_fn write_fn,
                                      crumbs_i2c_read_fn read_fn,
                                      void *io_ctx,
                                      uint8_t *found,
                                      size_t max_found,
                                      uint32_t timeout_us)
{
    return crumbs_controller_scan_for_crumbs_with_types(ctx, start_addr, end_addr,
                                                        strict, write_fn, read_fn,
                                                        io_ctx, found, NULL,
                                                        max_found, timeout_us);
}

/**
 * @brief Probe an explicit candidate address list for CRUMBS-capable devices.
 *
 * This helper is intended for mixed-bus setups where broad range scans are
 * undesirable. Duplicate candidate addresses are ignored.
 */
int crumbs_controller_scan_for_crumbs_candidates(const crumbs_context_t *ctx,
                                                 const uint8_t *candidates,
                                                 size_t candidate_count,
                                                 int strict,
                                                 crumbs_i2c_write_fn write_fn,
                                                 crumbs_i2c_read_fn read_fn,
                                                 void *io_ctx,
                                                 uint8_t *found,
                                                 uint8_t *types,
                                                 size_t max_found,
                                                 uint32_t timeout_us)
{
    if (!candidates || candidate_count == 0u || !read_fn || !found || max_found == 0u)
    {
        return -1;
    }

    uint8_t seen[128];
    memset(seen, 0, sizeof(seen));

    size_t total = 0u;
    for (size_t i = 0; i < candidate_count && total < max_found; ++i)
    {
        uint8_t addr = candidates[i];
        if (addr > 0x7Fu)
        {
            return -1;
        }

        if (seen[addr] != 0u)
        {
            continue;
        }
        seen[addr] = 1u;

        int n = crumbs_controller_scan_for_crumbs_with_types(
            ctx, addr, addr, strict, write_fn, read_fn, io_ctx,
            &found[total], types ? &types[total] : NULL, max_found - total, timeout_us);
        if (n < 0)
        {
            return n;
        }

        total += (size_t)n;
    }

    return (int)total;
}

/* ---- CRC stats helpers ------------------------------------------------- */

/**
 * @brief Get the number of CRC errors observed by the context.
 *
 * @param ctx Context to query (may be NULL).
 * @return Count of CRC failures recorded by @p ctx (0 if ctx is NULL).
 */
uint32_t crumbs_get_crc_error_count(const crumbs_context_t *ctx)
{
    return ctx ? ctx->crc_error_count : 0u;
}

/**
 * @brief Return 1 if the last decoded frame had a valid CRC.
 *
 * @param ctx Context to query (may be NULL).
 * @return 1 if the last decode was CRC-ok, 0 otherwise.
 */
int crumbs_last_crc_ok(const crumbs_context_t *ctx)
{
    return (ctx && ctx->last_crc_ok) ? 1 : 0;
}

/**
 * @brief Reset CRC statistics for @p ctx.
 *
 * This clears the error count and sets last_crc_ok to true.
 */
void crumbs_reset_crc_stats(crumbs_context_t *ctx)
{
    if (!ctx)
    {
        return;
    }
    ctx->crc_error_count = 0u;
    ctx->last_crc_ok = 1u;
}
