/*
 * usb_transfer.c - USB Transfer Operations Implementation
 *
 * Implements USB transfer functions for Control, Bulk, and Interrupt
 * transfers with both synchronous and asynchronous APIs.
 *
 * EXPERIMENTAL FEATURE - Subject to change
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_transfer.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

/* ETIMEDOUT fallback only for systems that don't define it in errno.h */
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/**
 * @brief Map libusb error codes to XOE error codes
 *
 * Converts libusb-specific error codes to XOE standard error codes.
 */
static int map_libusb_error(int libusb_error)
{
    switch (libusb_error) {
        case LIBUSB_SUCCESS:
            return 0;
        case LIBUSB_ERROR_IO:
            return E_IO_ERROR;
        case LIBUSB_ERROR_INVALID_PARAM:
            return E_INVALID_ARGUMENT;
        case LIBUSB_ERROR_ACCESS:
            return E_PERMISSION_DENIED;
        case LIBUSB_ERROR_NO_DEVICE:
            return E_USB_NOT_FOUND;
        case LIBUSB_ERROR_NOT_FOUND:
            return E_NOT_FOUND;
        case LIBUSB_ERROR_BUSY:
            return E_DEVICE_BUSY;
        case LIBUSB_ERROR_TIMEOUT:
            return E_TIMEOUT;
        case LIBUSB_ERROR_OVERFLOW:
            return E_BUFFER_TOO_SMALL;
        case LIBUSB_ERROR_PIPE:
            return E_USB_PIPE_ERROR;
        case LIBUSB_ERROR_INTERRUPTED:
            return E_INTERRUPTED;
        case LIBUSB_ERROR_NO_MEM:
            return E_OUT_OF_MEMORY;
        case LIBUSB_ERROR_NOT_SUPPORTED:
            return E_NOT_SUPPORTED;
        default:
            return E_USB_TRANSFER_ERROR;
    }
}

/* ========================================================================
 * Synchronous Transfer Functions
 * ======================================================================== */

/**
 * @brief Perform synchronous USB control transfer
 */
