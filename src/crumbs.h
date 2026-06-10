#ifndef CRUMBS_H
#define CRUMBS_H

#include <stddef.h>
#include <stdint.h>

#include "crumbs_version.h"
#include "crumbs_message.h"
#include "crumbs_crc.h"
#include "crumbs_i2c.h"

/* ============================================================================
 * Debug Configuration
 * ============================================================================ */

/**
 * @brief Enable CRUMBS debug output.
 *
 * Define CRUMBS_DEBUG before including crumbs.h to enable debug messages.
 * You must also define CRUMBS_DEBUG_PRINT to specify how to output debug.
 *
 * Example for Arduino:
 *   #define CRUMBS_DEBUG
 *   #define CRUMBS_DEBUG_PRINT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
 *
 * Example for Linux/stdio:
 *   #define CRUMBS_DEBUG
 *   #define CRUMBS_DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
 *
 * When CRUMBS_DEBUG is not defined, all debug calls compile to nothing.
 */
#ifdef CRUMBS_DEBUG
#ifndef CRUMBS_DEBUG_PRINT
/* Default: no-op if user didn't define a print function */
#define CRUMBS_DEBUG_PRINT(fmt, ...) ((void)0)
#endif
#define CRUMBS_DBG(fmt, ...) CRUMBS_DEBUG_PRINT("[CRUMBS] " fmt, ##__VA_ARGS__)
#else
#define CRUMBS_DBG(fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /** @file
     * @brief CRUMBS core public API declarations.
     *
     * CRUMBS is a minimal variable-length I2C message protocol. This header
     * declares the public types and functions used by both controller and
     * peripheral roles.
     */

    /**
     * @brief Maximum number of command handlers that can be registered.
     *
     * Define this before including crumbs.h to adjust memory usage.
     * Default is 16 which balances memory use and typical needs.
     *
     * Memory usage: CRUMBS_MAX_HANDLERS * (sizeof(void*) * 2 + 2) bytes
     * - 16 handlers: ~68 bytes on AVR, ~132 bytes on 32-bit
     * - 8 handlers: ~36 bytes on AVR, ~68 bytes on 32-bit
     * - 32 handlers: ~132 bytes on AVR, ~260 bytes on 32-bit
     *
     * Dispatch always uses O(n) linear search for portability.
     * Set to 0 to disable handler dispatch entirely.
     *
     * IMPORTANT: For Arduino/PlatformIO, you must add this to your
     * platformio.ini build_flags to affect the library compilation:
     *   build_flags = -DCRUMBS_MAX_HANDLERS=8
     *
     * Defining it only in your sketch does NOT work because Arduino
     * precompiles the library separately.
     */
#ifndef CRUMBS_MAX_HANDLERS
#define CRUMBS_MAX_HANDLERS 16
#endif

    /**
     * @brief Reserved opcode for SET_REPLY command.
     *
     * When a peripheral receives a message with opcode 0xFE, the library
     * automatically stores the first payload byte in ctx->requested_opcode.
     * This opcode is NOT dispatched to user handlers or on_message callbacks.
     *
     * Wire format: [type_id][0xFE][0x01][target_opcode][CRC8]
     */
