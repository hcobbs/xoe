/**
 * state_client_std.c
 *
 * Implements standard client mode with interactive stdin/stdout communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "core/config.h"
#include "lib/common/definitions.h"
#include "lib/net/net_resolve.h"

#if TLS_ENABLED
#include "lib/security/tls_config.h"
#include "lib/security/tls_context.h"
#include "lib/security/tls_session.h"
#include "lib/security/tls_io.h"
#endif

/**
 * state_client_std - Execute standard client mode
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_CLEANUP when client exits
 *
 * Standard client mode operation:
 * 1. Create socket and connect to server
 * 2. Interactive loop:
 *    - Read line from stdin
 *    - Send to server
 *    - Receive response
 *    - Display response
 * 3. Exit on "exit" command or EOF
 *
 * This is the interactive stdin/stdout client mode (not serial bridge).
 */
xoe_state_t state_client_std(xoe_config_t *config) {
    int sock = -1;
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = 0;
    net_resolve_result_t resolve_result;
    char error_buf[256];
#if TLS_ENABLED
    SSL_CTX* tls_ctx = NULL;
    SSL* tls = NULL;
#endif

    /* Resolve hostname/IP and connect to server */
    if (net_resolve_connect(config->connect_server_ip,
                            config->connect_server_port,
                            &sock, &resolve_result) != 0) {
        net_resolve_format_error(&resolve_result, error_buf, sizeof(error_buf));
        fprintf(stderr, "Failed to connect to %s:%d: %s\n",
                config->connect_server_ip, config->connect_server_port, error_buf);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

#if TLS_ENABLED
    /* Initialize TLS if encryption is enabled */
    if (config->encryption_mode == ENCRYPT_TLS12 || config->encryption_mode == ENCRYPT_TLS13) {
        tls_ctx = tls_context_init_client(config->encryption_mode);
        if (tls_ctx == NULL) {
            fprintf(stderr, "Failed to initialize TLS client context\n");
            close(sock);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }

        tls = tls_session_create_client(tls_ctx, sock);
        if (tls == NULL) {
            struct linger linger_opt;
            fprintf(stderr, "TLS handshake failed\n");
            tls_context_cleanup(tls_ctx);
            /* Set SO_LINGER to 0 to avoid blocking on close() after failed handshake */
            linger_opt.l_onoff = 1;
            linger_opt.l_linger = 0;
            setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
            close(sock);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }
#endif

    printf("Connected to server %s:%d\n",
           config->connect_server_ip, config->connect_server_port);
    printf("Enter messages to send (type 'exit' to quit):\n");

    /* Interactive stdin/stdout loop */
    while (TRUE) {
        printf("> ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break; /* EOF or error */
        }

        /* Remove newline character if present */
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

#if TLS_ENABLED
        if (tls != NULL) {
            /* Send via TLS */
            if (tls_write(tls, buffer, strlen(buffer)) <= 0) {
                fprintf(stderr, "TLS send failed\n");
                config->exit_code = EXIT_FAILURE;
                break;
            }

            /* Receive via TLS */
            bytes_received = tls_read(tls, buffer, BUFFER_SIZE - 1);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("Server response: %s\n", buffer);
            } else if (bytes_received == 0) {
                printf("Server disconnected.\n");
                config->exit_code = EXIT_FAILURE;
                break;
            } else {
                fprintf(stderr, "TLS recv failed\n");
                config->exit_code = EXIT_FAILURE;
                break;
            }
        } else
#endif
        {
            /* Send message to server (plain TCP) */
            send(sock, buffer, strlen(buffer), 0);

            /* Receive response (plain TCP) */
            bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                printf("Server response: %s\n", buffer);
            } else if (bytes_received == 0) {
                printf("Server disconnected.\n");
                break;
            } else {
                perror("recv failed");
                config->exit_code = EXIT_FAILURE;
                break;
            }
        }
    }

#if TLS_ENABLED
    /* Cleanup TLS */
    if (tls != NULL) {
        tls_session_shutdown(tls);
        tls_session_destroy(tls);
    }
    if (tls_ctx != NULL) {
        tls_context_cleanup(tls_ctx);
    }
#endif

    close(sock);
    printf("Client disconnected.\n");

    /* Only set SUCCESS if we haven't already set FAILURE */
    if (config->exit_code != EXIT_FAILURE) {
        config->exit_code = EXIT_SUCCESS;
    }
    return STATE_CLEANUP;
}