int usb_transfer_control(usb_device_t* dev,
                         uint8_t bmRequestType,
                         uint8_t bRequest,
                         uint16_t wValue,
                         uint16_t wIndex,
                         unsigned char* data,
                         uint16_t wLength,
                         unsigned int timeout)
{
    int result = 0;

    /* Validate input parameters */
    if (dev == NULL || dev->handle == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Perform synchronous control transfer */
    result = libusb_control_transfer(
        dev->handle,
        bmRequestType,
        bRequest,
        wValue,
        wIndex,
        data,
        wLength,
        timeout
    );

    /* Map error codes */
    if (result < 0) {
        return map_libusb_error(result);
    }

    /* Return number of bytes transferred */
    return result;
}

/**
 * @brief Perform synchronous bulk read transfer
 */
int usb_transfer_bulk_read(usb_device_t* dev,
                           uint8_t endpoint,
                           unsigned char* buffer,
                           int length,
                           int* transferred,
                           unsigned int timeout)
{
    int result = 0;
    int bytes_transferred = 0;

    /* Validate input parameters */
    if (dev == NULL || dev->handle == NULL || buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate endpoint direction (must be IN) */
    if ((endpoint & 0x80) == 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Perform synchronous bulk transfer */
    result = libusb_bulk_transfer(
        dev->handle,
        endpoint,
        buffer,
        length,
        &bytes_transferred,
        timeout
    );

    /* Store transferred bytes if requested */
    if (transferred != NULL) {
        *transferred = bytes_transferred;
    }

    /* Map error codes */
    if (result < 0) {
        return map_libusb_error(result);
    }

    return 0;
}

/**
 * @brief Perform synchronous bulk write transfer
 */
int usb_transfer_bulk_write(usb_device_t* dev,
                            uint8_t endpoint,
                            const unsigned char* buffer,
                            int length,
                            int* transferred,
                            unsigned int timeout)
{
    int result = 0;
    int bytes_transferred = 0;

    /* Validate input parameters */
    if (dev == NULL || dev->handle == NULL || buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate endpoint direction (must be OUT) */
    if ((endpoint & 0x80) != 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Perform synchronous bulk transfer (cast away const for libusb API) */
    result = libusb_bulk_transfer(
        dev->handle,
        endpoint,
        (unsigned char*)buffer,  /* libusb API doesn't use const */
        length,
        &bytes_transferred,
        timeout
    );

    /* Store transferred bytes if requested */
    if (transferred != NULL) {
        *transferred = bytes_transferred;
    }

    /* Map error codes */
    if (result < 0) {
        return map_libusb_error(result);
    }

    return 0;
}

/**
 * @brief Perform synchronous interrupt read transfer
 */
int usb_transfer_interrupt_read(usb_device_t* dev,
                                uint8_t endpoint,
                                unsigned char* buffer,
                                int length,
                                int* transferred,
                                unsigned int timeout)
{
    int result = 0;
    int bytes_transferred = 0;

    /* Validate input parameters */
    if (dev == NULL || dev->handle == NULL || buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate endpoint direction (must be IN) */
    if ((endpoint & 0x80) == 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Perform synchronous interrupt transfer */
    result = libusb_interrupt_transfer(
        dev->handle,
        endpoint,
        buffer,
        length,
        &bytes_transferred,
        timeout
    );

    /* Store transferred bytes if requested */
    if (transferred != NULL) {
        *transferred = bytes_transferred;
    }

    /* Map error codes */
    if (result < 0) {
        return map_libusb_error(result);
    }

    return 0;
}

/* ========================================================================
 * Asynchronous Transfer Functions
 * ======================================================================== */

/**
 * @brief Asynchronous transfer completion callback
 *
 * Called by libusb when an asynchronous transfer completes.
 */
static void async_transfer_callback(struct libusb_transfer* transfer)
{
    usb_transfer_ctx_t* ctx = (usb_transfer_ctx_t*)transfer->user_data;

    if (ctx == NULL) {
        return;
    }

    /* Lock context */
    pthread_mutex_lock(&ctx->lock);

    /* Mark as completed */
    ctx->completed = TRUE;
    ctx->result = transfer->status;

    /* Update URB header with actual length */
    ctx->urb_header.actual_length = (uint32_t)transfer->actual_length;
    ctx->urb_header.status = transfer->status;

    /* Signal completion */
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);
}

/**
 * @brief Allocate transfer context for asynchronous operations
 */
usb_transfer_ctx_t* usb_transfer_alloc(void)
{
    usb_transfer_ctx_t* ctx = NULL;

    /* Allocate context structure */
    ctx = (usb_transfer_ctx_t*)malloc(sizeof(usb_transfer_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    /* Initialize fields */
    memset(ctx, 0, sizeof(usb_transfer_ctx_t));
    ctx->completed = FALSE;
    ctx->result = 0;
    ctx->user_data = NULL;

    /* Allocate libusb transfer */
    ctx->transfer = libusb_alloc_transfer(0);
    if (ctx->transfer == NULL) {
        free(ctx);
        return NULL;
    }

    /* Initialize synchronization primitives */
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    return ctx;
}

/**
 * @brief Free transfer context
 */
void usb_transfer_free(usb_transfer_ctx_t* ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* Free libusb transfer */
    if (ctx->transfer != NULL) {
        libusb_free_transfer(ctx->transfer);
    }

    /* Destroy synchronization primitives */
    pthread_mutex_destroy(&ctx->lock);
    pthread_cond_destroy(&ctx->cond);

    /* Free context */
    free(ctx);
}

/**
 * @brief Submit asynchronous bulk transfer
 */
int usb_transfer_async_submit(usb_device_t* dev,
                               usb_transfer_ctx_t* ctx,
                               uint8_t endpoint,
                               unsigned char* buffer,
                               int length)
{
    int result = 0;

    /* Validate parameters */
    if (dev == NULL || dev->handle == NULL || ctx == NULL ||
        ctx->transfer == NULL || buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Reset completion state */
    pthread_mutex_lock(&ctx->lock);

    ctx->completed = FALSE;
    ctx->result = 0;

    pthread_mutex_unlock(&ctx->lock);

    /* Fill transfer structure */
    libusb_fill_bulk_transfer(
        ctx->transfer,
        dev->handle,
        endpoint,
        buffer,
        length,
        async_transfer_callback,
        ctx,
        dev->config.transfer_timeout_ms
    );

    /* Initialize URB header */
    memset(&ctx->urb_header, 0, sizeof(usb_urb_header_t));
    ctx->urb_header.endpoint = endpoint;
    ctx->urb_header.transfer_type = USB_TRANSFER_BULK;
    ctx->urb_header.transfer_length = (uint32_t)length;
    ctx->urb_header.device_id =
        ((uint32_t)dev->config.vendor_id << 16) | dev->config.product_id;

    /* Submit transfer */
    result = libusb_submit_transfer(ctx->transfer);
    if (result < 0) {
        return map_libusb_error(result);
    }

    return 0;
}

/**
 * @brief Wait for asynchronous transfer completion
 */
int usb_transfer_async_wait(usb_transfer_ctx_t* ctx,
                             unsigned int timeout_ms)
{
    int result = 0;
    struct timespec ts = {0};
    struct timeval now = {0};

    /* Validate parameters */
    if (ctx == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Lock context */
    pthread_mutex_lock(&ctx->lock);

    /* Wait for completion */
    while (!ctx->completed) {
        if (timeout_ms == 0) {
            /* Wait indefinitely */
            pthread_cond_wait(&ctx->cond, &ctx->lock);
        } else {
            /* Wait with timeout */
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec + (timeout_ms / 1000);
            ts.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }

            result = pthread_cond_timedwait(&ctx->cond, &ctx->lock, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&ctx->lock);
                return E_TIMEOUT;
            }
        }
    }

    /* Check transfer result */
    if (ctx->result != LIBUSB_TRANSFER_COMPLETED) {
        result = map_libusb_error(ctx->result);
        pthread_mutex_unlock(&ctx->lock);
        return result;
    }

    /* Get transferred bytes */
    result = (int)ctx->urb_header.actual_length;

    pthread_mutex_unlock(&ctx->lock);

    return result;
}

/**
 * @brief Cancel asynchronous transfer
 */
int usb_transfer_async_cancel(usb_transfer_ctx_t* ctx)
{
    int result = 0;

    /* Validate parameters */
    if (ctx == NULL || ctx->transfer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Cancel transfer */
    result = libusb_cancel_transfer(ctx->transfer);
    if (result < 0) {
        return map_libusb_error(result);
    }

    return 0;
}
