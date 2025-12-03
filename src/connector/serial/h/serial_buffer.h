/**
 * @file serial_buffer.h
 * @brief Thread-safe circular buffer for serial data flow control
 *
 * Provides a circular buffer implementation for handling the network→serial
 * speed mismatch. Implements thread-safe read/write operations using mutexes
 * and condition variables for blocking on full/empty conditions.
 *
 * The buffer size is configured to provide approximately 16 seconds of
 * buffering at 9600 baud (16KB).
 *
 * [LLM-ASSISTED]
 */

#ifndef SERIAL_BUFFER_H
#define SERIAL_BUFFER_H

#include <pthread.h>
#include "../../../core/packet_manager/h/protocol.h"

/* Default buffer size (16KB) */
#define SERIAL_BUFFER_DEFAULT_SIZE 16384

/**
 * @brief Circular buffer structure for serial data
 *
 * Implements a thread-safe circular buffer with blocking read/write
 * operations. Uses condition variables to wake waiting threads when
 * data becomes available or space is freed.
 */
typedef struct {
    unsigned char* data;      /* Buffer memory */
    uint32_t capacity;        /* Total buffer capacity */
    uint32_t head;            /* Write position */
    uint32_t tail;            /* Read position */
    uint32_t count;           /* Number of bytes currently in buffer */
    pthread_mutex_t mutex;    /* Mutex for thread safety */
    pthread_cond_t not_empty; /* Condition: buffer has data */
    pthread_cond_t not_full;  /* Condition: buffer has space */
    int closed;               /* Flag: buffer closed for writing */
} serial_buffer_t;

/**
 * @brief Initialize a circular buffer
 *
 * Allocates memory for the buffer and initializes synchronization primitives.
 * The buffer must be freed with serial_buffer_destroy() when no longer needed.
 *
 * @param buffer Pointer to buffer structure to initialize
 * @param capacity Buffer capacity in bytes (0 for default size)
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT - NULL pointer
 *         E_OUT_OF_MEMORY - Memory allocation failed
 *         E_UNKNOWN_ERROR - Mutex/condition variable initialization failed
 */
int serial_buffer_init(serial_buffer_t* buffer, uint32_t capacity);

/**
 * @brief Destroy a circular buffer
 *
 * Frees all resources associated with the buffer including memory and
 * synchronization primitives. Any threads blocked on read/write operations
 * should be unblocked before calling this function.
 *
 * @param buffer Pointer to buffer to destroy
 */
void serial_buffer_destroy(serial_buffer_t* buffer);

/**
 * @brief Write data to circular buffer
 *
 * Writes data to the buffer. Blocks if the buffer is full until space
 * becomes available. Returns immediately if the buffer has been closed.
 *
 * @param buffer Pointer to buffer
 * @param data Data to write
 * @param len Number of bytes to write
 * @return Number of bytes written on success (may be less than len)
 *         0 if buffer is closed
 *         Negative error code on failure
 *         E_INVALID_ARGUMENT - NULL pointer or invalid length
 */
int serial_buffer_write(serial_buffer_t* buffer, const void* data,
                        uint32_t len);

/**
 * @brief Read data from circular buffer
 *
 * Reads data from the buffer. Blocks if the buffer is empty until data
 * becomes available or the buffer is closed.
 *
 * @param buffer Pointer to buffer
 * @param data Output buffer for data
 * @param max_len Maximum number of bytes to read
 * @return Number of bytes read on success (may be less than max_len)
 *         0 if buffer is closed and empty
 *         Negative error code on failure
 *         E_INVALID_ARGUMENT - NULL pointer or invalid length
 */
int serial_buffer_read(serial_buffer_t* buffer, void* data, uint32_t max_len);

/**
 * @brief Get number of bytes available in buffer
 *
 * Thread-safe query of current buffer occupancy.
 *
 * @param buffer Pointer to buffer
 * @return Number of bytes available, or 0 on error
 */
uint32_t serial_buffer_available(serial_buffer_t* buffer);

/**
 * @brief Get free space in buffer
 *
 * Thread-safe query of available write space.
 *
 * @param buffer Pointer to buffer
 * @return Number of bytes of free space, or 0 on error
 */
uint32_t serial_buffer_free_space(serial_buffer_t* buffer);

/**
 * @brief Close buffer for writing
 *
 * Marks the buffer as closed for writing. Write operations will return
 * immediately. Read operations will continue to work until the buffer
 * is drained, then return 0.
 *
 * This is used during shutdown to signal the read thread that no more
 * data will arrive.
 *
 * @param buffer Pointer to buffer
 */
void serial_buffer_close(serial_buffer_t* buffer);

/**
 * @brief Check if buffer is closed
 *
 * @param buffer Pointer to buffer
 * @return TRUE if closed, FALSE otherwise
 */
int serial_buffer_is_closed(serial_buffer_t* buffer);

#endif /* SERIAL_BUFFER_H */
