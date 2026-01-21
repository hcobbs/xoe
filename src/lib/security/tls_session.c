/**
 * @file tls_session.c
 * @brief Implementation of TLS session management
 *
 * This module implements per-client TLS session management, including
 * session creation, handshake, shutdown, and cleanup. Each client thread
 * has exclusive ownership of its SSL object.
 */

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "tls_session.h"
#include "tls_error.h"
#include "lib/common/definitions.h"

SSL* tls_session_create(SSL_CTX* ctx, int client_socket) {
    SSL* ssl = NULL;
    int ret = 0;

    /* Validate arguments */
    if (ctx == NULL) {
        fprintf(stderr, "SSL context is NULL\n");
        return NULL;
    }

    if (client_socket < 0) {
        fprintf(stderr, "Invalid client socket: %d\n", client_socket);
        return NULL;
    }

    /* Create new SSL session from context */
    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        tls_print_errors("Failed to create SSL session");
        return NULL;
    }

    /* Bind SSL session to socket file descriptor */
    if (!SSL_set_fd(ssl, client_socket)) {
        tls_print_errors("Failed to bind SSL to socket");
        SSL_free(ssl);
        return NULL;
    }

    /* Perform TLS handshake */
    ret = tls_session_handshake(ssl);
    if (ret != 0) {
        SSL_free(ssl);
        return NULL;
    }

    return ssl;
}

int tls_session_handshake(SSL* ssl) {
    int ret = 0;
    int ssl_error = 0;

    if (ssl == NULL) {
        fprintf(stderr, "SSL session is NULL\n");
        return E_INVALID_ARGUMENT;
    }

    /* Perform server-side TLS handshake */
    ret = SSL_accept(ssl);
    if (ret <= 0) {
        ssl_error = SSL_get_error(ssl, ret);

        /* Map SSL error to xoe error code */
        switch (ssl_error) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                /* Should retry, but we're in blocking mode so this shouldn't happen */
                fprintf(stderr, "TLS handshake: unexpected WANT_READ/WANT_WRITE\n");
                return E_TIMEOUT;

            case SSL_ERROR_ZERO_RETURN:
                fprintf(stderr, "TLS handshake: connection closed by peer\n");
                return E_NETWORK_ERROR;

            case SSL_ERROR_SYSCALL:
                tls_print_errors("TLS handshake: system call error");
                return E_NETWORK_ERROR;

            case SSL_ERROR_SSL:
                tls_print_errors("TLS handshake: protocol error");
                return E_TLS_HANDSHAKE_FAILED;

            default:
                tls_print_errors("TLS handshake: unknown error");
                return E_UNKNOWN_ERROR;
        }
    }

    /* Handshake successful */
    return 0;
}

int tls_session_shutdown(SSL* ssl) {
    int ret = 0;
    int ssl_error = 0;

    if (ssl == NULL) {
        fprintf(stderr, "tls_session_shutdown: SSL session is NULL\n");
        return E_INVALID_ARGUMENT;
    }

    /* Send close_notify and wait for peer's close_notify */
    ret = SSL_shutdown(ssl);

    if (ret == 1) {
        /* Shutdown completed successfully (bidirectional) */
        return 0;
    } else if (ret == 0) {
        /* Sent close_notify, but haven't received peer's close_notify yet */
        /* Call SSL_shutdown again to complete bidirectional shutdown */
        ret = SSL_shutdown(ssl);
        if (ret == 1) {
            return 0;  /* Now completed */
        }
        /* If still not complete, that's OK - peer may have already closed */
        return 0;
    } else {
        /* Error during shutdown - log but don't fail */
        ssl_error = SSL_get_error(ssl, ret);
        if (ssl_error != SSL_ERROR_ZERO_RETURN) {
            /* Not a clean shutdown, but not critical */
            tls_print_errors("TLS shutdown warning");
        }
        return 0;  /* Non-fatal */
    }
}

