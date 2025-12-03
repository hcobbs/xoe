/**
 * @file tls_types.h
 * @brief Common forward declarations for OpenSSL types
 *
 * This header provides C89-compatible forward declarations for OpenSSL
 * types (SSL and SSL_CTX) to avoid redefinition warnings when multiple
 * TLS module headers are included.
 *
 * All TLS-related headers should include this file instead of declaring
 * these types independently.
 *
 * The forward declarations are guarded to prevent conflicts with OpenSSL
 * headers if they are included directly in implementation files.
 */

#ifndef TLS_TYPES_H
#define TLS_TYPES_H

/*
 * Forward declarations for OpenSSL types (C89 compatible).
 * Only define if not already defined by OpenSSL headers.
 */
#ifndef OPENSSL_SSL_H
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
#endif

#endif /* TLS_TYPES_H */
