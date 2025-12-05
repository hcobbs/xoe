/*
 * usb_transfer.h - USB Transfer Operations
 *
 * Provides abstraction layer for USB transfers (Control, Bulk, Interrupt).
 * Supports both synchronous and asynchronous transfer modes.
 *
 * EXPERIMENTAL FEATURE - Subject to change
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_TRANSFER_H
#define USB_TRANSFER_H

#include "lib/usb_compat.h"
#include "usb_device.h"
#include "usb_protocol.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/**
 * @brief USB transfer context for asynchronous operations
 *
 * Maintains state for a single USB transfer including completion
 * synchronization, URB header for protocol encapsulation, and
 * user-provided callback data.
 */
typedef struct {
    /* libusb transfer object */
    struct libusb_transfer* transfer;

    /* URB header for network protocol */
    usb_urb_header_t urb_header;

    /* Callback context */
    void* user_data;

    /* Completion state */
    int completed;              /* Transfer completed flag */
    int result;                 /* Transfer result code */

    /* Thread synchronization */
#ifdef _WIN32
    CRITICAL_SECTION lock;      /* Windows critical section */
    HANDLE event;               /* Windows event object */
#else
    pthread_mutex_t lock;       /* POSIX mutex */
    pthread_cond_t cond;        /* POSIX condition variable */
#endif
} usb_transfer_ctx_t;

/* ========================================================================
 * Synchronous Transfer Functions
 * ======================================================================== */

/**
 * @brief Perform synchronous USB control transfer
 *
 * Executes a control transfer on endpoint 0 with the specified parameters.
 * This is a blocking call that waits for transfer completion or timeout.
 *
 * @param dev Device context
 * @param bmRequestType Request type and direction (USB spec)
 * @param bRequest Request code (USB spec)
 * @param wValue Request-specific value parameter
 * @param wIndex Request-specific index parameter (usually interface/endpoint)
 * @param data Buffer for data phase (IN or OUT)
 * @param wLength Length of data buffer
 * @param timeout Timeout in milliseconds
 * @return Number of bytes transferred on success, negative error code on failure
 *
 * Common bmRequestType values:
 * - LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD: Get descriptor
 * - LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_STANDARD: Set configuration
 * - LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS: Get HID report
 */
int usb_transfer_control(usb_device_t* dev,
                         uint8_t bmRequestType,
                         uint8_t bRequest,
                         uint16_t wValue,
                         uint16_t wIndex,
                         unsigned char* data,
                         uint16_t wLength,
                         unsigned int timeout);

/**
 * @brief Perform synchronous bulk read transfer
 *
 * Reads data from a bulk IN endpoint. This is a blocking call.
 *
 * @param dev Device context
 * @param endpoint Bulk IN endpoint address (must have bit 7 set)
 * @param buffer Buffer to receive data
 * @param length Maximum bytes to read
 * @param transferred Pointer to receive actual bytes transferred
 * @param timeout Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 *
 * Note: The endpoint address must have the IN direction bit set (0x80).
 *       Example: 0x81 for bulk IN endpoint 1.
 */
int usb_transfer_bulk_read(usb_device_t* dev,
                           uint8_t endpoint,
                           unsigned char* buffer,
                           int length,
                           int* transferred,
                           unsigned int timeout);

/**
 * @brief Perform synchronous bulk write transfer
 *
 * Writes data to a bulk OUT endpoint. This is a blocking call.
 *
 * @param dev Device context
 * @param endpoint Bulk OUT endpoint address (bit 7 clear)
 * @param buffer Buffer containing data to write
 * @param length Number of bytes to write
 * @param transferred Pointer to receive actual bytes transferred
 * @param timeout Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 *
 * Note: The endpoint address must NOT have the IN direction bit set.
 *       Example: 0x01 for bulk OUT endpoint 1.
 */
int usb_transfer_bulk_write(usb_device_t* dev,
                            uint8_t endpoint,
                            const unsigned char* buffer,
                            int length,
                            int* transferred,
                            unsigned int timeout);

/**
 * @brief Perform synchronous interrupt read transfer
 *
 * Reads data from an interrupt IN endpoint. Used for HID input reports,
 * device notifications, etc. This is a blocking call.
 *
 * @param dev Device context
 * @param endpoint Interrupt IN endpoint address (must have bit 7 set)
 * @param buffer Buffer to receive data
 * @param length Maximum bytes to read
 * @param transferred Pointer to receive actual bytes transferred
 * @param timeout Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 *
 * Note: Interrupt transfers are typically small (8-64 bytes for HID).
 *       The endpoint address must have the IN direction bit set (0x80).
 */
int usb_transfer_interrupt_read(usb_device_t* dev,
                                uint8_t endpoint,
                                unsigned char* buffer,
                                int length,
                                int* transferred,
                                unsigned int timeout);

/* ========================================================================
 * Asynchronous Transfer Functions
 * ======================================================================== */

/**
 * @brief Allocate transfer context for asynchronous operations
 *
 * Creates and initializes a transfer context including synchronization
 * primitives. The context must be freed with usb_transfer_free().
 *
 * @return Transfer context pointer, or NULL on allocation failure
 */
usb_transfer_ctx_t* usb_transfer_alloc(void);

/**
 * @brief Free transfer context
 *
 * Releases all resources associated with a transfer context including
 * the libusb transfer object and synchronization primitives.
 *
 * @param ctx Transfer context to free (may be NULL)
 *
 * Note: This function safely handles NULL pointers and partially
 *       initialized contexts.
 */
void usb_transfer_free(usb_transfer_ctx_t* ctx);

/**
 * @brief Submit asynchronous bulk transfer
 *
 * Submits a bulk transfer for asynchronous execution. Use
 * usb_transfer_async_wait() to wait for completion.
 *
 * @param dev Device context
 * @param ctx Transfer context (from usb_transfer_alloc)
 * @param endpoint Endpoint address (IN or OUT)
 * @param buffer Data buffer (must remain valid until completion)
 * @param length Transfer length
 * @return 0 on success, negative error code on failure
 *
 * Note: The buffer must remain valid until the transfer completes.
 *       Do not free or modify the buffer until usb_transfer_async_wait()
 *       returns.
 */
int usb_transfer_async_submit(usb_device_t* dev,
                               usb_transfer_ctx_t* ctx,
                               uint8_t endpoint,
                               unsigned char* buffer,
                               int length);

/**
 * @brief Wait for asynchronous transfer completion
 *
 * Blocks until the transfer completes or the timeout expires.
 * This function handles libusb event processing.
 *
 * @param ctx Transfer context
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Number of bytes transferred on success, negative error code on failure
 *
 * Note: This function processes libusb events and may handle other
 *       pending transfers as well.
 */
int usb_transfer_async_wait(usb_transfer_ctx_t* ctx,
                             unsigned int timeout_ms);

/**
 * @brief Cancel asynchronous transfer
 *
 * Attempts to cancel a pending asynchronous transfer. The transfer
 * may still complete normally if already in progress.
 *
 * @param ctx Transfer context
 * @return 0 on success, negative error code on failure
 */
int usb_transfer_async_cancel(usb_transfer_ctx_t* ctx);

#endif /* USB_TRANSFER_H */
