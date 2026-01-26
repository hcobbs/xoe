/*
 * usb_device.c - USB Device Management Implementation
 *
 * Implements USB device lifecycle operations including initialization,
 * enumeration, opening, interface management, and cleanup.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_device.h"
#include "lib/common/definitions.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Map libusb error codes to XOE error codes
 */
static int map_libusb_error(int libusb_code)
{
    switch (libusb_code) {
        case LIBUSB_SUCCESS:
            return 0;
        case LIBUSB_ERROR_TIMEOUT:
            return E_USB_TIMEOUT;
        case LIBUSB_ERROR_NO_DEVICE:
            return E_USB_NO_DEVICE;
        case LIBUSB_ERROR_ACCESS:
            return E_USB_ACCESS_DENIED;
        case LIBUSB_ERROR_NO_MEM:
            return E_OUT_OF_MEMORY;
        case LIBUSB_ERROR_INVALID_PARAM:
            return E_INVALID_ARGUMENT;
        case LIBUSB_ERROR_BUSY:
            return E_USB_BUSY;
        case LIBUSB_ERROR_NOT_SUPPORTED:
            return E_USB_NOT_SUPPORTED;
        case LIBUSB_ERROR_PIPE:
            return E_USB_PIPE_ERROR;
        case LIBUSB_ERROR_OVERFLOW:
            return E_USB_OVERFLOW;
        case LIBUSB_ERROR_INTERRUPTED:
            return E_USB_CANCELLED;
        default:
            return E_UNKNOWN_ERROR;
    }
}

/**
 * @brief Initialize libusb library
 */
