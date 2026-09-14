#ifndef CRUMBS_CRC_H
#define CRUMBS_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @file
     * @brief CRC-8 helper API for CRUMBS.
     */

    /** @typedef crumbs_crc8_t
     *  @brief 8-bit CRC type used by CRUMBS.
     */
    typedef uint8_t crumbs_crc8_t;

    /**
     * @brief Compute CRC-8 over a contiguous buffer.
     *
     * The implementation parameters (polynomial, reflect options, etc.) are
     * supplied by the generated crc8 implementation (pycrc).
     *
     * @param data Pointer to input buffer. NULL or len == 0 yields 0.
     * @param len Number of bytes to process.
     * @return CRC-8 value for provided bytes.
     */
    crumbs_crc8_t crumbs_crc8(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CRUMBS_CRC_H */
