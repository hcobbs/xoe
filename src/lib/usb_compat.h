/*
 * usb_compat.h - C89 compatibility wrapper for libusb-1.0
 *
 * This header provides C89 compatibility for libusb-1.0, which uses
 * modern C features that may not be available in strict C89 mode.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_COMPAT_H
#define USB_COMPAT_H

/*
 * C89 compatibility: Define 'inline' as empty if not available
 * This prevents compilation errors with libusb headers that use inline
 */
#if defined(__STDC__) && !defined(__STDC_VERSION__)
    #define inline
#endif

/*
 * Include libusb-1.0 headers
 * Note: libusb-1.0 is a modern library, so we may encounter some
 * compatibility issues that need to be addressed during implementation
 */
#include <libusb-1.0/libusb.h>

/*
 * Restore inline definition for any code that follows
 */
#if defined(__STDC__) && !defined(__STDC_VERSION__)
    #undef inline
#endif

#endif /* USB_COMPAT_H */