int usb_device_init_library(struct libusb_context** ctx)
{
    int result;

    if (ctx == NULL) {
        return E_INVALID_ARGUMENT;
    }

    result = libusb_init(ctx);
    if (result != LIBUSB_SUCCESS) {
        fprintf(stderr, "Failed to initialize libusb: %s\n",
                libusb_strerror(result));
        return map_libusb_error(result);
    }

    /* Set debug level (optional, helpful for development) */
#if defined(LIBUSB_LOG_LEVEL_WARNING)
    libusb_set_option(*ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
#endif

    return 0;
}

/**
 * @brief Cleanup libusb library
 */
void usb_device_cleanup_library(struct libusb_context* ctx)
{
    if (ctx != NULL) {
        libusb_exit(ctx);
    }
}

/**
 * @brief Enumerate connected USB devices
 */
int usb_device_enumerate(struct libusb_context* ctx)
{
    struct libusb_device** device_list;
    struct libusb_device_descriptor desc;
    ssize_t count;
    ssize_t i;
    int result;

    if (ctx == NULL) {
        return E_INVALID_ARGUMENT;
    }

    count = libusb_get_device_list(ctx, &device_list);
    if (count < 0) {
        return map_libusb_error((int)count);
    }

    printf("Found %d USB devices:\n", (int)count);
    printf("%-4s %-9s %-4s %-4s %s\n",
           "Bus", "Address", "VID", "PID", "Description");
    printf("-----------------------------------------------------------\n");

    for (i = 0; i < count; i++) {
        result = libusb_get_device_descriptor(device_list[i], &desc);
        if (result == LIBUSB_SUCCESS) {
            printf("%03d  %03d       %04x %04x Class=%02x\n",
                   libusb_get_bus_number(device_list[i]),
                   libusb_get_device_address(device_list[i]),
                   desc.idVendor,
                   desc.idProduct,
                   desc.bDeviceClass);
        }
    }

    libusb_free_device_list(device_list, 1);
    return 0;
}

/**
 * @brief Find USB device by VID/PID
 */
int usb_device_find(struct libusb_context* ctx,
                    uint16_t vid,
                    uint16_t pid,
                    struct libusb_device** device)
{
    struct libusb_device** device_list;
    struct libusb_device_descriptor desc;
    ssize_t count;
    ssize_t i;
    int result;
    int found = 0;

    if (ctx == NULL || device == NULL) {
        return E_INVALID_ARGUMENT;
    }

    *device = NULL;

    count = libusb_get_device_list(ctx, &device_list);
    if (count < 0) {
        return map_libusb_error((int)count);
    }

    for (i = 0; i < count && !found; i++) {
        result = libusb_get_device_descriptor(device_list[i], &desc);
        if (result == LIBUSB_SUCCESS) {
            if (desc.idVendor == vid && desc.idProduct == pid) {
                *device = libusb_ref_device(device_list[i]);
                found = 1;
            }
        }
    }

    libusb_free_device_list(device_list, 1);

    return found ? 0 : E_USB_NOT_FOUND;
}

/**
 * @brief Open USB device
 */
int usb_device_open(usb_device_t* dev,
                    struct libusb_context* ctx,
                    const usb_config_t* config)
{
    int result;
    int kernel_active;

    if (dev == NULL || config == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate configuration */
    result = usb_config_validate(config);
    if (result != 0) {
        return result;
    }

    /* Initialize device structure */
    memset(dev, 0, sizeof(usb_device_t));
    memcpy(&dev->config, config, sizeof(usb_config_t));

    /* Create or use provided context */
    if (ctx == NULL) {
        result = usb_device_init_library(&dev->ctx);
        if (result != 0) {
            return result;
        }
        dev->owns_context = TRUE;
    } else {
        dev->ctx = ctx;
        dev->owns_context = FALSE;
    }

    /* Open device by VID/PID */
    dev->handle = libusb_open_device_with_vid_pid(
        dev->ctx,
        config->vendor_id,
        config->product_id
    );

    if (dev->handle == NULL) {
        fprintf(stderr, "Failed to open USB device %04x:%04x\n",
                config->vendor_id, config->product_id);
        if (dev->owns_context) {
            usb_device_cleanup_library(dev->ctx);
        }
        return E_USB_NOT_FOUND;
    }

    /* Check if kernel driver is active (Linux-specific) */
    kernel_active = libusb_kernel_driver_active(
        dev->handle,
        config->interface_number
    );

    if (kernel_active == 1 && config->detach_kernel_driver) {
        printf("Detaching kernel driver from interface %d...\n",
               config->interface_number);
        result = libusb_detach_kernel_driver(
            dev->handle,
            config->interface_number
        );
        if (result == LIBUSB_SUCCESS) {
            dev->kernel_driver_detached = TRUE;
        } else if (result != LIBUSB_ERROR_NOT_FOUND) {
            fprintf(stderr, "Failed to detach kernel driver: %s\n",
                    libusb_strerror(result));
            /* Continue anyway - might work without detaching */
        }
    }

    /* Claim interface */
    result = usb_device_claim_interface(dev);
    if (result != 0) {
        usb_device_close(dev);
        return result;
    }

    printf("Successfully opened USB device %04x:%04x\n",
           config->vendor_id, config->product_id);

    return 0;
}

/**
 * @brief Close USB device
 */
int usb_device_close(usb_device_t* dev)
{
    if (dev == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Release interface if claimed */
    if (dev->interface_claimed) {
        usb_device_release_interface(dev);
    }

    /* Re-attach kernel driver if we detached it */
    if (dev->kernel_driver_detached && dev->handle != NULL) {
        printf("Re-attaching kernel driver...\n");
        libusb_attach_kernel_driver(dev->handle, dev->config.interface_number);
        dev->kernel_driver_detached = FALSE;
    }

    /* Close device handle */
    if (dev->handle != NULL) {
        libusb_close(dev->handle);
        dev->handle = NULL;
    }

    /* Clean up context if we own it */
    if (dev->owns_context && dev->ctx != NULL) {
        usb_device_cleanup_library(dev->ctx);
        dev->ctx = NULL;
        dev->owns_context = FALSE;
    }

    return 0;
}

/**
 * @brief Claim USB interface
 */
int usb_device_claim_interface(usb_device_t* dev)
{
    int result;

    if (dev == NULL || dev->handle == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (dev->interface_claimed) {
        return 0;  /* Already claimed */
    }

    result = libusb_claim_interface(dev->handle, dev->config.interface_number);
    if (result != LIBUSB_SUCCESS) {
        fprintf(stderr, "Failed to claim interface %d: %s\n",
                dev->config.interface_number,
                libusb_strerror(result));
        return map_libusb_error(result);
    }

    dev->interface_claimed = TRUE;
    return 0;
}

/**
 * @brief Release USB interface
 */
int usb_device_release_interface(usb_device_t* dev)
{
    int result;

    if (dev == NULL || dev->handle == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (!dev->interface_claimed) {
        return 0;  /* Not claimed */
    }

    result = libusb_release_interface(dev->handle, dev->config.interface_number);
    if (result != LIBUSB_SUCCESS) {
        fprintf(stderr, "Failed to release interface: %s\n",
                libusb_strerror(result));
        return map_libusb_error(result);
    }

    dev->interface_claimed = FALSE;
    return 0;
}

/**
 * @brief Get endpoint addresses from device
 */
int usb_device_get_endpoints(usb_device_t* dev,
                              uint8_t* endpoints,
                              int max_endpoints)
{
    const struct libusb_interface_descriptor* iface_desc;
    const struct libusb_endpoint_descriptor* ep_desc;
    struct libusb_config_descriptor* config_desc;
    int result;
    int i;
    int count = 0;

    if (dev == NULL || dev->handle == NULL || endpoints == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Get active configuration descriptor */
    result = libusb_get_active_config_descriptor(
        libusb_get_device(dev->handle),
        &config_desc
    );
    if (result != LIBUSB_SUCCESS) {
        return map_libusb_error(result);
    }

    /* Get interface descriptor with bounds validation (USB-005 fix) */
    if (dev->config.interface_number >= (int)config_desc->bNumInterfaces) {
        libusb_free_config_descriptor(config_desc);
        return E_INVALID_ARGUMENT;
    }

    /* Validate altsetting exists before access (buffer overflow prevention) */
    if (config_desc->interface[dev->config.interface_number].num_altsetting < 1) {
        fprintf(stderr, "Interface %d has no alternate settings\n",
                dev->config.interface_number);
        libusb_free_config_descriptor(config_desc);
        return E_INVALID_ARGUMENT;
    }

    iface_desc = &config_desc->interface[dev->config.interface_number].altsetting[0];

    /* Extract endpoint addresses */
    for (i = 0; i < (int)iface_desc->bNumEndpoints && count < max_endpoints; i++) {
        ep_desc = &iface_desc->endpoint[i];
        endpoints[count++] = ep_desc->bEndpointAddress;
    }

    libusb_free_config_descriptor(config_desc);
    return count;
}

/**
 * @brief Check if device is connected
 */
int usb_device_is_connected(const usb_device_t* dev)
{
    if (dev == NULL || dev->handle == NULL) {
        return FALSE;
    }

    /* Device is considered connected if we have a valid handle */
    /* A more thorough check would attempt a control transfer */
    return TRUE;
}