#define CRUMBS_CMD_SET_REPLY 0xFE

    /**
     * @brief Role of a CRUMBS endpoint on the I2C bus.
     */
    typedef enum
    {
        CRUMBS_ROLE_CONTROLLER = 0,
        CRUMBS_ROLE_PERIPHERAL = 1
    } crumbs_role_t;

    struct crumbs_context_s;
    /**
     * @typedef crumbs_context_t
     * @brief Opaque handle type for a CRUMBS endpoint context (see struct crumbs_context_s).
     */
    typedef struct crumbs_context_s crumbs_context_t;

    /**
     * @brief Called when a complete, CRC-valid message is received (peripheral).
     *
     * @param ctx Pointer to the active CRUMBS context.
     * @param msg Pointer to the decoded message (valid only for callback duration).
     */
    typedef void (*crumbs_message_cb_t)(
        struct crumbs_context_s *ctx,
        const crumbs_message_t *msg);

    /**
     * @brief Called when the bus master requests a reply from a peripheral.
     *
     * Callback must populate @p msg with the reply message.
     *
     * @param ctx Pointer to the active CRUMBS context.
     * @param msg Pointer to message object to fill with the reply.
     */
    typedef void (*crumbs_request_cb_t)(
        struct crumbs_context_s *ctx,
        crumbs_message_t *msg);

    /**
     * @brief Reply builder function type for per-opcode GET dispatch.
     *
     * Registered with crumbs_register_reply_handler() and called from
     * crumbs_peripheral_build_reply() when ctx->requested_opcode matches.
     * The handler must populate @p reply with the response data.
     *
     * @param ctx       Active CRUMBS context (peripheral role).
     * @param reply     Message to fill with the reply data.
     * @param user_data Opaque pointer registered with the handler.
     */
    typedef void (*crumbs_reply_fn)(
        struct crumbs_context_s *ctx,
        crumbs_message_t *reply,
        void *user_data);

    /**
     * @brief Command handler function type for dispatch-based message handling.
     *
     * Handlers are registered per opcode and called when a message with
     * that command is received. This provides an alternative to the on_message
     * callback for command-specific processing.
     *
     * @param ctx Pointer to the active CRUMBS context.
     * @param opcode The command type that triggered this handler.
     * @param data Pointer to the payload bytes (may be NULL if data_len==0).
     * @param data_len Number of payload bytes (0-27).
     * @param user_data Opaque pointer registered with the handler.
     */
    typedef void (*crumbs_handler_fn)(
        struct crumbs_context_s *ctx,
        uint8_t opcode,
        const uint8_t *data,
        uint8_t data_len,
        void *user_data);

    /**
     * @brief State and configuration for a CRUMBS endpoint.
     *
     * This structure is plain-old-data so it is safe to allocate on the
     * stack or in static storage across supported platforms.
     */
    struct crumbs_context_s
    {
        uint8_t address;    /**< I2C address for peripheral role; 0 for controller. */
        crumbs_role_t role; /**< Controller or peripheral. */

        uint32_t crc_error_count; /**< Number of CRC failures seen during decode. */
        uint8_t last_crc_ok;      /**< Non-zero if the last decode had a valid CRC. */

        crumbs_message_cb_t on_message; /**< Called when a message is received (peripheral). */
        crumbs_request_cb_t on_request; /**< Called when the bus master requests a reply. */
        void *user_data;                /**< Opaque pointer for user code (forwarded to callbacks). */

        /**
         * @brief Target opcode set by SET_REPLY command (0xFE).
         *
         * When a controller sends SET_REPLY with a target opcode, the library
         * stores it here. The user's on_request callback should switch on this
         * value to determine which data to return.
         *
         * Initialized to 0. By convention, opcode 0 means "default reply"
         * (recommended: return device/version info).
         */
        uint8_t requested_opcode;

#if CRUMBS_MAX_HANDLERS > 0
        /** @name Command Handler Dispatch Table
         *  Per-opcode handler functions and associated user data.
         *  Size controlled by CRUMBS_MAX_HANDLERS (default 16).
         *  Uses linear search for dispatch (O(n) but portable/safe).
         *  @{ */
        uint8_t handler_count;                           /**< Number of registered handlers. */
        uint8_t handler_opcode[CRUMBS_MAX_HANDLERS];     /**< Opcode for each handler slot. */
        crumbs_handler_fn handlers[CRUMBS_MAX_HANDLERS]; /**< Handler functions. */
        void *handler_userdata[CRUMBS_MAX_HANDLERS];     /**< User data for each handler. */
                                                         /** @} */

        /** @name Reply Handler Dispatch Table
         *  Per-opcode reply-builder functions for peripheral GET ops.
         *  Dispatched by crumbs_peripheral_build_reply() when ctx->requested_opcode
         *  matches; on_request is called as fallback when no match is found.
         *  Size controlled by CRUMBS_MAX_HANDLERS.
         *  @{ */
        uint8_t reply_handler_count;                              /**< Number of registered reply handlers. */
        uint8_t reply_handler_opcode[CRUMBS_MAX_HANDLERS];        /**< Opcode for each reply handler slot. */
        crumbs_reply_fn reply_handlers[CRUMBS_MAX_HANDLERS];      /**< Reply handler functions. */
        void *reply_handler_userdata[CRUMBS_MAX_HANDLERS];        /**< User data for each reply handler. */
                                                                  /** @} */
#endif                                                   /* CRUMBS_MAX_HANDLERS > 0 */
    };

    /**
     * @brief Bound device handle — groups all transport fields for a single
     *        CRUMBS device on the bus.
     *
     * Pass a pointer to this struct to ops-header functions instead of the
     * individual ctx / addr / write_fn / read_fn / delay_fn / io arguments.
     * Populate once at startup (or after scan) and reuse for every call to
     * that device.
     *
     * @note  read_fn and delay_fn are only required by GET operations (_get_*).
     *        SET-only devices may leave them NULL.
     */
    typedef struct
    {
        crumbs_context_t   *ctx;      /**< Shared controller context. */
        uint8_t             addr;     /**< 7-bit I2C address of this device. */
        crumbs_i2c_write_fn write_fn; /**< I2C write callback. */
        crumbs_i2c_read_fn  read_fn;  /**< I2C read callback (NULL if no GET ops). */
        crumbs_delay_fn     delay_fn; /**< Microsecond delay callback (NULL if no GET ops). */
        void               *io;       /**< Platform I/O context (Wire*, linux handle, etc.). */
    } crumbs_device_t;

    /**
     * @brief Initialize a CRUMBS context.
     *
     * Hardware setup is the responsibility of the platform HAL.
     *
     * @param ctx Context to initialize (must not be NULL).
     * @param role Role for this endpoint.
     * @param address Peripheral I2C address (ignored for controller).
     */
    void crumbs_init(crumbs_context_t *ctx,
                     crumbs_role_t role,
                     uint8_t address);

    /**
     * @brief Install callbacks and user data for a context.
     *
     * Pass NULL for callbacks you do not need.
     */
    void crumbs_set_callbacks(crumbs_context_t *ctx,
                              crumbs_message_cb_t on_message,
                              crumbs_request_cb_t on_request,
                              void *user_data);

    /**
     * @brief Get the size of crumbs_context_t as compiled in the library.
     *
     * This can be used to verify that the user's context size matches the
     * library's expected size. On Arduino, a mismatch occurs when the user
     * defines CRUMBS_MAX_HANDLERS in their sketch instead of build_flags.
     *
     * Usage:
     * @code
     * if (sizeof(crumbs_context_t) != crumbs_context_size()) {
     *     // ERROR: CRUMBS_MAX_HANDLERS mismatch!
     * }
     * @endcode
     *
     * @return Size of crumbs_context_t in bytes as seen by the library.
     */
    size_t crumbs_context_size(void);

    /** @name Command Handler Registration
     *  Register/unregister per-command handlers for dispatch-based processing.
     *  Maximum handlers controlled by CRUMBS_MAX_HANDLERS (default 16).
     *  @{ */

    /**
     * @brief Register a handler for a specific command type.
     *
     * The handler will be invoked when a message with the given opcode
     * is received (after on_message, if configured). Registering a handler
     * for a opcode that already has one will overwrite the previous.
     *
     * @param ctx Context to register the handler on.
     * @param opcode The command type to handle (0-255).
     * @param fn Handler function to call (NULL to unregister).
     * @param user_data Opaque pointer passed to the handler when invoked.
     * @return 0 on success, -1 if ctx is NULL or handler table is full.
     */
    int crumbs_register_handler(crumbs_context_t *ctx,
                                uint8_t opcode,
                                crumbs_handler_fn fn,
                                void *user_data);

    /**
     * @brief Unregister a handler for a specific command type.
     *
     * Equivalent to crumbs_register_handler(ctx, opcode, NULL, NULL).
     *
     * @param ctx Context to unregister from.
     * @param opcode The command type to unregister.
     * @return 0 on success, -1 if ctx is NULL.
     */
    int crumbs_unregister_handler(crumbs_context_t *ctx,
                                  uint8_t opcode);

    /**
     * @brief Register a reply handler for a specific GET opcode.
     *
     * The handler is invoked by crumbs_peripheral_build_reply() when
     * ctx->requested_opcode matches. It provides per-opcode dispatch for GET
     * replies, symmetric with the crumbs_register_handler() pattern for SETs.
     *
     * When both a reply handler and an on_request callback are configured,
     * the reply handler takes priority; on_request is called only when no
     * matching reply handler is found (backward-compatible).
     *
     * @param ctx       Context to register on.
     * @param opcode    GET opcode to handle (0-255).
     * @param fn        Reply-builder function (NULL to unregister).
     * @param user_data Opaque pointer passed to fn.
     * @return 0 on success, -1 if ctx is NULL or handler table is full.
     */
    int crumbs_register_reply_handler(crumbs_context_t *ctx,
                                      uint8_t opcode,
                                      crumbs_reply_fn fn,
                                      void *user_data);

    /** @} */

    /**
     * @brief Encode a message into the CRUMBS wire frame.
     *
     * Note: msg->address is not serialized on the wire.
     *
     * @param msg Pointer to message to encode.
     * @param buffer Destination buffer.
     * @param buffer_len Size of @p buffer in bytes.
     * @return Number of bytes written (4 + data_len) or 0 on error.
     */
    size_t crumbs_encode_message(const crumbs_message_t *msg,
                                 uint8_t *buffer,
                                 size_t buffer_len);

    /**
     * @brief Decode a CRUMBS frame into a message object.
     *
     * Updates CRC-related statistics in @p ctx when provided.
     *
     * @param buffer Input buffer containing a serialized frame.
     * @param buffer_len Length in bytes of @p buffer (must be 4 + data_len).
     * @param msg Output message struct (must not be NULL).
     * @param ctx Optional context updated with CRC stats (may be NULL).
     * @return 0 on success, -1 if frame length/contents are invalid, -2 on CRC mismatch.
     */
    int crumbs_decode_message(const uint8_t *buffer,
                              size_t buffer_len,
                              crumbs_message_t *msg,
                              crumbs_context_t *ctx);

    /**
     * @brief Send a CRUMBS message to a 7-bit I2C target (controller helper).
     *
     * @param ctx Initialized CRUMBS context in controller mode.
     * @param target_addr 7-bit I2C address of the peripheral.
     * @param msg Message to send.
     * @param write_fn I2C write function (crumbs_i2c_write_fn).
     * @param write_ctx Opaque pointer passed to @p write_fn (Wire*, linux handle, etc.).
     * @return 0 on success, non-zero on error.
     */
    int crumbs_controller_send(const crumbs_context_t *ctx,
                               uint8_t target_addr,
                               const crumbs_message_t *msg,
                               crumbs_i2c_write_fn write_fn,
                               void *write_ctx);

    /**
     * @brief Read and decode a CRUMBS reply frame from a peripheral (controller helper).
     *
     * Symmetric counterpart to crumbs_controller_send(). Reads raw bytes via
     * @p read_fn, then decodes them with crumbs_decode_message(). Passes
     * timeout_us=0 to @p read_fn so the HAL uses its own default; call
     * delay_fn() before this to give the peripheral time to stage its reply
     * after a SET_REPLY write.
     *
     * @param ctx         Initialized CRUMBS context in controller mode.
     * @param target_addr 7-bit I2C address of the peripheral.
     * @param out_msg     Output message struct (must not be NULL).
     * @param read_fn     I2C read function (crumbs_i2c_read_fn).
     * @param read_ctx    Opaque pointer passed to @p read_fn.
     * @return 0 on success, -1 on bad args or short read, or crumbs_decode_message error code.
     */
    int crumbs_controller_read(crumbs_context_t *ctx,
                               uint8_t target_addr,
                               crumbs_message_t *out_msg,
                               crumbs_i2c_read_fn read_fn,
                               void *read_ctx);

    /**
     * @brief Probe an I2C address range for CRUMBS-capable devices.
     *
     * The function attempts to read a CRUMBS frame and decode it. In
     * non-strict mode a probe write may be issued to stimulate a reply.
     *
     * This is a convenience wrapper around crumbs_controller_scan_for_crumbs_with_types()
     * that discards type information.
     *
     * @param ctx Controller context (may be NULL for strict mode; required for probe writes).
     * @param start_addr Address range start (inclusive).
     * @param end_addr Address range end (inclusive).
     * @param strict Non-zero for strict read-only; 0 to also try probe writes.
     * @param write_fn Write function for probe writes (may be NULL if strict).
     * @param read_fn Read function to use for reading frames.
     * @param io_ctx Opaque I/O context forwarded to read/write callbacks.
     * @param found Output buffer to receive discovered addresses.
     * @param max_found Capacity of @p found buffer.
     * @param timeout_us Read timeout hint in microseconds.
     * @return Number of discovered devices (>=0) or negative on error.
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
                                          uint32_t timeout_us);

    /**
     * @brief Probe an I2C address range for CRUMBS-capable devices with type IDs.
     *
     * Attempts to read a CRUMBS frame from each address in the range and
     * validates via CRC decode. Returns both addresses and type_id values.
     *
     * In strict mode, only direct reads are attempted. In non-strict mode,
     * if a direct read fails, a probe write is sent first to stimulate devices
     * that only respond after being written to.
     *
     * @param ctx Controller context (may be NULL for strict mode; required for probe writes).
     * @param start_addr Address range start (inclusive).
     * @param end_addr Address range end (inclusive).
     * @param strict Non-zero for strict read-only; 0 to also try probe writes.
     * @param write_fn Write function for probe writes (may be NULL if strict).
     * @param read_fn Read function to use for reading frames.
     * @param io_ctx Opaque I/O context forwarded to read/write callbacks.
     * @param found Output buffer to receive discovered addresses.
     * @param types Output buffer for type_id from each device (parallel to found).
     *              May be NULL if type IDs are not needed.
     * @param max_found Capacity of @p found (and @p types) buffers.
     * @param timeout_us Read timeout hint in microseconds.
     * @return Number of discovered devices (>=0) or negative on error.
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
                                                     uint32_t timeout_us);

    /**
     * @brief Probe an explicit candidate address list for CRUMBS-capable devices.
     *
     * This variant avoids broad range scans and only probes the addresses in
     * @p candidates, which is useful on mixed buses that include non-CRUMBS
     * peripherals.
     *
     * @param ctx Controller context (may be NULL for strict mode; required for probe writes).
     * @param candidates Input list of candidate 7-bit addresses.
     * @param candidate_count Number of entries in @p candidates.
     * @param strict Non-zero for strict read-only; 0 to also try probe writes.
     * @param write_fn Write function for probe writes (may be NULL if strict).
     * @param read_fn Read function to use for reading frames.
     * @param io_ctx Opaque I/O context forwarded to read/write callbacks.
     * @param found Output buffer to receive discovered addresses.
     * @param types Output buffer for discovered type_id values (parallel to @p found), may be NULL.
     * @param max_found Capacity of @p found (and @p types when provided).
     * @param timeout_us Read timeout hint in microseconds.
     * @return Number of discovered devices (>=0) or negative on error.
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
                                                     uint32_t timeout_us);

    /** @name Raw I2C helper error codes
     *  Return codes used by crumbs_i2c_dev_* helpers.
     *  @{ */
