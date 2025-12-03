/**
 * @file tls_context.c
 * @brief Implementation of TLS context management
 *
 * This module implements the global TLS context (SSL_CTX) initialization,
 * configuration, and cleanup. The context is shared across all client
 * connections and is thread-safe for read-only access after initialization.
 */

#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "tls_context.h"
#include "tls_config.h"
#include "tls_error.h"
#include "lib/common/definitions.h"

/**
 * @brief Load certificate and private key into SSL context
 *
 * Helper function to load server certificate and private key from files.
 * Validates that the key matches the certificate.
 *
 * @param ctx SSL context
 * @param cert_file Path to certificate file
 * @param key_file Path to key file
 * @return 0 on success, negative error code on failure
 */
static int load_certificates(SSL_CTX* ctx, const char* cert_file, const char* key_file) {
    /* Load server certificate */
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        tls_print_errors("Failed to load certificate");
        return E_TLS_CERT_INVALID;
    }

    /* Load private key */
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        tls_print_errors("Failed to load private key");
        return E_TLS_CERT_INVALID;
    }

    /* Verify that private key matches certificate */
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match certificate\n");
        return E_TLS_CERT_INVALID;
    }

    return 0;
}

SSL_CTX* tls_context_init(const char* cert_file, const char* key_file, int tls_version) {
    SSL_CTX* ctx;
    int ret;

    /* Validate arguments */
    if (cert_file == NULL || key_file == NULL) {
        fprintf(stderr, "Certificate and key file paths must not be NULL\n");
        return NULL;
    }

    if (tls_version != ENCRYPT_TLS12 && tls_version != ENCRYPT_TLS13) {
        fprintf(stderr, "Invalid TLS version: %d (must be ENCRYPT_TLS12 or ENCRYPT_TLS13)\n", tls_version);
        return NULL;
    }

    /* Create new SSL context using TLS server method */
    ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == NULL) {
        tls_print_errors("Failed to create SSL context");
        return NULL;
    }

    /* Configure TLS version and cipher suites */
    ret = tls_context_configure(ctx, tls_version);
    if (ret != 0) {
        SSL_CTX_free(ctx);
        return NULL;
    }

    /* Load certificate and key */
    ret = load_certificates(ctx, cert_file, key_file);
    if (ret != 0) {
        SSL_CTX_free(ctx);
        return NULL;
    }

    /* Enable session caching for performance */
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
    SSL_CTX_set_timeout(ctx, TLS_SESSION_TIMEOUT);

    return ctx;
}

int tls_context_configure(SSL_CTX* ctx, int tls_version) {
    int min_version;
    int max_version;

    if (ctx == NULL) {
        fprintf(stderr, "SSL context is NULL\n");
        return E_INVALID_ARGUMENT;
    }

    /* Determine TLS version constants based on encryption mode */
    if (tls_version == ENCRYPT_TLS12) {
        min_version = TLS1_2_VERSION;
        max_version = TLS1_2_VERSION;
    } else if (tls_version == ENCRYPT_TLS13) {
        min_version = TLS1_3_VERSION;
        max_version = TLS1_3_VERSION;
    } else {
        fprintf(stderr, "Invalid TLS version: %d\n", tls_version);
        return E_TLS_VERSION_MISMATCH;
    }

    /* Set minimum TLS version */
    if (!SSL_CTX_set_min_proto_version(ctx, min_version)) {
        tls_print_errors("Failed to set minimum TLS version");
        return E_TLS_VERSION_MISMATCH;
    }

    /* Set maximum TLS version (enforce specific version only) */
    if (!SSL_CTX_set_max_proto_version(ctx, max_version)) {
        tls_print_errors("Failed to set maximum TLS version");
        return E_TLS_VERSION_MISMATCH;
    }

    /* Set cipher suites for TLS 1.3 */
    /* Note: TLS 1.2 uses SSL_CTX_set_cipher_list(), but we use secure defaults */
    if (tls_version == ENCRYPT_TLS13) {
        if (!SSL_CTX_set_ciphersuites(ctx, TLS_CIPHER_SUITES)) {
            tls_print_errors("Failed to set cipher suites");
            return E_TLS_CIPHER_MISMATCH;
        }
    }

    /* Set secure options */
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);  /* Disable compression (CRIME attack) */
    SSL_CTX_set_options(ctx, SSL_OP_NO_RENEGOTIATION); /* Disable renegotiation */

    return 0;
}

void tls_context_cleanup(SSL_CTX* ctx) {
    if (ctx != NULL) {
        SSL_CTX_free(ctx);
    }
}
