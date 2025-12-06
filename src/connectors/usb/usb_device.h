/*
 * usb_device.h - USB Device Management Interface
 *
 * Defines the interface for USB device lifecycle management including
 * initialization, enumeration, opening, interface claiming, and cleanup.
 *
 * This module handles single device operations. For multi-device management,
 * see usb_device_manager.h.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include "lib/usb_compat.h"
#include "usb_config.h"

/**
 * @brief USB device context structure
 *
 * Maintains all state for a single USB device connection including
 * libusb handles, configuration, and connection status.
 */
typedef struct {
    /* libusb handles */
    struct libusb_context* ctx;         /* libusb context (may be shared) */
    struct libusb_device_handle* handle;/* Device handle */

    /* Configuration */
    usb_config_t config;                /* Device configuration */

    /* Connection state */
    int interface_claimed;              /* TRUE if interface claimed */
    int kernel_driver_detached;         /* TRUE if kernel driver was detached */
    int owns_context;                   /* TRUE if this device owns the context */
} usb_device_t;

/**
 * @brief Initialize libusb library
 *
 * Initializes the libusb-1.0 library and creates a context.
 * This must be called before any other USB operations.
 *
 * @param ctx Pointer to receive the libusb context
 * @return 0 on success, negative error code on failure
 *
 * Thread safety: This function is NOT thread-safe. Call once at startup.
 */
int usb_device_init_library(struct libusb_context** ctx);

/**
 * @brief Cleanup libusb library
 *
 * Releases the libusb context and cleans up library resources.
 * All devices must be closed before calling this function.
 *
 * @param ctx libusb context to cleanup
 *
 * Thread safety: This function is NOT thread-safe. Call once at shutdown.
 */
void usb_device_cleanup_library(struct libusb_context* ctx);

/**
 * @brief Enumerate connected USB devices
 *
 * Lists all USB devices connected to the system with their basic information.
 * Useful for debugging and device discovery.
 *
 * @param ctx libusb context
 * @return 0 on success, negative error code on failure
 *
 * Note: This function prints device information to stdout.
 */
int usb_device_enumerate(struct libusb_context* ctx);

/**
 * @brief Find USB device by VID/PID
 *
 * Searches for a USB device matching the specified vendor and product IDs.
 *
 * @param ctx libusb context
 * @param vid USB Vendor ID (0xVVVV format)
 * @param pid USB Product ID (0xPPPP format)
 * @param device Pointer to receive the device (caller must unref)
 * @return 0 on success, E_USB_NOT_FOUND if not found, other negative on error
 *
 * Note: The caller must call libusb_unref_device() when done with the device.
 */
int usb_device_find(struct libusb_context* ctx,
                    uint16_t vid,
                    uint16_t pid,
                    struct libusb_device** device);

/**
 * @brief Open USB device
 *
 * Opens a USB device for communication and initializes the device context.
 * This function:
 * - Opens the device by VID/PID
 * - Detaches kernel driver if requested
 * - Claims the specified interface
 * - Stores configuration for later use
 *
 * @param dev Device context structure to initialize
 * @param ctx libusb context (if NULL, creates new context)
 * @param config Device configuration
 * @return 0 on success, negative error code on failure
 *
 * Note: If ctx is NULL, the device will own its context and clean it up
 *       in usb_device_close(). Otherwise, the context is shared.
 */
int usb_device_open(usb_device_t* dev,
                    struct libusb_context* ctx,
                    const usb_config_t* config);

/**
 * @brief Close USB device
 *
 * Closes the USB device and releases all resources including:
 * - Releasing claimed interface
 * - Re-attaching kernel driver if it was detached
 * - Closing device handle
 * - Cleaning up context if owned
 *
 * @param dev Device context to cleanup
 * @return 0 on success, negative error code on failure
 *
 * Note: This function is safe to call on partially initialized devices.
 *       It checks state flags before attempting cleanup operations.
 */
int usb_device_close(usb_device_t* dev);

/**
 * @brief Claim USB interface
 *
 * Claims exclusive access to a USB interface. This must be called
 * before performing any I/O operations on the interface.
 *
 * @param dev Device context
 * @return 0 on success, negative error code on failure
 *
 * Note: This is typically called automatically by usb_device_open().
 */
int usb_device_claim_interface(usb_device_t* dev);

/**
 * @brief Release USB interface
 *
 * Releases a previously claimed interface, allowing other applications
 * or drivers to access it.
 *
 * @param dev Device context
 * @return 0 on success, negative error code on failure
 *
 * Note: This is typically called automatically by usb_device_close().
 */
int usb_device_release_interface(usb_device_t* dev);

/**
 * @brief Get endpoint addresses from device
 *
 * Queries the device for available endpoint addresses on the current
 * interface. This is useful for auto-detecting endpoints.
 *
 * @param dev Device context
 * @param endpoints Array to receive endpoint addresses
 * @param max_endpoints Size of endpoints array
 * @return Number of endpoints found, or negative error code
 *
 * Note: Endpoint addresses include the direction bit (bit 7).
 *       IN endpoints have bit 7 set (0x80+), OUT endpoints don't.
 */
int usb_device_get_endpoints(usb_device_t* dev,
                              uint8_t* endpoints,
                              int max_endpoints);

/**
 * @brief Check if device is connected
 *
 * Tests whether the USB device is still connected and responsive.
 *
 * @param dev Device context
 * @return TRUE if connected, FALSE otherwise
 *
 * Note: This performs a lightweight check, not a full device probe.
 */
int usb_device_is_connected(const usb_device_t* dev);

#endif /* USB_DEVICE_H */
