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
 * @code
 * // Arduino
 * #define CRUMBS_DEBUG
 * #define CRUMBS_DEBUG_PRINT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
 *
 * // Linux/stdio
 * #define CRUMBS_DEBUG
 * #define CRUMBS_DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
 * @endcode
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
     * Two tables (SET handlers and reply handlers) of this many entries each
     * live in the context, so it sizes crumbs_context_t: 183 bytes on
     * ATmega328P and 324 on Cortex-M0 at 16, against 21 and 32 at 0. Set it
     * with a build flag, identically for the library and the sketch, and
     * compare crumbs_context_size() with sizeof(crumbs_context_t) at startup.
     * 0 compiles both tables out. Dispatch is a linear search.
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
     * The library sends [type_id][0xFE][0x01][target_opcode][CRC8]; a receiver
     * accepts any data_len >= 1 and uses data[0]. An empty payload leaves
     * requested_opcode unchanged.
     */
#define CRUMBS_CMD_SET_REPLY 0xFE

/**
 * @brief Reserved opcode for the acknowledgement query (GET_ACK).
 *
 * A controller stages it with SET_REPLY like any GET opcode. The library
 * answers it itself, before any reply handler or on_request runs, with a
 * frame of the peripheral's declared type_id, opcode 0xFD and the four
 * bytes [counter][opcode][status][reason] that describe the last command
 * frame (see crumbs_ack_t). A reply handler registered for 0xFD is never
 * called. A SET frame carrying 0xFD is an ordinary command; family headers
 * may not declare it (CRUMBS_DEFINE_FAMILY).
 */
#define CRUMBS_CMD_GET_ACK 0xFD

/** @brief Payload length of the GET_ACK reply; longer replies are an extension. */
#define CRUMBS_ACK_PAYLOAD_LEN 4u

/** @name Last-command status (GET_ACK byte 2)
 *  @{ */
#define CRUMBS_ACK_NONE 0x00     /**< No command frame since crumbs_init(). */
#define CRUMBS_ACK_APPLIED 0x01  /**< Dispatched to on_message or a handler, and not refused. */
#define CRUMBS_ACK_REFUSED 0x02  /**< A handler called crumbs_refuse(); the reason byte is its own. */
#define CRUMBS_ACK_REJECTED 0x03 /**< Decoded, but nothing took it; the reason byte says why. */
/** @} */

/** @name Rejection reasons (GET_ACK byte 3 when the status is CRUMBS_ACK_REJECTED)
 *  @{ */
#define CRUMBS_ACK_REJECT_NO_HANDLER 0x01 /**< No on_message and no handler for the opcode. */
#define CRUMBS_ACK_REJECT_TYPE 0x02       /**< The frame failed the type check (CRUMBS_RX_TYPE_MISMATCH). */
/** @} */

/**
 * @brief The first CRUMBS_VERSION whose peripherals answer GET_ACK.
 *
 * A controller that reads a peripheral's opcode 0x00 version reply
 * (crumbs_build_version_reply()) compares its CRUMBS_VERSION bytes with
 * this. A peripheral built with an older library cannot confirm a SET:
 * send to it with crumbs_controller_send() and treat the result as
 * unconfirmed.
 */
#define CRUMBS_ACK_MIN_VERSION 1600

/**
 * @brief How many times crumbs_controller_read_ack() stages and reads before giving up.
 */
#define CRUMBS_ACK_READ_ATTEMPTS 3

/** @name crumbs_ack_classify() and crumbs_controller_send_confirmed() outcomes
 *  Non-negative: what the peripheral reported. Only CRUMBS_SET_APPLIED (0)
 *  means the command took effect.
 *  @{ */
#define CRUMBS_SET_APPLIED 0      /**< The counter advanced and the command was applied. */
#define CRUMBS_SET_REFUSED 1      /**< The counter advanced and a handler refused the command. */
#define CRUMBS_SET_REJECTED 2     /**< The counter advanced and nothing took the command. */
#define CRUMBS_SET_NOT_ADVANCED 3 /**< The counter did not move: the frame never arrived intact. */
/** @} */

/**
 * @brief Wildcard type_id.
 *
 * A frame whose type_id is 0x00 is never rejected by a peripheral's type
 * check, whatever type the peripheral has declared. The library's own
 * SET_REPLY frames and the scan probe carry this value.
 */
#define CRUMBS_TYPE_ID_ANY 0x00

