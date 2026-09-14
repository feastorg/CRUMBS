#ifndef CRUMBS_ARDUINO_H
#define CRUMBS_ARDUINO_H

#include <stdint.h>
#include <stddef.h>

#include "crumbs.h"

/**
 * @file
 * @brief Arduino HAL adapter declarations for CRUMBS.
 */

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize a CRUMBS context for use as an I2C controller on Arduino.
     *
     * This uses the default global Wire instance and calls Wire.begin().
     * You can still call crumbs_set_callbacks() afterwards if you want to
     * attach any controller-side metadata or debug hooks.
     *
     * @param ctx Pointer to the CRUMBS context to initialize.
     */
    void crumbs_arduino_init_controller(crumbs_context_t *ctx);

    /**
     * @brief Initialize a CRUMBS context for use as an I2C peripheral on Arduino.
     *
     * This:
     *   - calls crumbs_init(ctx, CRUMBS_ROLE_PERIPHERAL, address);
     *   - calls Wire.begin(address);
     *   - registers internal Wire.onReceive/onRequest handlers that forward
     *     into the CRUMBS C core.
     *
     * You must call crumbs_set_callbacks() to install your on_message/on_request
     * callbacks before or after this, as long as it is done before traffic arrives.
     *
     * @param ctx     Pointer to the CRUMBS context to initialize.
     * @param address I2C peripheral address (7-bit).
     */
    void crumbs_arduino_init_peripheral(crumbs_context_t *ctx, uint8_t address);

    /**
     * @brief Arduino implementation of crumbs_i2c_write_fn using Wire.
     *
     * This is intended to be passed to crumbs_controller_send().
     *
     * @param user_ctx Pointer to TwoWire instance or NULL to use &Wire.
     * @param addr     7-bit I2C address of the target peripheral.
     * @param data     Pointer to data buffer to transmit.
     * @param len      Number of bytes to transmit.
     * @return 0 on success, non-zero on error.
     */
    int crumbs_arduino_wire_write(void *user_ctx,
                                  uint8_t addr,
                                  const uint8_t *data,
                                  size_t len);

    /**
     * @brief Probe an address range for any I2C device (not CRUMBS-specific).
     *
     * Reports every address that acknowledges. No frame is read or decoded;
     * for CRUMBS-aware discovery use crumbs_controller_scan_for_crumbs()
     * with crumbs_arduino_read as the read function.
     *
     * The two probes put the same transactions on the wire as
     * crumbs_linux_scan() (linux-wire 0.1.3+):
     * - strict != 0: one-byte read. Present if the target ACKs a read and
     *   clocks a byte out. Writes nothing, but consumes one byte from
     *   register-addressed or stream-style devices.
     * - strict == 0: address + write bit, then STOP, no data phase (Linux
     *   does this as an SMBus Quick Write). Present if the address ACKs.
     *   Writes nothing.
     * Two differences remain: Linux also reports an address a kernel driver
     * owns as present (there is no such notion here), and this function
     * keeps counting past @p max_found while Linux stops there.
     *
     * @param user_ctx     Pointer to TwoWire instance or NULL to use &Wire
     * @param start_addr   Start address (inclusive) to probe, typically 0x03
     * @param end_addr     End address (inclusive) to probe, typically 0x77
     * @param strict       Non-zero for the read probe, 0 for the address-only probe
     * @param found        Output buffer to receive found addresses
     * @param max_found    Capacity of @p found buffer
     * @return number of addresses found (>=0) or negative on error
     */
    int crumbs_arduino_scan(void *user_ctx,
                            uint8_t start_addr,
                            uint8_t end_addr,
                            int strict,
                            uint8_t *found,
                            size_t max_found);

    /**
     * @brief Read bytes from an I2C peripheral using TwoWire.
     *
     * @param user_ctx   Pointer to TwoWire instance or NULL to use &Wire.
     * @param addr       7-bit I2C address of the target peripheral.
     * @param buffer     Output buffer to receive data.
     * @param len        Maximum number of bytes to read.
     * @param timeout_us Timeout hint in microseconds.
     * @return Number of bytes read (>=0) or negative on error.
     */
    int crumbs_arduino_read(void *user_ctx,
                            uint8_t addr,
                            uint8_t *buffer,
                            size_t len,
                            uint32_t timeout_us);

    /**
     * @brief Combined write-then-read helper using Arduino TwoWire.
     *
     * When @p require_repeated_start is non-zero this performs:
     *   endTransmission(false) + requestFrom(...)
     * for an atomic register-style transaction.
     *
     * @param user_ctx                Pointer to TwoWire instance or NULL to use &Wire.
     * @param addr                    7-bit I2C target address.
     * @param tx                      Write-phase bytes (may be NULL if tx_len==0).
     * @param tx_len                  Number of write-phase bytes.
     * @param rx                      Read buffer (may be NULL if rx_len==0).
     * @param rx_len                  Number of bytes to read.
     * @param timeout_us              Timeout hint in microseconds.
     * @param require_repeated_start  Non-zero requires combined no-STOP behavior.
     * @return Number of bytes read (>=0) or negative on error.
     */
    int crumbs_arduino_write_then_read(void *user_ctx,
                                       uint8_t addr,
                                       const uint8_t *tx,
                                       size_t tx_len,
                                       uint8_t *rx,
                                       size_t rx_len,
                                       uint32_t timeout_us,
                                       int require_repeated_start);

    /**
     * @brief Arduino platform millisecond timer.
     * @return Milliseconds since boot.
     */
    uint32_t crumbs_arduino_millis(void);

    /**
     * @brief Arduino platform microsecond delay (conforms to crumbs_delay_fn).
     *
     * Wraps Arduino's delayMicroseconds(). Safe to call with us=0.
     *
     * @param us Microseconds to delay.
     */
    static inline void crumbs_arduino_delay_us(uint32_t us)
    {
        if (us > 0)
        {
            delayMicroseconds((unsigned long)us);
        }
    }

#ifdef __cplusplus
}
#endif

#endif /* CRUMBS_ARDUINO_H */
