/**
 * @file tls_context.h
 * @brief TLS context management API for global SSL_CTX initialization
 *
 * This module manages the global TLS context (SSL_CTX) which is shared
 * across all client connections. The context is initialized once at server
 * startup and is read-only afterward, making it thread-safe without locking.
 *
 * The SSL_CTX contains:
 * - Server certificate and private key
 * - TLS version constraints (TLS 1.3 only)
 * - Cipher suite configuration
 * - Session cache settings
 */

#ifndef TLS_CONTEXT_H
#define TLS_CONTEXT_H

#include "tls_types.h"

/**
 * @brief Initialize the global TLS context
 *
 * Creates and configures an SSL_CTX for TLS server operation.
 * Loads the server certificate and private key from the specified files.
 * Configures TLS version according to tls_version parameter.
 * Enables session caching for performance.
 *
 * Must be called once before accepting client connections.
 * The returned context is read-only and thread-safe after initialization.
 *
 * @param cert_file Path to PEM-encoded certificate file
 * @param key_file  Path to PEM-encoded private key file
 * @param tls_version TLS version to use (ENCRYPT_TLS12 or ENCRYPT_TLS13)
 * @return SSL_CTX* on success, NULL on failure
 *
 * Error codes (check tls_get_last_error()):
 *   E_INVALID_ARGUMENT - Invalid file paths (NULL) or invalid version
 *   E_FILE_NOT_FOUND   - Certificate or key file not found
 *   E_PERMISSION_DENIED - Cannot read cert/key files
 *   E_TLS_CERT_INVALID - Certificate/key validation failed
 *   E_UNKNOWN_ERROR    - OpenSSL initialization failed
 */
SSL_CTX* tls_context_init(const char* cert_file, const char* key_file, int tls_version);

/**
 * @brief Configure TLS context with cipher suites and options
 *
 * Sets minimum and maximum TLS version according to parameter.
 * Configures cipher suites from tls_config.h.
 * Enables session caching for connection resumption.
 * Sets secure default options.
 *
 * Called internally by tls_context_init(), but exposed for custom configs.
 *
 * @param ctx SSL context to configure
 * @param tls_version TLS version to enforce (ENCRYPT_TLS12 or ENCRYPT_TLS13)
 * @return 0 on success, negative error code on failure
 */
int tls_context_configure(SSL_CTX* ctx, int tls_version);

/**
 * @brief Initialize a TLS context for client mode
 *
 * Creates and configures an SSL_CTX for TLS client operation.
 * Configures TLS version according to tls_version parameter.
 * Does NOT require certificates (client mode).
 *
 * Must be called before initiating TLS connections to servers.
 * The returned context is read-only and thread-safe after initialization.
 *
 * @param tls_version TLS version to use (ENCRYPT_TLS12 or ENCRYPT_TLS13)
 * @return SSL_CTX* on success, NULL on failure
 *
 * Error codes (check tls_get_last_error()):
 *   E_INVALID_ARGUMENT - Invalid version
 *   E_UNKNOWN_ERROR    - OpenSSL initialization failed
 */
SSL_CTX* tls_context_init_client(int tls_version);

/**
 * @brief Clean up the global TLS context
 *
 * Frees all resources associated with the SSL_CTX.
 * Must be called after all client connections are closed.
 * Not thread-safe - call only during server shutdown.
 *
 * @param ctx SSL context to destroy (may be NULL, in which case no-op)
 */
void tls_context_cleanup(SSL_CTX* ctx);

#endif /* TLS_CONTEXT_H */
