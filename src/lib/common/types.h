/**
 * types.h
 *
 * Centralized type definitions for the XOE project.
 * Provides C89-compatible fixed-width integer types.
 *
 * This header must be included by any file that needs uint16_t or uint32_t
 * to avoid duplicate type definition errors.
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

/**
 * C99 fixed-width integer types via <stdint.h>
 *
 * C99 provides native support for uint8_t, uint16_t, uint32_t, uint64_t
 * and other fixed-width integer types through the standard header.
 *
 * This avoids type redefinition errors when linking with libraries
 * like libusb-1.0 that also include <stdint.h>.
 */
#include <stdint.h>

#endif /* COMMON_TYPES_H */