/*
 * Protocol-level result codes (CRUMBS_RX_*) start at -7, deliberately above
 * every transport- and helper-level code (-1..-6: decode, HAL I/O, and the
 * CRUMBS_I2C_DEV_E_* helpers). Getters pass transport codes through, so the
 * ranges must stay disjoint for a caller to tell the two apart. Add new
 * CRUMBS_RX_* codes downward from -9.
 */

/**
 * @brief crumbs_controller_read_expect() result: valid reply, wrong identity.
 *
 * The reply decoded correctly but its type_id/opcode are not the pair the
 * controller asked for. out_msg holds what was received.
 */
#define CRUMBS_RX_REPLY_MISMATCH (-7)

/**
 * @brief crumbs_peripheral_handle_receive() result: valid frame, wrong type.
 *
 * The frame decoded correctly but its type_id is neither the peripheral's
 * declared type_id nor CRUMBS_TYPE_ID_ANY. Nothing was dispatched.
 */
#define CRUMBS_RX_TYPE_MISMATCH (-8)

/**
 * @brief crumbs_ack_classify() result: the counter advanced on another command.
 *
 * The acknowledgement names an opcode other than the one sent, so some other
 * frame was dispatched after the baseline was read. Whether the command sent
 * was applied cannot be told, so it is not re-sent.
 */
