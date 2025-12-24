/**
 * @file tls_config.h
 * @brief TLS configuration constants and compile-time options
 *
 * This file contains all configuration constants for the TLS implementation,
 * including enable/disable flags, certificate paths, version constraints,
 * and cipher suite preferences.
 */

#ifndef TLS_CONFIG_H
#define TLS_CONFIG_H

/* TLS Enable/Disable - Set to 0 to disable TLS and use plain TCP */
#define TLS_ENABLED 1

/* Encryption Mode Selection (runtime) */
#define ENCRYPT_NONE  0  /* Plain TCP, no encryption */
#define ENCRYPT_TLS12 1  /* TLS 1.2 only */
#define ENCRYPT_TLS13 2  /* TLS 1.3 only (recommended) */

/* Certificate and Key File Paths */
#define TLS_CERT_PATH_MAX 256
#define TLS_DEFAULT_CERT_FILE "./certs/server.crt"
#define TLS_DEFAULT_KEY_FILE  "./certs/server.key"
#define TLS_DEFAULT_CA_FILE   ""  /* Empty = use system CA store */

/* TLS Version Configuration */
/* Note: TLS_MIN_VERSION uses OpenSSL constants defined in openssl/tls1.h */
/* TLS1_3_VERSION is defined as 0x0304 in OpenSSL 1.1.1+ */

/* TLS Cipher Suite Configuration */
/* TLS 1.3 cipher suites (TLSv1.3 ciphersuites parameter) */
#define TLS_CIPHER_SUITES "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:TLS_CHACHA20_POLY1305_SHA256"

/* TLS 1.2 cipher list (strong ciphers only with forward secrecy) */
/* Prioritizes ECDHE for forward secrecy, AES-GCM and ChaCha20 for AEAD */
/* Excludes: anonymous auth, export ciphers, DES, RC4, MD5, PSK */
#define TLS_CIPHER_LIST "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:" \
                        "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:" \
                        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256"

/* Client Certificate Verification Mode */
#define TLS_VERIFY_NONE   0  /* No client certificate verification */
#define TLS_VERIFY_PEER   1  /* Request and verify client certificate */

#define TLS_DEFAULT_VERIFY_MODE TLS_VERIFY_PEER

/* TLS Session Configuration */
#define TLS_SESSION_TIMEOUT 300  /* Session timeout in seconds (5 minutes) */

/* TLS Buffer Sizes */
/* Note: OpenSSL TLS record size is typically 16KB, but we use smaller
 * buffers to match the existing xoe server buffer size */
#define TLS_MAX_RECORD_SIZE 16384  /* Maximum TLS record size */

#endif /* TLS_CONFIG_H */
