/**
 * @file tls_io.c
 * @brief Implementation of TLS I/O wrappers
 *
 * This module implements encrypted I/O functions that serve as drop-in
 * replacements for recv() and send(). These functions handle TLS protocol
 * details and provide a familiar socket-like interface.
 */

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "tls_io.h"
#include "tls_error.h"
#include "lib/common/definitions.h"

int tls_read(SSL* ssl, void* buffer, int len) {
    int ret = 0;
    int ssl_error = 0;

    /* Validate arguments */
    if (ssl == NULL || buffer == NULL) {
        fprintf(stderr, "tls_read: Invalid arguments\n");
        return -1;
    }

    if (len <= 0) {
        return 0;
    }

    /* Read from TLS connection */
    ret = SSL_read(ssl, buffer, len);

    if (ret > 0) {
        /* Successful read */
        return ret;
    } else if (ret == 0) {
        /* Clean connection shutdown */
        return 0;
    } else {
        /* Error occurred */
        ssl_error = SSL_get_error(ssl, ret);

        switch (ssl_error) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                /* For blocking sockets, this shouldn't happen */
                /* For non-blocking, caller should retry */
                fprintf(stderr, "tls_read: WANT_READ/WANT_WRITE (unexpected for blocking socket)\n");
                return -1;

            case SSL_ERROR_ZERO_RETURN:
                /* Peer sent close_notify - clean shutdown */
                return 0;

            case SSL_ERROR_SYSCALL:
                /* Underlying network error */
                if (ERR_peek_error() == 0) {
                    /* EOF or connection reset */
                    if (ret == 0) {
                        /* Unexpected EOF */
                        fprintf(stderr, "tls_read: Unexpected EOF\n");
                    } else {
                        /* System call error - errno is set */
                        perror("tls_read: System call error");
                    }
                } else {
                    tls_print_errors("tls_read: System call error");
                }
                return -1;

            case SSL_ERROR_SSL:
                /* SSL protocol error */
                tls_print_errors("tls_read: Protocol error");
                return -1;

            default:
                tls_print_errors("tls_read: Unknown error");
                return -1;
        }
    }
}

int tls_write(SSL* ssl, const void* buffer, int len) {
    int ret = 0;
    int ssl_error = 0;

    /* Validate arguments */
    if (ssl == NULL || buffer == NULL) {
        fprintf(stderr, "tls_write: Invalid arguments\n");
        return -1;
    }

    if (len <= 0) {
        return 0;
    }

    /* Write to TLS connection */
    ret = SSL_write(ssl, buffer, len);

    if (ret > 0) {
        /* Successful write */
        return ret;
    } else {
        /* Error occurred */
        ssl_error = SSL_get_error(ssl, ret);

        switch (ssl_error) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                /* For blocking sockets, this shouldn't happen */
                /* For non-blocking, caller should retry */
                fprintf(stderr, "tls_write: WANT_READ/WANT_WRITE (unexpected for blocking socket)\n");
                return -1;

            case SSL_ERROR_ZERO_RETURN:
                /* Connection closed - can't write */
                fprintf(stderr, "tls_write: Connection closed\n");
                return -1;

            case SSL_ERROR_SYSCALL:
                /* Underlying network error */
                if (ERR_peek_error() == 0) {
                    /* EOF or connection reset */
                    if (ret == 0) {
                        fprintf(stderr, "tls_write: Unexpected EOF\n");
                    } else {
                        /* System call error - errno is set */
                        perror("tls_write: System call error");
                    }
                } else {
                    tls_print_errors("tls_write: System call error");
                }
                return -1;

            case SSL_ERROR_SSL:
                /* SSL protocol error */
                tls_print_errors("tls_write: Protocol error");
                return -1;

            default:
                tls_print_errors("tls_write: Unknown error");
                return -1;
        }
    }
}
