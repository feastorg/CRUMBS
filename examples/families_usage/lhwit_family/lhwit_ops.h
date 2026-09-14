/**
 * @file lhwit_ops.h
 * @brief Convenience header for all LHWIT family operation definitions
 *
 * This header includes all operation definitions for the LHWIT
 * (Low Hardware Implementation Test) family, which consists of:
 * - LED Controller (Type 0x01)
 * - Servo Controller (Type 0x02)
 * - Calculator (Type 0x03)
 * - Display (Type 0x04)
 *
 * Include this single header in your controller applications to access
 * all operations for the LHWIT family.
 *
 * Also provides version parsing and compatibility checking helpers
 * for controller-side version verification (opcode 0x00 convention, docs/protocol.md).
 */

#ifndef LHWIT_OPS_H
#define LHWIT_OPS_H

#include "led_ops.h"
#include "servo_ops.h"
#include "calculator_ops.h"
#include "display_ops.h"
#include "crumbs_version.h"

#include <stdio.h> /* for sprintf */

/* ============================================================================
 * Version Parsing (Controller Side)
 * ============================================================================ */

/**
 * @brief Parse version info from opcode 0x00 response payload.
 *
 * Per the opcode 0x00 convention in docs/protocol.md, the reply is 5 bytes:
 *   [CRUMBS_VERSION:u16][module_major:u8][module_minor:u8][module_patch:u8]
 *
 * @param data       Pointer to payload data (reply.data)
 * @param len        Length of payload (reply.data_len)
 * @param crumbs_ver Output: CRUMBS_VERSION (may be NULL)
 * @param mod_major  Output: module major version (may be NULL)
 * @param mod_minor  Output: module minor version (may be NULL)
 * @param mod_patch  Output: module patch version (may be NULL)
 * @return 0 on success, -1 if payload too short
 */
static inline int lhwit_parse_version(const uint8_t *data, uint8_t len,
                                      uint16_t *crumbs_ver,
                                      uint8_t *mod_major,
                                      uint8_t *mod_minor,
                                      uint8_t *mod_patch)
{
    if (len < 5)
        return -1;
    if (crumbs_ver)
        *crumbs_ver = data[0] | ((uint16_t)data[1] << 8);
    if (mod_major)
        *mod_major = data[2];
    if (mod_minor)
        *mod_minor = data[3];
    if (mod_patch)
        *mod_patch = data[4];
    return 0;
}

/**
 * @brief Check if CRUMBS library version is compatible.
 *
 * Requires peripheral CRUMBS_VERSION >= 1000 (v0.10.0, first stable release).
 *
 * @param peripheral_ver Peripheral's CRUMBS_VERSION
 * @return 0 if compatible, -1 if incompatible (too old)
 */
static inline int lhwit_check_crumbs_compat(uint16_t peripheral_ver)
{
    /* Require at least v0.10.0 (version 1000) */
    return (peripheral_ver >= 1000) ? 0 : -1;
}

/**
 * @brief Check if module protocol version is compatible.
 *
 * Compatibility rules:
 *   - MAJOR must match exactly (breaking changes)
 *   - Peripheral MINOR >= Controller MINOR (backward-compatible)
 *   - PATCH is ignored (bugfixes only)
 *
 * @param peri_major   Peripheral's module major version
 * @param peri_minor   Peripheral's module minor version
 * @param expect_major Controller's expected major (from compiled header)
 * @param expect_minor Controller's expected minor (from compiled header)
 * @return 0 if compatible, -1 if major mismatch, -2 if minor too old
 */
static inline int lhwit_check_module_compat(uint8_t peri_major, uint8_t peri_minor,
                                            uint8_t expect_major, uint8_t expect_minor)
{
    if (peri_major != expect_major)
        return -1; /* Major must match */
    if (peri_minor < expect_minor)
        return -2; /* Minor must be >= */
    return 0;
}

/**
 * @brief Format CRUMBS_VERSION as human-readable string (e.g., "0.10.0").
 *
 * @param ver CRUMBS_VERSION value (e.g., 1000 for v0.10.0)
 * @param buf Output buffer (minimum 12 bytes recommended)
 */
static inline void lhwit_format_version(uint16_t ver, char *buf)
{
    uint16_t major = ver / 10000;
    uint16_t minor = (ver / 100) % 100;
    uint16_t patch = ver % 100;
    sprintf(buf, "%u.%u.%u", major, minor, patch);
}

#endif /* LHWIT_OPS_H */