#define CRUMBS_I2C_DEV_OK 0
#define CRUMBS_I2C_DEV_E_INVALID -1
#define CRUMBS_I2C_DEV_E_WRITE -2
#define CRUMBS_I2C_DEV_E_READ -3
#define CRUMBS_I2C_DEV_E_SHORT_READ -4
#define CRUMBS_I2C_DEV_E_NO_REPEATED_START -5
#define CRUMBS_I2C_DEV_E_SIZE -6
    /** @} */

    /**
     * @brief Raw I2C write helper bound to a crumbs_device_t transport.
     *
     * @return CRUMBS_I2C_DEV_OK on success or a negative CRUMBS_I2C_DEV_E_* code.
     */
    int crumbs_i2c_dev_write(const crumbs_device_t *dev,
                             const uint8_t *data,
                             size_t len);

    /**
     * @brief Raw I2C read helper bound to a crumbs_device_t transport.
     *
     * @return CRUMBS_I2C_DEV_OK on success or a negative CRUMBS_I2C_DEV_E_* code.
     */
    int crumbs_i2c_dev_read(const crumbs_device_t *dev,
                            uint8_t *data,
                            size_t len,
                            uint32_t timeout_us);

    /**
     * @brief Raw combined write-then-read helper bound to a crumbs_device_t transport.
     *
     * If @p write_read_fn is NULL and @p require_repeated_start is zero, this
     * helper falls back to dev->write_fn + dev->read_fn.
     *
     * @return CRUMBS_I2C_DEV_OK on success or a negative CRUMBS_I2C_DEV_E_* code.
     */
    int crumbs_i2c_dev_write_then_read(const crumbs_device_t *dev,
                                       const uint8_t *tx,
                                       size_t tx_len,
                                       uint8_t *rx,
                                       size_t rx_len,
                                       uint32_t timeout_us,
                                       int require_repeated_start,
                                       crumbs_i2c_write_read_fn write_read_fn);

    /**
     * @brief Read bytes from a register/address sequence of arbitrary length.
     *
     * @return CRUMBS_I2C_DEV_OK on success or a negative CRUMBS_I2C_DEV_E_* code.
     */
    int crumbs_i2c_dev_read_reg_ex(const crumbs_device_t *dev,
                                   const uint8_t *reg,
                                   size_t reg_len,
                                   uint8_t *out,
                                   size_t out_len,
                                   uint32_t timeout_us,
                                   int require_repeated_start,
                                   crumbs_i2c_write_read_fn write_read_fn);

    /**
     * @brief Write bytes to a register/address sequence of arbitrary length.
     *
     * @return CRUMBS_I2C_DEV_OK on success or a negative CRUMBS_I2C_DEV_E_* code.
     */
    int crumbs_i2c_dev_write_reg_ex(const crumbs_device_t *dev,
                                    const uint8_t *reg,
                                    size_t reg_len,
                                    const uint8_t *data,
                                    size_t data_len);

    /**
     * @brief Convenience wrapper for 8-bit register reads.
     */
    int crumbs_i2c_dev_read_reg_u8(const crumbs_device_t *dev,
                                   uint8_t reg,
                                   uint8_t *out,
                                   size_t out_len,
                                   uint32_t timeout_us,
                                   int require_repeated_start,
                                   crumbs_i2c_write_read_fn write_read_fn);

    /**
     * @brief Convenience wrapper for 8-bit register writes.
     */
    int crumbs_i2c_dev_write_reg_u8(const crumbs_device_t *dev,
                                    uint8_t reg,
                                    const uint8_t *data,
                                    size_t data_len);

    /**
     * @brief Convenience wrapper for big-endian 16-bit register reads.
     */
    int crumbs_i2c_dev_read_reg_u16be(const crumbs_device_t *dev,
                                      uint16_t reg,
                                      uint8_t *out,
                                      size_t out_len,
                                      uint32_t timeout_us,
                                      int require_repeated_start,
                                      crumbs_i2c_write_read_fn write_read_fn);

    /**
     * @brief Convenience wrapper for big-endian 16-bit register writes.
     */
    int crumbs_i2c_dev_write_reg_u16be(const crumbs_device_t *dev,
                                       uint16_t reg,
                                       const uint8_t *data,
                                       size_t data_len);

    /**
     * @brief Process raw bytes received by a peripheral HAL.
     *
     * HALs should pass the raw frame bytes into this function so the core can
     * decode them and invoke on_message() when valid.
     *
     * @param ctx Active CRUMBS context (peripheral role).
     * @param buffer Raw bytes received.
     * @param len Number of bytes in @p buffer.
     * @return 0 on success, negative on decode error.
     */
    int crumbs_peripheral_handle_receive(crumbs_context_t *ctx,
                                         const uint8_t *buffer,
                                         size_t len);

    /**
     * @brief Build an encoded reply frame for use inside an I2C request handler.
     *
     * Dispatches in order:
     * 1. Per-opcode reply handler table (crumbs_register_reply_handler) for
     *    ctx->requested_opcode — preferred for family peripherals.
     * 2. on_request callback — called when no matching reply handler is found
     *    (backward-compatible with existing code).
     * 3. No reply configured — returns success with *out_len set to 0.
     *
     * @param ctx Active CRUMBS context (peripheral role).
     * @param out_buf Buffer to receive encoded frame.
     * @param out_buf_len Length of @p out_buf in bytes.
     * @param out_len On success, set to the encoded byte count (may be 0).
     * @return 0 on success, negative on error.
     */
    int crumbs_peripheral_build_reply(crumbs_context_t *ctx,
                                      uint8_t *out_buf,
                                      size_t out_buf_len,
                                      size_t *out_len);

    /** @name CRC statistics helpers
     *  Convenience helpers to access / reset CRC statistics stored in a context.
     *  @{ */
    /**
     * @brief Get the number of CRC failures recorded in @p ctx.
     *
     * @param ctx Context to query (may be NULL).
     * @return Number of CRC failures (0 if ctx is NULL).
     */
    uint32_t crumbs_get_crc_error_count(const crumbs_context_t *ctx);

    /**
     * @brief Query whether the last decoded frame had a valid CRC.
     *
     * @param ctx Context to query (may be NULL).
     * @return 1 if last decode had valid CRC, 0 otherwise (or if ctx==NULL).
     */
    int crumbs_last_crc_ok(const crumbs_context_t *ctx);

    /**
     * @brief Reset CRC statistics tracked in the context.
     *
     * Clears the error count and marks last CRC as OK.
     */
    void crumbs_reset_crc_stats(crumbs_context_t *ctx);
    /** @} */

#ifdef __cplusplus
}
#endif

#endif /* CRUMBS_H */
