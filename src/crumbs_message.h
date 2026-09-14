#ifndef CRUMBS_MESSAGE_H
#define CRUMBS_MESSAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file
 * @brief CRUMBS message layout and serialization constants.
 *
 * The wire frame is a variable-length sequence (4-31 bytes):
 *   - type_id   : 1 byte
 *   - opcode    : 1 byte
 *   - data_len  : 1 byte (0-27)
 *   - data[]    : data_len bytes (opaque payload)
 *   - crc8      : 1 byte
 *
 * Maximum frame size is 31 bytes to fit within Arduino Wire's 32-byte buffer.
 */

/** @brief Maximum payload size in bytes (opaque byte array). */
#define CRUMBS_MAX_PAYLOAD 27u

/** @brief Maximum serialized message length in bytes (header + max payload + CRC). */
#define CRUMBS_MESSAGE_MAX_SIZE (3u + CRUMBS_MAX_PAYLOAD + 1u)

    /**
     * @brief Variable-length message structure for CRUMBS communication.
     */
    typedef struct crumbs_message_s
    {
        uint8_t type_id;                  /**< Identifier for the module type */
        uint8_t opcode;                   /**< Command or opcode identifier */
        uint8_t data_len;                 /**< Number of payload bytes (0-27) */
        uint8_t data[CRUMBS_MAX_PAYLOAD]; /**< Opaque payload bytes */
        uint8_t crc8;                     /**< CRC-8 over the header and payload bytes; set by the decoder, ignored by the encoder */
    } crumbs_message_t;

#ifdef __cplusplus
}
#endif

#endif /* CRUMBS_MESSAGE_H */
