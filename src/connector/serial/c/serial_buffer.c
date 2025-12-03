/**
 * @file serial_buffer.c
 * @brief Thread-safe circular buffer implementation
 *
 * Implements a circular buffer with mutex-protected operations and
 * condition variables for blocking on full/empty conditions. Handles
 * network→serial speed mismatch by buffering incoming data.
 *
 * [LLM-ASSISTED]
 */

#include "../h/serial_buffer.h"
#include "../../../common/h/commonDefinitions.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Initialize a circular buffer
 */
int serial_buffer_init(serial_buffer_t* buffer, uint32_t capacity)
{
    int result;

    if (buffer == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Use default capacity if not specified */
    if (capacity == 0) {
        capacity = SERIAL_BUFFER_DEFAULT_SIZE;
    }

    /* Initialize structure */
    memset(buffer, 0, sizeof(serial_buffer_t));
    buffer->capacity = capacity;

    /* Allocate buffer memory */
    buffer->data = (unsigned char*)malloc(capacity);
    if (buffer->data == NULL) {
        return E_OUT_OF_MEMORY;
    }

    /* Initialize mutex */
    result = pthread_mutex_init(&buffer->mutex, NULL);
    if (result != 0) {
        free(buffer->data);
        return E_UNKNOWN_ERROR;
    }

    /* Initialize condition variables */
    result = pthread_cond_init(&buffer->not_empty, NULL);
    if (result != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->data);
        return E_UNKNOWN_ERROR;
    }

    result = pthread_cond_init(&buffer->not_full, NULL);
    if (result != 0) {
        pthread_cond_destroy(&buffer->not_empty);
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->data);
        return E_UNKNOWN_ERROR;
    }

    return 0;
}

/**
 * @brief Destroy a circular buffer
 */
void serial_buffer_destroy(serial_buffer_t* buffer)
{
    if (buffer == NULL) {
        return;
    }

    /* Wake all waiting threads before destroying */
    pthread_mutex_lock(&buffer->mutex);
    buffer->closed = TRUE;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_cond_broadcast(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);

    /* Destroy synchronization primitives */
    pthread_cond_destroy(&buffer->not_full);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_mutex_destroy(&buffer->mutex);

    /* Free buffer memory */
    if (buffer->data != NULL) {
        free(buffer->data);
        buffer->data = NULL;
    }
}

/**
 * @brief Write data to circular buffer
 */
int serial_buffer_write(serial_buffer_t* buffer, const void* data,
                        uint32_t len)
{
    const unsigned char* src;
    uint32_t bytes_written;
    uint32_t chunk_size;
    uint32_t space_to_end;

    if (buffer == NULL || data == NULL || len == 0) {
        return E_INVALID_ARGUMENT;
    }

    src = (const unsigned char*)data;
    bytes_written = 0;

    pthread_mutex_lock(&buffer->mutex);

    /* Check if buffer is closed */
    if (buffer->closed) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }

    /* Write data in chunks as space becomes available */
    while (bytes_written < len) {
        /* Wait for space if buffer is full */
        while (buffer->count == buffer->capacity && !buffer->closed) {
            pthread_cond_wait(&buffer->not_full, &buffer->mutex);
        }

        /* Check if buffer was closed while waiting */
        if (buffer->closed) {
            pthread_mutex_unlock(&buffer->mutex);
            return bytes_written;
        }

        /* Calculate how much we can write */
        chunk_size = len - bytes_written;
        if (chunk_size > buffer->capacity - buffer->count) {
            chunk_size = buffer->capacity - buffer->count;
        }

        /* Handle wraparound */
        space_to_end = buffer->capacity - buffer->head;
        if (chunk_size > space_to_end) {
            /* Write first part to end of buffer */
            memcpy(buffer->data + buffer->head, src + bytes_written,
                   space_to_end);
            /* Write second part to beginning of buffer */
            memcpy(buffer->data, src + bytes_written + space_to_end,
                   chunk_size - space_to_end);
        } else {
            /* Write entire chunk */
            memcpy(buffer->data + buffer->head, src + bytes_written,
                   chunk_size);
        }

        /* Update head position */
        buffer->head = (buffer->head + chunk_size) % buffer->capacity;
        buffer->count += chunk_size;
        bytes_written += chunk_size;

        /* Signal that data is available */
        pthread_cond_signal(&buffer->not_empty);
    }

    pthread_mutex_unlock(&buffer->mutex);
    return bytes_written;
}

/**
 * @brief Read data from circular buffer
 */
int serial_buffer_read(serial_buffer_t* buffer, void* data, uint32_t max_len)
{
    unsigned char* dst;
    uint32_t bytes_read;
    uint32_t chunk_size;
    uint32_t data_to_end;

    if (buffer == NULL || data == NULL || max_len == 0) {
        return E_INVALID_ARGUMENT;
    }

    dst = (unsigned char*)data;
    bytes_read = 0;

    pthread_mutex_lock(&buffer->mutex);

    /* Wait for data if buffer is empty */
    while (buffer->count == 0 && !buffer->closed) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    /* Check if buffer is closed and empty */
    if (buffer->closed && buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }

    /* Calculate how much we can read */
    chunk_size = max_len;
    if (chunk_size > buffer->count) {
        chunk_size = buffer->count;
    }

    /* Handle wraparound */
    data_to_end = buffer->capacity - buffer->tail;
    if (chunk_size > data_to_end) {
        /* Read first part from tail to end of buffer */
        memcpy(dst, buffer->data + buffer->tail, data_to_end);
        /* Read second part from beginning of buffer */
        memcpy(dst + data_to_end, buffer->data,
               chunk_size - data_to_end);
    } else {
        /* Read entire chunk */
        memcpy(dst, buffer->data + buffer->tail, chunk_size);
    }

    /* Update tail position */
    buffer->tail = (buffer->tail + chunk_size) % buffer->capacity;
    buffer->count -= chunk_size;
    bytes_read = chunk_size;

    /* Signal that space is available */
    pthread_cond_signal(&buffer->not_full);

    pthread_mutex_unlock(&buffer->mutex);
    return bytes_read;
}

/**
 * @brief Get number of bytes available in buffer
 */
uint32_t serial_buffer_available(serial_buffer_t* buffer)
{
    uint32_t count;

    if (buffer == NULL) {
        return 0;
    }

    pthread_mutex_lock(&buffer->mutex);
    count = buffer->count;
    pthread_mutex_unlock(&buffer->mutex);

    return count;
}

/**
 * @brief Get free space in buffer
 */
uint32_t serial_buffer_free_space(serial_buffer_t* buffer)
{
    uint32_t free_space;

    if (buffer == NULL) {
        return 0;
    }

    pthread_mutex_lock(&buffer->mutex);
    free_space = buffer->capacity - buffer->count;
    pthread_mutex_unlock(&buffer->mutex);

    return free_space;
}

/**
 * @brief Close buffer for writing
 */
void serial_buffer_close(serial_buffer_t* buffer)
{
    if (buffer == NULL) {
        return;
    }

    pthread_mutex_lock(&buffer->mutex);
    buffer->closed = TRUE;
    /* Wake all waiting threads */
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_cond_broadcast(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
}

/**
 * @brief Check if buffer is closed
 */
int serial_buffer_is_closed(serial_buffer_t* buffer)
{
    int closed;

    if (buffer == NULL) {
        return FALSE;
    }

    pthread_mutex_lock(&buffer->mutex);
    closed = buffer->closed;
    pthread_mutex_unlock(&buffer->mutex);

    return closed;
}
