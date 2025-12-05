/*
 * usb_config.h - USB Connector Configuration Structures
 *
 * Defines configuration structures and constants for USB device
 * connections over XOE network protocol.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_CONFIG_H
#define USB_CONFIG_H

#include "lib/common/types.h"

/* Maximum path/string lengths */
#define USB_DEVICE_DESC_MAX     256
#define USB_MAX_ENDPOINTS       16
#define USB_MAX_DEVICES         8   /* Max simultaneous devices */

/* Endpoint sentinel value */
#define USB_NO_ENDPOINT         0xFF

/* Default values */
#define USB_DEFAULT_TIMEOUT_MS      5000
#define USB_DEFAULT_MAX_PACKET      512
#define USB_DEFAULT_INTERFACE       0
#define USB_DEFAULT_CONFIGURATION   0

/**
 * @brief USB device configuration structure
 *
 * This structure contains all configuration parameters for connecting
 * to and communicating with a USB device over the network.
 *
 * The configuration supports both automatic endpoint detection and
 * explicit endpoint specification for different device classes.
 */
typedef struct {
    /* Device identification */
    uint16_t vendor_id;         /* USB Vendor ID (0 = any) */
    uint16_t product_id;        /* USB Product ID (0 = any) */
    int      interface_number;  /* Interface to claim */
    int      configuration;     /* Configuration value (0 = active) */

    /* Endpoint configuration */
    uint8_t  bulk_in_endpoint;      /* Bulk IN endpoint (0x81, etc.) */
    uint8_t  bulk_out_endpoint;     /* Bulk OUT endpoint (0x01, etc.) */
    uint8_t  interrupt_in_endpoint; /* Interrupt IN (USB_NO_ENDPOINT if none) */

    /* Transfer parameters */
    int      transfer_timeout_ms;   /* Default timeout */
    int      max_packet_size;       /* Maximum packet size */

    /* Flags */
    int      detach_kernel_driver;  /* Auto-detach kernel driver */
    int      enable_hotplug;        /* Enable hotplug detection */
} usb_config_t;

/**
 * @brief Multi-device configuration structure
 *
 * This structure holds configuration for multiple USB devices
 * that will be managed by a single client session.
 */
typedef struct {
    usb_config_t* devices;      /* Array of device configurations */
    int           device_count; /* Number of devices */
    int           max_devices;  /* Allocated array size */
} usb_multi_config_t;

/**
 * @brief Initialize USB configuration with default values
 *
 * Sets all configuration fields to sensible defaults. The caller
 * must then set device-specific values (VID/PID, endpoints, etc.).
 *
 * @param config Pointer to configuration structure to initialize
 */
void usb_config_init_defaults(usb_config_t* config);

/**
 * @brief Validate USB configuration
 *
 * Checks that the configuration has all required fields set correctly
 * and that values are within valid ranges.
 *
 * @param config Pointer to configuration structure to validate
 * @return 0 if valid, negative error code otherwise
 *
 * Validation checks:
 * - VID and PID are set (non-zero)
 * - Interface number is >= 0
 * - At least one endpoint is configured
 * - Timeout and packet size are reasonable
 */
int usb_config_validate(const usb_config_t* config);

/**
 * @brief Initialize multi-device configuration
 *
 * Allocates and initializes a multi-device configuration structure
 * with space for the specified maximum number of devices.
 *
 * @param max_devices Maximum number of devices to support
 * @return Pointer to allocated structure, or NULL on failure
 */
usb_multi_config_t* usb_multi_config_init(int max_devices);

/**
 * @brief Add device to multi-device configuration
 *
 * Adds a device configuration to the multi-device configuration.
 * The device configuration is copied into the array.
 *
 * @param multi_config Pointer to multi-device configuration
 * @param device_config Pointer to device configuration to add
 * @return 0 on success, negative error code on failure
 */
int usb_multi_config_add_device(usb_multi_config_t* multi_config,
                                 const usb_config_t* device_config);

/**
 * @brief Free multi-device configuration
 *
 * Frees all memory associated with a multi-device configuration.
 *
 * @param multi_config Pointer to multi-device configuration to free
 */
void usb_multi_config_free(usb_multi_config_t* multi_config);

#endif /* USB_CONFIG_H */
