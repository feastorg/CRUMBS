#ifndef CRUMBS_LINUX_H
#define CRUMBS_LINUX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

#include "crumbs.h"     /* crumbs_context_t, crumbs_message_t */
#include "crumbs_i2c.h" /* crumbs_i2c_write_fn */

    /** @file
     * @brief Linux-native I2C helpers (linux-wire) used by CRUMBS.
     */

    /**
     * @brief Linux I2C handle for CRUMBS.
     *
     * @details On native Linux builds this contains a linux-wire lw_i2c_bus. On other
     * platforms we provide a small placeholder so the type can be stack
     * allocated in examples while remaining harmless for Arduino builds.
     */
#if defined(__linux__)
#include <linux_wire.h>
    typedef struct crumbs_linux_i2c_s
    {
        lw_i2c_bus bus;
    } crumbs_linux_i2c_t;
#else
typedef struct crumbs_linux_i2c_s
{
    int _placeholder; /**< Placeholder used on non-Linux builds so the type is instantiable. */
} crumbs_linux_i2c_t;
#endif

    /**
     * @brief Initialize a CRUMBS context as a controller on a Linux I2C bus.
     *
     * @param ctx          Pointer to CRUMBS context (will be initialized).
     * @param i2c          Pointer to Linux I2C handle (will be initialized).
     * @param device_path  Path to I2C device, e.g. "/dev/i2c-1".
     * @param timeout_us   Optional timeout hint in microseconds (0 = no timeout).
     *
     * @return 0 on success.
     *        -1 if arguments are invalid.
     *        -2 if opening the bus failed.
     */
    int crumbs_linux_init_controller(crumbs_context_t *ctx,
                                     crumbs_linux_i2c_t *i2c,
                                     const char *device_path,
                                     uint32_t timeout_us);

    /**
     * @brief Close the underlying Linux I2C bus and clear the handle.
     *
     * Safe to call multiple times on a handle that has been through
     * crumbs_linux_init_controller(). A handle the caller zeroed and never
     * opened has fd 0 (stdin), which this would close.
     *
     * @param i2c Linux I2C handle to close.
     */
    void crumbs_linux_close(crumbs_linux_i2c_t *i2c);

    /**
     * @brief I2C write adapter for CRUMBS on Linux; compatible with crumbs_i2c_write_fn.
     *
     * This uses linux-wire to:
     *   - select the target address with lw_set_target()
     *   - write the frame with lw_write(..., send_stop=1)
     *
     * @param user_ctx     Must be a (crumbs_linux_i2c_t*).
     * @param target_addr  7-bit I2C address.
     * @param data         Frame buffer to send.
     * @param len          Length of frame.
     *
     * @return 0 on success.
     *        -1 invalid args
     *        -2 failed to set slave address
     *        -3 low-level write error
     *        -4 partial write
     */
    int crumbs_linux_i2c_write(void *user_ctx,
                               uint8_t target_addr,
                               const uint8_t *data,
                               size_t len);

    /**
     * @brief Read a CRUMBS reply message from a peripheral.
     *
     * @deprecated Prefer crumbs_controller_read() with crumbs_linux_read() for
     *             portable code:
     *             @code
     *             crumbs_controller_read(ctx, addr, &msg, crumbs_linux_read, (void *)lw);
     *             @endcode
     *             This HAL wrapper predates the platform-agnostic read API and is
     *             retained for compatibility. The one legitimate remaining use is a
     *             raw bus scan before any crumbs_device_t handles are available.
     *
     * This is analogous to Arduino's `Wire.requestFrom()` + `crumbs_decode_message`.
     * It performs:
     *
     *   1. lw_set_target()
     *   2. one or more lw_read() calls until 0 or error, or buffer filled
     *   3. crumbs_decode_message() on the bytes actually read
     *
     * @param i2c          Linux I2C handle (initialized).
     * @param target_addr  7-bit I2C address to read from.
     * @param ctx          CRUMBS context (for CRC stats), may be NULL.
     * @param out_msg      Output message struct.
     *
     * @return 0 on success (message decoded and stored in out_msg).
     *        <0 on error (I2C failure or decode/CRC failure).
     */
    int crumbs_linux_read_message(crumbs_linux_i2c_t *i2c,
                                  uint8_t target_addr,
                                  crumbs_context_t *ctx,
                                  crumbs_message_t *out_msg);

    /**
     * @brief Read up to @p len bytes from the specified address using linux-wire.
     *
     * @param user_ctx   Pointer to crumbs_linux_i2c_t handle.
     * @param addr       7-bit I2C address of the target peripheral.
     * @param buffer     Output buffer to receive data.
     * @param len        Maximum number of bytes to read.
     * @param timeout_us Timeout hint in microseconds (0 = no timeout).
     * @return Number of bytes read (>=0) or negative on error.
     */
    int crumbs_linux_read(void *user_ctx,
                          uint8_t addr,
                          uint8_t *buffer,
                          size_t len,
                          uint32_t timeout_us);

    /**
     * @brief Combined write-then-read helper for Linux backends.
     *
     * For repeated-start-required transactions this uses linux-wire's
     * I2C_RDWR combined transfer path (`lw_ioctl_read`).
     *
     * @param user_ctx                Pointer to crumbs_linux_i2c_t handle.
     * @param addr                    7-bit I2C target address.
     * @param tx                      Write-phase bytes (may be NULL if tx_len==0).
     * @param tx_len                  Number of write-phase bytes.
     * @param rx                      Read buffer (may be NULL if rx_len==0).
     * @param rx_len                  Number of bytes to read.
     * @param timeout_us              Timeout hint in microseconds.
     * @param require_repeated_start  Non-zero requires a combined transaction.
     * @return Number of bytes read (>=0) or negative on error.
     */
    int crumbs_linux_write_then_read(void *user_ctx,
                                     uint8_t addr,
                                     const uint8_t *tx,
                                     size_t tx_len,
                                     uint8_t *rx,
                                     size_t rx_len,
                                     uint32_t timeout_us,
                                     int require_repeated_start);

    /**
     * @brief Scan for I2C devices on the bus for addresses in [start_addr, end_addr].
     *
     * Reports every address that acknowledges; no CRUMBS frame is read or
     * decoded (use crumbs_linux_scan_for_crumbs() for that). Two probes:
     *   - strict (strict != 0): a one-byte read. Present if the target ACKs a
     *     read and returns a byte. Consumes one byte from register-addressed
     *     or stream-style devices.
     *   - non-strict (strict == 0): an address-only SMBus Quick Write via
     *     lw_probe(). Present if the address ACKs. Puts no data on the bus.
     * In both modes an address owned by a kernel driver (I2C_SLAVE refuses
     * with EBUSY, what i2cdetect shows as "UU") is reported as present.
     * Expected failures are not logged during the sweep. Requires
     * linux-wire 0.1.3 or newer.
     *
     * @param user_ctx    Pointer to crumbs_linux_i2c_t handle.
     * @param start_addr  Start of address range (inclusive).
     * @param end_addr    End of address range (inclusive).
     * @param strict      Non-zero = strict read probe, 0 = non-strict probe.
     * @param found       Buffer to receive discovered addresses.
     * @param max_found   Maximum number of entries the buffer can hold.
     * @return Number of found addresses (>=0); -1 on invalid arguments or a
     *         closed bus; -2 if the adapter cannot perform an SMBus Quick
     *         Write (non-strict mode only; errno EOPNOTSUPP) - use strict
     *         mode on that adapter.
     */
    int crumbs_linux_scan(void *user_ctx,
                          uint8_t start_addr,
                          uint8_t end_addr,
                          int strict,
                          uint8_t *found,
                          size_t max_found);

    /**
     * @brief Scan for CRUMBS devices and return their addresses and type IDs.
     *
     * This is a convenience wrapper around crumbs_controller_scan_for_crumbs_with_types()
     * that automatically suppresses expected I/O error messages during the scan.
     * Scanning probes many addresses without devices; this function silences
     * the noise from those expected failures.
     *
     * @param ctx         CRUMBS context (initialized as controller).
     * @param i2c         Linux I2C handle (initialized).
     * @param start_addr  Start of address range (inclusive), typically 0x08.
     * @param end_addr    End of address range (inclusive), typically 0x77.
     * @param strict      0 = non-strict (probe writes), non-zero = strict (read only).
     * @param found       Buffer to receive discovered addresses.
     * @param types       Buffer to receive type IDs (parallel to found), may be NULL.
     * @param max_found   Maximum number of entries the buffers can hold.
     * @param timeout_us  Timeout per device in microseconds.
     * @return Number of devices found (>=0), or negative on error.
     */
    int crumbs_linux_scan_for_crumbs_with_types(crumbs_context_t *ctx,
                                                crumbs_linux_i2c_t *i2c,
                                                uint8_t start_addr,
                                                uint8_t end_addr,
                                                int strict,
                                                uint8_t *found,
                                                uint8_t *types,
                                                size_t max_found,
                                                uint32_t timeout_us);

    /**
     * @brief Scan for CRUMBS devices (addresses only, no type IDs).
     *
     * Convenience wrapper that calls crumbs_linux_scan_for_crumbs_with_types()
     * with types=NULL.
     *
     * @param ctx         CRUMBS context (initialized as controller).
     * @param i2c         Linux I2C handle (initialized).
     * @param start_addr  Start of address range (inclusive).
     * @param end_addr    End of address range (inclusive).
     * @param strict      0 = non-strict, non-zero = strict.
     * @param found       Buffer to receive discovered addresses.
     * @param max_found   Maximum number of entries the buffer can hold.
     * @param timeout_us  Timeout per device in microseconds.
     * @return Number of devices found (>=0), or negative on error.
     */
    int crumbs_linux_scan_for_crumbs(crumbs_context_t *ctx,
                                     crumbs_linux_i2c_t *i2c,
                                     uint8_t start_addr,
                                     uint8_t end_addr,
                                     int strict,
                                     uint8_t *found,
                                     size_t max_found,
                                     uint32_t timeout_us);

    /**
     * @brief Linux platform millisecond timer.
     * @return Milliseconds since boot.
     */
    uint32_t crumbs_linux_millis(void);

    /**
     * @brief Linux platform microsecond delay (conforms to crumbs_delay_fn).
     *
     * Wraps POSIX usleep(). On non-Linux builds this is a no-op stub.
     *
     * @param us Microseconds to delay.
     */
    void crumbs_linux_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* CRUMBS_LINUX_H */
