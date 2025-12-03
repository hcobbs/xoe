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
 * C89-compatible fixed-width integer types
 *
 * Note: C89 does not have stdint.h, so we define these manually.
 * These definitions are guaranteed to work on all supported platforms
 * (Linux, macOS, Windows) with their standard compilers.
 */
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

#endif /* COMMON_TYPES_H */
