/**
 * @file tls_error.h
 * @brief TLS error handling utilities
 *
 * This module provides thread-safe error reporting and error code mapping
 * between OpenSSL errors and xoe error codes. It uses pthread thread-specific
 * data to ensure thread safety in a C89-compatible way.
 */

#ifndef TLS_ERROR_H
#define TLS_ERROR_H

/**
 * @brief Get the last TLS error code
 *
 * Returns an error code from commonDefinitions.h (E_* constants).
 * Thread-safe using thread-local storage.
 *
 * Currently maps OpenSSL errors to xoe error codes. Future implementation
 * could store per-thread error state.
 *
 * @return Error code (negative value from commonDefinitions.h)
 */
int tls_get_last_error(void);

/**
 * @brief Get human-readable error string
 *
 * Returns a description of the last OpenSSL error from the error queue.
 * Thread-safe (uses thread-local storage for error buffer).
 *
 * The returned string is valid until the next error occurs in the same thread
 * or until the thread terminates.
 *
 * @return Error string (never NULL, returns "No error" if no error)
 */
const char* tls_get_error_string(void);

/**
 * @brief Convert OpenSSL error to xoe error code
 *
 * Maps SSL_get_error() return codes to E_* error codes from
 * commonDefinitions.h. This provides a consistent error reporting
 * interface across the xoe codebase.
 *
 * @param ssl          SSL object (may be NULL for context errors)
 * @param ssl_ret_code Return code from SSL function (SSL_read, SSL_write, etc.)
 * @return Mapped error code from commonDefinitions.h
 */
int tls_map_error(void* ssl, int ssl_ret_code);

/**
 * @brief Print detailed OpenSSL error queue
 *
 * Debugging utility to print all pending OpenSSL errors from the error queue.
 * Prints to stderr with the provided prefix.
 * Clears the error queue after printing.
 *
 * Useful for debugging TLS issues during development and troubleshooting.
 *
 * @param prefix Message to print before errors (e.g., "TLS handshake failed")
 */
void tls_print_errors(const char* prefix);

#endif /* TLS_ERROR_H */