SSL* tls_session_create_client(SSL_CTX* ctx, int server_socket) {
    SSL* ssl = NULL;
    int ret = 0;
    int ssl_error = 0;

    /* Validate arguments */
    if (ctx == NULL) {
        fprintf(stderr, "SSL context is NULL\n");
        return NULL;
    }

    if (server_socket < 0) {
        fprintf(stderr, "Invalid server socket: %d\n", server_socket);
        return NULL;
    }

    /* Create new SSL session from context */
    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        tls_print_errors("Failed to create SSL client session");
        return NULL;
    }

    /* Bind SSL session to socket file descriptor */
    if (!SSL_set_fd(ssl, server_socket)) {
        tls_print_errors("Failed to bind SSL to socket");
        SSL_free(ssl);
        return NULL;
    }

    /* Perform client-side TLS handshake */
    ret = SSL_connect(ssl);
    if (ret <= 0) {
        ssl_error = SSL_get_error(ssl, ret);

        /* Map SSL error to error messages */
        switch (ssl_error) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                fprintf(stderr, "TLS client handshake: unexpected WANT_READ/WANT_WRITE\n");
                break;

            case SSL_ERROR_ZERO_RETURN:
                fprintf(stderr, "TLS client handshake: connection closed by server\n");
                break;

            case SSL_ERROR_SYSCALL:
                tls_print_errors("TLS client handshake: system call error");
                break;

            case SSL_ERROR_SSL:
                tls_print_errors("TLS client handshake: protocol error");
                break;

            default:
                tls_print_errors("TLS client handshake: unknown error");
                break;
        }

        SSL_free(ssl);
        return NULL;
    }

    /* Handshake successful */
    return ssl;
}

SSL* tls_session_create_client_verified(SSL_CTX* ctx, int server_socket,
                                        const char* hostname) {
    SSL* ssl = NULL;
    int ret = 0;
    int ssl_error = 0;

    /* Validate arguments */
    if (ctx == NULL) {
        fprintf(stderr, "SSL context is NULL\n");
        return NULL;
    }

    if (server_socket < 0) {
        fprintf(stderr, "Invalid server socket: %d\n", server_socket);
        return NULL;
    }

    if (hostname == NULL || hostname[0] == '\0') {
        fprintf(stderr, "Hostname is required for verified TLS session\n");
        return NULL;
    }

    /* Create new SSL session from context */
    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        tls_print_errors("Failed to create SSL client session");
        return NULL;
    }

    /* Bind SSL session to socket file descriptor */
    if (!SSL_set_fd(ssl, server_socket)) {
        tls_print_errors("Failed to bind SSL to socket");
        SSL_free(ssl);
        return NULL;
    }

    /* Enable hostname verification */
    /* SSL_set1_host sets the expected hostname for verification */
    if (!SSL_set1_host(ssl, hostname)) {
        tls_print_errors("Failed to set hostname for verification");
        SSL_free(ssl);
        return NULL;
    }

    /* Set SNI (Server Name Indication) for virtual hosting */
    if (!SSL_set_tlsext_host_name(ssl, hostname)) {
        tls_print_errors("Failed to set SNI hostname");
        SSL_free(ssl);
        return NULL;
    }

    /* Perform client-side TLS handshake */
    ret = SSL_connect(ssl);
    if (ret <= 0) {
        ssl_error = SSL_get_error(ssl, ret);

        /* Map SSL error to error messages */
        switch (ssl_error) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                fprintf(stderr, "TLS client handshake: unexpected WANT_READ/WANT_WRITE\n");
                break;

            case SSL_ERROR_ZERO_RETURN:
                fprintf(stderr, "TLS client handshake: connection closed by server\n");
                break;

            case SSL_ERROR_SYSCALL:
                tls_print_errors("TLS client handshake: system call error");
                break;

            case SSL_ERROR_SSL:
                tls_print_errors("TLS client handshake: protocol or verification error");
                break;

            default:
                tls_print_errors("TLS client handshake: unknown error");
                break;
        }

        SSL_free(ssl);
        return NULL;
    }

    /* Handshake successful with hostname verification */
    return ssl;
}

void tls_session_destroy(SSL* ssl) {
    if (ssl != NULL) {
        SSL_free(ssl);
    }
}
