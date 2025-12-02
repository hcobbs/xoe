/**
 * @file tls_error.c
 * @brief Implementation of TLS error handling utilities
 *
 * This module implements thread-safe error reporting using pthread
 * thread-specific data. This is C89-compatible (pthread is POSIX, not C11).
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "tls_error.h"
#include "commonDefinitions.h"

/* Thread-specific data key for error buffer */
static pthread_key_t error_buffer_key;
static pthread_once_t error_buffer_key_once = PTHREAD_ONCE_INIT;

/* Error buffer size */
#define ERROR_BUFFER_SIZE 512

/**
 * @brief Initialize thread-specific data key for error buffer
 *
 * Called once via pthread_once to create the key for thread-local
 * error buffers.
 */
static void make_error_buffer_key(void) {
    /* Create key with free() as destructor to clean up thread-local buffers */
    pthread_key_create(&error_buffer_key, (void (*)(void*))free);
}

/**
 * @brief Get thread-local error buffer
 *
 * Returns a per-thread error buffer, allocating it on first access.
 * The buffer is automatically freed when the thread exits.
 *
 * @return Pointer to thread-local error buffer (never NULL)
 */
static char* get_error_buffer(void) {
    char* buffer;

    /* Ensure key is initialized */
    pthread_once(&error_buffer_key_once, make_error_buffer_key);

    /* Get thread-specific buffer */
    buffer = (char*)pthread_getspecific(error_buffer_key);

    if (buffer == NULL) {
        /* Allocate buffer for this thread */
        buffer = (char*)malloc(ERROR_BUFFER_SIZE);
        if (buffer != NULL) {
            pthread_setspecific(error_buffer_key, buffer);
            buffer[0] = '\0';
        } else {
            /* Allocation failed - return static buffer as fallback */
            static char fallback_buffer[] = "Error: Out of memory for error buffer";
            return fallback_buffer;
        }
    }

    return buffer;
}

int tls_get_last_error(void) {
    unsigned long err;

    /* Get last OpenSSL error from error queue */
    err = ERR_peek_last_error();

    if (err == 0) {
        return 0;  /* No error */
    }

    /* Map to generic error - specific mapping done by tls_map_error() */
    return E_TLS_PROTOCOL_ERROR;
}

const char* tls_get_error_string(void) {
    char* error_buffer;
    unsigned long err;

    error_buffer = get_error_buffer();

    /* Get last error from OpenSSL error queue */
    err = ERR_peek_last_error();

    if (err == 0) {
        strncpy(error_buffer, "No error", ERROR_BUFFER_SIZE - 1);
        error_buffer[ERROR_BUFFER_SIZE - 1] = '\0';
    } else {
        /* Convert error code to human-readable string */
        ERR_error_string_n(err, error_buffer, ERROR_BUFFER_SIZE);
    }

    return error_buffer;
}

int tls_map_error(void* ssl_void, int ssl_ret_code) {
    SSL* ssl;
    int ssl_error;

    if (ssl_void == NULL) {
        return E_INVALID_ARGUMENT;
    }

    ssl = (SSL*)ssl_void;
    ssl_error = SSL_get_error(ssl, ssl_ret_code);

    switch (ssl_error) {
        case SSL_ERROR_NONE:
            return 0;

        case SSL_ERROR_ZERO_RETURN:
            /* Clean shutdown */
            return 0;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
            /* Would block - treat as timeout for blocking sockets */
            return E_TIMEOUT;

        case SSL_ERROR_SYSCALL:
            /* System call error - usually network issue */
            return E_NETWORK_ERROR;

        case SSL_ERROR_SSL:
            /* SSL protocol error */
            return E_TLS_PROTOCOL_ERROR;

        default:
            return E_UNKNOWN_ERROR;
    }
}

void tls_print_errors(const char* prefix) {
    unsigned long err;
    char error_string[256];

    if (prefix != NULL) {
        fprintf(stderr, "%s:\n", prefix);
    }

    /* Print all errors from the OpenSSL error queue */
    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, error_string, sizeof(error_string));
        fprintf(stderr, "  OpenSSL error: %s\n", error_string);
    }

    /* If no errors were in queue */
    if (prefix != NULL && ERR_peek_error() == 0) {
        fprintf(stderr, "  (No OpenSSL errors in queue)\n");
    }
}
