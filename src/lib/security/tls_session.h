/**
 * @file tls_session.h
 * @brief TLS session management API for per-client SSL connections
 *
 * This module manages per-client TLS sessions (SSL objects). Each client
 * connection has its own SSL object that handles the TLS handshake and
 * encrypted I/O for that specific connection.
 *
 * Each SSL session is owned exclusively by a single client thread, so no
 * locking is required for SSL operations.
 */

#ifndef TLS_SESSION_H
#define TLS_SESSION_H

#include "tls_types.h"

/**
 * @brief Create a new TLS session for a client connection
 *
 * Creates an SSL object from the global context, associates it with the
 * client socket, and performs the TLS handshake. This is a blocking operation
 * that will wait for the client to complete the handshake.
 *
 * On success, returns an SSL object ready for encrypted I/O.
 * On failure, returns NULL and the socket should be closed.
 *
 * @param ctx           Global SSL context (from tls_context_init)
 * @param client_socket Client socket file descriptor
 * @return SSL* on success, NULL on failure
 *
 * Error codes (check tls_get_last_error()):
 *   E_INVALID_ARGUMENT - Invalid ctx or socket
 *   E_OUT_OF_MEMORY    - SSL object allocation failed
 *   E_TLS_HANDSHAKE_FAILED - Handshake failed
 *   E_NETWORK_ERROR    - Network error during handshake
 *   E_TIMEOUT          - Handshake timeout
 *   E_UNKNOWN_ERROR    - Other OpenSSL error
 */
SSL* tls_session_create(SSL_CTX* ctx, int client_socket);

/**
 * @brief Perform TLS handshake
 *
 * Performs the server-side TLS handshake with the client.
 * This is a blocking operation.
 *
 * Called internally by tls_session_create(), but exposed for manual control
 * or retries.
 *
 * @param ssl SSL session object
 * @return 0 on success, negative error code on failure
 */
int tls_session_handshake(SSL* ssl);

/**
 * @brief Gracefully shutdown TLS session
 *
 * Sends a close_notify alert to the peer and waits for the peer's
 * close_notify response. This ensures a clean TLS shutdown.
 *
 * Non-blocking - does not wait indefinitely if peer doesn't respond.
 * Returns 0 on success, negative on error (which is non-fatal).
 *
 * Should be called before closing the underlying socket.
 *
 * @param ssl SSL session object
 * @return 0 on success, negative on error (non-fatal)
 */
int tls_session_shutdown(SSL* ssl);

/**
 * @brief Create a new TLS session for client-side connection
 *
 * Creates an SSL object from the client context, associates it with the
 * server socket, and performs the client-side TLS handshake.
 * This is a blocking operation that will wait for the server to complete the handshake.
 *
 * On success, returns an SSL object ready for encrypted I/O.
 * On failure, returns NULL and the socket should be closed.
 *
 * @param ctx           Client SSL context (from tls_context_init_client)
 * @param server_socket Server socket file descriptor
 * @return SSL* on success, NULL on failure
 *
 * Error codes (check tls_get_last_error()):
 *   E_INVALID_ARGUMENT - Invalid ctx or socket
 *   E_OUT_OF_MEMORY    - SSL object allocation failed
 *   E_TLS_HANDSHAKE_FAILED - Handshake failed
 *   E_NETWORK_ERROR    - Network error during handshake
 *   E_TIMEOUT          - Handshake timeout
 *   E_UNKNOWN_ERROR    - Other OpenSSL error
 */
SSL* tls_session_create_client(SSL_CTX* ctx, int server_socket);

/**
 * @brief Destroy TLS session and free resources
 *
 * Frees the SSL object and associated resources.
 * Does NOT close the underlying socket - caller must close socket separately.
 *
 * Safe to call with NULL (no-op).
 *
 * @param ssl SSL session object (may be NULL)
 */
void tls_session_destroy(SSL* ssl);

#endif /* TLS_SESSION_H */
