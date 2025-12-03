/**
 * @file tls_io.h
 * @brief TLS I/O wrappers for encrypted read/write operations
 *
 * This module provides drop-in replacements for recv() and send() that
 * handle TLS encrypted I/O. The functions have the same semantics as
 * standard socket I/O functions, making integration straightforward.
 *
 * These functions handle SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE
 * internally for blocking sockets.
 */

#ifndef TLS_IO_H
#define TLS_IO_H

#include "tls_types.h"

/**
 * @brief Read data from TLS connection
 *
 * Drop-in replacement for recv(). Reads up to len bytes from the encrypted
 * TLS connection into buffer. Handles SSL_ERROR_WANT_READ internally for
 * blocking sockets.
 *
 * @param ssl    SSL session object
 * @param buffer Buffer to receive data
 * @param len    Maximum bytes to read
 * @return Number of bytes read on success,
 *         0 on clean connection shutdown,
 *         -1 on error
 *
 * Error codes (check tls_get_last_error()):
 *   E_INVALID_ARGUMENT - Invalid parameters (NULL ssl or buffer)
 *   E_NETWORK_ERROR    - Connection closed unexpectedly or network error
 *   E_TLS_PROTOCOL_ERROR - SSL protocol error
 *   E_UNKNOWN_ERROR    - Other SSL error
 */
int tls_read(SSL* ssl, void* buffer, int len);

/**
 * @brief Write data to TLS connection
 *
 * Drop-in replacement for send(). Writes len bytes from buffer to the
 * encrypted TLS connection. Handles SSL_ERROR_WANT_WRITE internally for
 * blocking sockets.
 *
 * @param ssl    SSL session object
 * @param buffer Data to send
 * @param len    Number of bytes to send
 * @return Number of bytes written on success,
 *         -1 on error
 *
 * Error codes (check tls_get_last_error()):
 *   E_INVALID_ARGUMENT - Invalid parameters (NULL ssl or buffer)
 *   E_NETWORK_ERROR    - Connection closed unexpectedly or network error
 *   E_TLS_PROTOCOL_ERROR - SSL protocol error
 *   E_UNKNOWN_ERROR    - Other SSL error
 */
int tls_write(SSL* ssl, const void* buffer, int len);

#endif /* TLS_IO_H */
