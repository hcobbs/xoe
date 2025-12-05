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
 * On compilers that provide <stdint.h> (C99+), use the standard types.
 * Otherwise, define compatible types manually for C89 compatibility.
 *
 * This approach avoids type redefinition errors when linking with
 * libraries like libusb-1.0 that include <stdint.h>.
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C99 or later: use standard header */
    #include <stdint.h>
#else
    /* C89 fallback: define types manually with include guards */
    #ifndef _UINT8_T
        typedef unsigned char  uint8_t;
        #define _UINT8_T
    #endif
    #ifndef _UINT16_T
        typedef unsigned short uint16_t;
        #define _UINT16_T
    #endif
    #ifndef _UINT32_T
        typedef unsigned int   uint32_t;
        #define _UINT32_T
    #endif
    #ifndef _INT32_T
        typedef signed int     int32_t;
        #define _INT32_T
    #endif
#endif

#endif /* COMMON_TYPES_H */