#define CRUMBS_RX_ACK_MISMATCH (-9)

    /**
     * @brief Role of a CRUMBS endpoint on the I2C bus.
     */
    typedef enum
    {
        CRUMBS_ROLE_CONTROLLER = 0, /**< Initiates transactions; sends and reads. */
        CRUMBS_ROLE_PERIPHERAL = 1  /**< Answers to one address; receives and replies. */
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
     * @brief Called when the controller reads a reply from a peripheral.
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
     * @param data Pointer to the payload bytes; never NULL, even when data_len is 0.
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

        /**
         * @brief Declared device type for the peripheral role; 0 = not declared.
         *
         * When non-zero, crumbs_peripheral_handle_receive() rejects frames whose
         * type_id is neither this value nor CRUMBS_TYPE_ID_ANY, before any
         * callback or handler runs. Set with crumbs_set_type_id().
         */
        uint8_t type_id;

        uint32_t crc_error_count; /**< Number of CRC failures seen during decode. */
        uint8_t last_crc_ok;      /**< Non-zero if the last decode had a valid CRC. */

        crumbs_message_cb_t on_message; /**< Called when a message is received (peripheral). */
        crumbs_request_cb_t on_request; /**< Called when the controller reads a reply. */
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

        /** @name Last-command acknowledgement (GET_ACK)
         *  Kept by crumbs_peripheral_handle_receive() and answered by
         *  crumbs_peripheral_build_reply(); see CRUMBS_CMD_GET_ACK.
         *  @{ */
        uint8_t ack_counter;     /**< Command frames counted since init, modulo 256. */
        uint8_t ack_opcode;      /**< Opcode of the last counted frame. */
        uint8_t ack_status;      /**< CRUMBS_ACK_* status of the last counted frame. */
        uint8_t ack_reason;      /**< Refusal or rejection reason; 0 when applied. */
        uint8_t ack_dispatching; /**< Non-zero while on_message or a handler runs. */
        /** @} */

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
     * @brief Bound device handle - groups all transport fields for a single
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
     * @brief A peripheral's acknowledgement, as the GET_ACK reply carries it.
     */
    typedef struct
    {
        uint8_t counter; /**< Command frames the peripheral has counted, modulo 256. */
        uint8_t opcode;  /**< Opcode of the last counted frame. */
        uint8_t status;  /**< CRUMBS_ACK_NONE, _APPLIED, _REFUSED or _REJECTED. */
        uint8_t reason;  /**< Refusal reason (the firmware's) or CRUMBS_ACK_REJECT_*; 0 when applied. */
    } crumbs_ack_t;

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
     * @brief Declare the peripheral's device type.
     *
     * After this call, incoming frames whose type_id is neither @p type_id nor
     * CRUMBS_TYPE_ID_ANY are dropped by crumbs_peripheral_handle_receive()
     * with CRUMBS_RX_TYPE_MISMATCH. Passing 0 clears the declaration and
     * restores the default (every frame is dispatched).
     *
     * @param ctx Active CRUMBS context.
     * @param type_id Device type, or 0 to disable the check.
     */
    void crumbs_set_type_id(crumbs_context_t *ctx, uint8_t type_id);

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
     * @return 0 on success, -1 if ctx is NULL, the table is full, or
     *         CRUMBS_MAX_HANDLERS is 0.
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
     * @return 0 on success (also when nothing was registered), -1 if ctx is
     *         NULL or CRUMBS_MAX_HANDLERS is 0.
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
     * @return 0 on success, -1 if ctx is NULL, the table is full, or
     *         CRUMBS_MAX_HANDLERS is 0.
     */
    int crumbs_register_reply_handler(crumbs_context_t *ctx,
                                      uint8_t opcode,
                                      crumbs_reply_fn fn,
                                      void *user_data);

    /** @} */

    /**
     * @brief Encode a message into the CRUMBS wire frame.
     *
     * The CRC is written to the buffer only; msg->crc8 is not touched.
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
     * @brief Length of the frame declared by a received header.
     *
     * Transports that must request a fixed byte count up front (e.g. Linux
     * i2c-dev) return the requested count with bus padding after the frame,
     * because a peripheral writes only its actual frame and then stops
     * driving the bus. crumbs_decode_message() enforces an exact frame
     * length, so such reads must be trimmed to the header-declared length
     * before decoding. All CRUMBS host read helpers do this internally;
     * call it directly when handing raw reads to the decoder yourself.
     *
     * @param buffer     Bytes returned by the raw read.
     * @param buffer_len Number of bytes the read returned.
     * @param frame_len  Set to the declared frame length (4 + data_len) on success.
     * @return 0 on success; -1 if the arguments are invalid, the read is
     *         shorter than a minimum frame, the declared payload length is
     *         out of range, or the read is shorter than the declared frame.
     */
    int crumbs_frame_length(const uint8_t *buffer,
                            size_t buffer_len,
                            size_t *frame_len);

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
     * @p read_fn, trims them to the header-declared frame length with
     * crumbs_frame_length() (fixed-count transports pad reads past the frame),
     * then decodes them with crumbs_decode_message(). Always passes
     * timeout_us=0 to @p read_fn (the Arduino HAL then returns what is
     * already buffered; the Linux HAL never enforces a timeout). After a
     * SET_REPLY write, wait CRUMBS_DEFAULT_QUERY_DELAY_US before calling
     * this, as the generated getters do.
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
     * @brief Read a reply and check it is the one that was asked for.
     *
     * crumbs_controller_read() followed by an identity check: the reply's
     * opcode must equal @p expect_opcode, and its type_id must equal
     * @p expect_type_id unless that is CRUMBS_TYPE_ID_ANY. A well-formed
     * reply with a valid CRC can still be the wrong frame (another opcode's
     * staged reply, another device type at a duplicated address); the CRC
     * cannot catch that, this does. CRUMBS_DEFINE_GET_OP uses it.
     *
     * @param ctx            Initialized CRUMBS context in controller mode.
     * @param target_addr    7-bit I2C address of the peripheral.
     * @param expect_type_id Expected type_id, or CRUMBS_TYPE_ID_ANY to skip.
     * @param expect_opcode  Expected opcode (always checked; 0x00 is a real opcode).
     * @param out_msg        Output message; filled on success and on mismatch.
     * @param read_fn        I2C read function (crumbs_i2c_read_fn).
     * @param read_ctx       Opaque pointer passed to @p read_fn.
     * @return 0 on success; crumbs_controller_read()'s codes on read/decode
     *         failure; CRUMBS_RX_REPLY_MISMATCH (-7) on an identity mismatch.
 *         CRUMBS_RX_* codes are disjoint from every HAL and helper code, so
 *         the value survives pass-through from a getter.
     */
    int crumbs_controller_read_expect(crumbs_context_t *ctx,
                                      uint8_t target_addr,
                                      uint8_t expect_type_id,
                                      uint8_t expect_opcode,
                                      crumbs_message_t *out_msg,
                                      crumbs_i2c_read_fn read_fn,
                                      void *read_ctx);

    /** @name SET acknowledgement (controller)
     *  Read a peripheral's GET_ACK reply, classify a SET by it, and send a
     *  SET that is confirmed and re-sent when it did not arrive.
     *  @{ */

    /**
     * @brief Parse a GET_ACK reply.
     *
     * For callers with their own transport, which stage CRUMBS_CMD_GET_ACK
     * with SET_REPLY and decode the reply themselves.
     *
     * @param reply A decoded reply frame.
     * @param out   Filled with the first four payload bytes.
     * @return 0 on success; -1 for NULL arguments or a payload shorter than
     *         CRUMBS_ACK_PAYLOAD_LEN (longer is accepted);
     *         CRUMBS_RX_REPLY_MISMATCH if the opcode is not CRUMBS_CMD_GET_ACK.
     */
    int crumbs_ack_decode(const crumbs_message_t *reply, crumbs_ack_t *out);

    /**
     * @brief Classify a SET by the acknowledgements read before and after it.
     *
     * The counter is compared for change only, so it may wrap.
     *
     * @param before Acknowledgement read before the SET was sent.
     * @param after  Acknowledgement read after it.
     * @param opcode Opcode of the SET.
     * @return CRUMBS_SET_NOT_ADVANCED if the counter is unchanged; otherwise
     *         CRUMBS_SET_APPLIED, CRUMBS_SET_REFUSED or CRUMBS_SET_REJECTED
     *         from the status; CRUMBS_RX_ACK_MISMATCH if the last command is
     *         another opcode; -1 for NULL arguments or a status that is none
     *         of the three.
     */
    int crumbs_ack_classify(const crumbs_ack_t *before,
                            const crumbs_ack_t *after,
                            uint8_t opcode);

    /**
     * @brief Read a peripheral's acknowledgement.
     *
     * Stages CRUMBS_CMD_GET_ACK with SET_REPLY (wildcard type), waits
     * CRUMBS_DEFAULT_QUERY_DELAY_US, reads with
     * crumbs_controller_read_expect() and parses the reply. A failed stage
     * or read, a corrupt reply, or another opcode's reply is tried again,
     * staging each time, up to CRUMBS_ACK_READ_ATTEMPTS in all. A GET is
     * safe to repeat; nothing here re-sends a SET.
     *
     * A peripheral built before CRUMBS_ACK_MIN_VERSION answers with another
     * opcode's reply, if it answers at all, so CRUMBS_RX_REPLY_MISMATCH on
     * every attempt means "no acknowledgement". Its version reply is the
     * reliable test.
     *
     * @param dev Device with ctx, write_fn, read_fn and delay_fn bound.
     * @param out Filled on success.
     * @return 0 on success; -1 for NULL arguments or an unbound device;
     *         otherwise the last attempt's send, read or decode code
     *         (CRUMBS_RX_REPLY_MISMATCH when it was another opcode's reply).
     */
    int crumbs_controller_read_ack(const crumbs_device_t *dev, crumbs_ack_t *out);

    /**
     * @brief Send a SET and confirm it from the peripheral's acknowledgement.
     *
     * Reads the acknowledgement (the baseline), sends @p msg, reads it
     * again and classifies the SET with crumbs_ack_classify(). If the
     * counter did not advance the frame did not arrive, and it is sent
     * again, up to @p max_sends sends in all. A write that reports an error
     * is still followed by a read, since the frame may have landed. An
     * acknowledgement that cannot be read after a send is returned as an
     * error, never answered with a re-send, so a frame is not applied twice
     * because a read failed.
     *
     * Use it only with a peripheral that answers GET_ACK (see
     * CRUMBS_ACK_MIN_VERSION); if the baseline cannot be read, nothing is
     * sent.
     *
     * @param dev       Device with ctx, write_fn, read_fn and delay_fn bound.
     * @param msg       The SET. SET_REPLY is refused: it is never counted.
     * @param max_sends Sends allowed in all, at least 1.
     * @param out_ack   If not NULL, the last acknowledgement read.
     * @return CRUMBS_SET_APPLIED (0), CRUMBS_SET_REFUSED, CRUMBS_SET_REJECTED,
     *         or CRUMBS_SET_NOT_ADVANCED after @p max_sends sends;
     *         -1 for NULL arguments, an unbound device, @p max_sends 0 or a
     *         SET_REPLY message; CRUMBS_RX_ACK_MISMATCH if another command
     *         was counted; otherwise crumbs_controller_read_ack()'s code, with
     *         nothing sent if it was the baseline that failed.
     */
    int crumbs_controller_send_confirmed(const crumbs_device_t *dev,
                                         const crumbs_message_t *msg,
                                         uint8_t max_sends,
                                         crumbs_ack_t *out_ack);

    /** @} */

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
     *        The probe frame is 00 00 00 00: a CRUMBS peripheral dispatches it as
     *        an opcode-0x00 SET with no payload (to on_message and any handler
     *        registered for 0x00), and a
     *        24Cxx-style EEPROM takes it as a page write at address 0.
     * @param write_fn Write function for probe writes (may be NULL if strict).
     * @param read_fn Read function to use for reading frames.
     * @param io_ctx Opaque I/O context forwarded to read/write callbacks.
     * @param found Output buffer to receive discovered addresses.
     * @param max_found Capacity of @p found buffer.
     * @param timeout_us Read timeout hint in microseconds.
     * @return Number of discovered devices (0..max_found), or -1 on invalid arguments.
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
     *        The probe frame is 00 00 00 00: a CRUMBS peripheral dispatches it as
     *        an opcode-0x00 SET with no payload (to on_message and any handler
     *        registered for 0x00), and a
     *        24Cxx-style EEPROM takes it as a page write at address 0.
     * @param write_fn Write function for probe writes (may be NULL if strict).
     * @param read_fn Read function to use for reading frames.
     * @param io_ctx Opaque I/O context forwarded to read/write callbacks.
     * @param found Output buffer to receive discovered addresses.
     * @param types Output buffer for type_id from each device (parallel to found).
     *              May be NULL if type IDs are not needed.
     * @param max_found Capacity of @p found (and @p types) buffers.
     * @param timeout_us Read timeout hint in microseconds.
     * @return Number of discovered devices (0..max_found), or -1 on invalid arguments.
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
     *        The probe frame is 00 00 00 00: a CRUMBS peripheral dispatches it as
     *        an opcode-0x00 SET with no payload (to on_message and any handler
     *        registered for 0x00), and a
     *        24Cxx-style EEPROM takes it as a page write at address 0.
     * @param write_fn Write function for probe writes (may be NULL if strict).
     * @param read_fn Read function to use for reading frames.
     * @param io_ctx Opaque I/O context forwarded to read/write callbacks.
     * @param found Output buffer to receive discovered addresses.
     * @param types Output buffer for discovered type_id values (parallel to @p found), may be NULL.
     * @param max_found Capacity of @p found (and @p types when provided).
     * @param timeout_us Read timeout hint in microseconds.
     * @return Number of discovered devices (0..max_found), or -1 on invalid arguments.
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
#define CRUMBS_I2C_DEV_OK 0                  /**< Success. */
#define CRUMBS_I2C_DEV_E_INVALID -1          /**< NULL device or callback, NULL buffer with non-zero length, or nothing to write. */
#define CRUMBS_I2C_DEV_E_WRITE -2            /**< The write callback failed. */
#define CRUMBS_I2C_DEV_E_READ -3             /**< The read callback failed. */
#define CRUMBS_I2C_DEV_E_SHORT_READ -4       /**< The read returned fewer bytes than requested. */
#define CRUMBS_I2C_DEV_E_NO_REPEATED_START -5 /**< A repeated start was required but the device cannot provide one. */
#define CRUMBS_I2C_DEV_E_SIZE -6             /**< Register+data write exceeds CRUMBS_I2C_DEV_MAX_WRITE (64 unless overridden). */
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
     * Every frame that decodes, other than SET_REPLY, is a command frame:
     * it advances ctx->ack_counter and becomes the last command GET_ACK
     * reports, as applied, refused (crumbs_refuse()) or rejected (no
     * on_message and no handler for its opcode, or a failed type check). A
     * frame that does not decode is not counted.
     *
     * @return 0 on success; -1 on invalid arguments or a malformed frame;
     *         -2 on CRC mismatch; CRUMBS_RX_TYPE_MISMATCH (-8) when the frame
     *         is valid but its type_id is neither the declared type nor
     *         CRUMBS_TYPE_ID_ANY (see crumbs_set_type_id()).
     */
    int crumbs_peripheral_handle_receive(crumbs_context_t *ctx,
                                         const uint8_t *buffer,
                                         size_t len);

    /**
     * @brief Mark the command being dispatched as refused.
     *
     * Call it from on_message or a SET handler to report that the command
     * was received and deliberately not acted on (a tripped latch, an
     * argument out of range). GET_ACK then reports CRUMBS_ACK_REFUSED with
     * @p reason, which the firmware defines. A command whose handler only
     * queues work for later is reported as applied when the handler
     * returns; a refusal must be decided in the handler.
     *
     * @param ctx    Active CRUMBS context (peripheral role).
     * @param reason One byte for the controller; its meaning is the family's.
     * @return 0 on success; -1 if @p ctx is NULL or no command is being
     *         dispatched (the call then changes nothing).
     */
    int crumbs_refuse(crumbs_context_t *ctx, uint8_t reason);

    /**
     * @brief Build an encoded reply frame for use inside an I2C request handler.
     *
     * Dispatches in order:
     * 0. CRUMBS_CMD_GET_ACK - answered by the library with the last-command
     *    acknowledgement; nothing below runs.
     * 1. Per-opcode reply handler table (crumbs_register_reply_handler) for
     *    ctx->requested_opcode - preferred for family peripherals.
     * 2. on_request callback - called when no matching reply handler is found
     *    (backward-compatible with existing code).
     * 3. No reply configured - returns success with *out_len set to 0.
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
