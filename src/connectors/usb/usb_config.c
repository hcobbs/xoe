/*
 * usb_config.c - USB Configuration Implementation
 *
 * Implements USB configuration initialization, validation, and
 * multi-device management functions.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_config.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initialize USB configuration with default values
 *
 * Sets all fields to reasonable defaults. The caller must set
 * device-specific values before use.
 */
void usb_config_init_defaults(usb_config_t* config)
{
    if (config == NULL) {
        return;
    }

    /* Initialize all fields to zero/defaults */
    memset(config, 0, sizeof(usb_config_t));

    /* Set specific default values */
    config->vendor_id = 0;  /* Must be set by caller */
    config->product_id = 0; /* Must be set by caller */
    config->interface_number = USB_DEFAULT_INTERFACE;
    config->configuration = USB_DEFAULT_CONFIGURATION;

    /* Mark endpoints as unconfigured */
    config->bulk_in_endpoint = USB_NO_ENDPOINT;
    config->bulk_out_endpoint = USB_NO_ENDPOINT;
    config->interrupt_in_endpoint = USB_NO_ENDPOINT;

    /* Set transfer parameters */
    config->transfer_timeout_ms = USB_DEFAULT_TIMEOUT_MS;
    config->max_packet_size = USB_DEFAULT_MAX_PACKET;

    /* Set flags */
    config->detach_kernel_driver = TRUE;  /* Auto-detach by default */
    config->enable_hotplug = FALSE;       /* Disabled by default */
}

/**
 * @brief Validate USB configuration
 *
 * Verifies that all required fields are set and values are valid.
 */
int usb_config_validate(const usb_config_t* config)
{
    if (config == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* VID and PID must be set */
    if (config->vendor_id == 0 || config->product_id == 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Interface number must be non-negative */
    if (config->interface_number < 0) {
        return E_INVALID_ARGUMENT;
    }

    /* At least one endpoint must be configured */
    if (config->bulk_in_endpoint == USB_NO_ENDPOINT &&
        config->bulk_out_endpoint == USB_NO_ENDPOINT &&
        config->interrupt_in_endpoint == USB_NO_ENDPOINT) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate endpoint directions (USB spec: bit 7 set = IN, clear = OUT) */
    if (config->bulk_in_endpoint != USB_NO_ENDPOINT &&
        (config->bulk_in_endpoint & 0x80) == 0) {
        return E_INVALID_ARGUMENT;  /* IN endpoint must have bit 7 set */
    }

    if (config->bulk_out_endpoint != USB_NO_ENDPOINT &&
        (config->bulk_out_endpoint & 0x80) != 0) {
        return E_INVALID_ARGUMENT;  /* OUT endpoint must NOT have bit 7 set */
    }

    if (config->interrupt_in_endpoint != USB_NO_ENDPOINT &&
        (config->interrupt_in_endpoint & 0x80) == 0) {
        return E_INVALID_ARGUMENT;  /* IN endpoint must have bit 7 set */
    }

    /* Timeout must be positive */
    if (config->transfer_timeout_ms <= 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Packet size must be reasonable */
    if (config->max_packet_size <= 0 || config->max_packet_size > 65536) {
        return E_INVALID_ARGUMENT;
    }

    return 0;
}

/**
 * @brief Initialize multi-device configuration
 *
 * Allocates and initializes a multi-device configuration structure.
 */
usb_multi_config_t* usb_multi_config_init(int max_devices)
{
    usb_multi_config_t* multi_config;

    /* Validate max_devices */
    if (max_devices <= 0 || max_devices > USB_MAX_DEVICES) {
        return NULL;
    }

    /* Allocate multi-config structure */
    multi_config = (usb_multi_config_t*)malloc(sizeof(usb_multi_config_t));
    if (multi_config == NULL) {
        return NULL;
    }

    /* Allocate device array */
    multi_config->devices = (usb_config_t*)malloc(
        sizeof(usb_config_t) * max_devices
    );
    if (multi_config->devices == NULL) {
        free(multi_config);
        return NULL;
    }

    /* Initialize fields */
    multi_config->device_count = 0;
    multi_config->max_devices = max_devices;

    /* Initialize all device configs to defaults */
    {
        int i;
        for (i = 0; i < max_devices; i++) {
            usb_config_init_defaults(&multi_config->devices[i]);
        }
    }

    return multi_config;
}

/**
 * @brief Add device to multi-device configuration
 *
 * Copies the device configuration into the multi-device array.
 */
int usb_multi_config_add_device(usb_multi_config_t* multi_config,
                                 const usb_config_t* device_config)
{
    if (multi_config == NULL || device_config == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Check if array is full */
    if (multi_config->device_count >= multi_config->max_devices) {
        return E_BUFFER_TOO_SMALL;
    }

    /* Validate device configuration */
    {
        int result = usb_config_validate(device_config);
        if (result != 0) {
            return result;
        }
    }

    /* Copy device configuration */
    memcpy(&multi_config->devices[multi_config->device_count],
           device_config,
           sizeof(usb_config_t));

    multi_config->device_count++;

    return 0;
}

/**
 * @brief Free multi-device configuration
 *
 * Releases all memory associated with the multi-device configuration.
 */
void usb_multi_config_free(usb_multi_config_t* multi_config)
{
    if (multi_config == NULL) {
        return;
    }

    /* Free device array */
    if (multi_config->devices != NULL) {
        free(multi_config->devices);
    }

    /* Free structure */
    free(multi_config);
}
