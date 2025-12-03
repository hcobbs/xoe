/**
 * state_server_mode.c
 *
 * Implements the TCP/TLS server mode with multi-threaded client handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../h/xoe.h"
#include "../../../../../common/h/commonDefinitions.h"

#if TLS_ENABLED
#include "../../security/h/tls_config.h"
#include "../../security/h/tls_context.h"
#include "../../security/h/tls_error.h"
#endif

/* Forward declarations for helper functions from xoe.c */
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
    int in_use;
#if TLS_ENABLED
    SSL* tls_session;
#endif
} client_info_t;

extern client_info_t* acquire_client_slot(void);
extern void release_client_slot(client_info_t *slot);
extern void init_client_pool(void);
extern void *handle_client(void *arg);

#if TLS_ENABLED
extern SSL_CTX* g_tls_ctx;
#endif

/**
 * state_server_mode - Execute server mode operation
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_CLEANUP when server exits
 *
 * Server mode operation:
 * 1. Initialize TLS context if encryption is enabled
 * 2. Create and bind server socket
 * 3. Listen for incoming connections
 * 4. Accept connections and spawn handler threads
 * 5. Manage client pool to limit concurrent connections
 *
 * The server runs in an infinite loop until interrupted.
 */
xoe_state_t state_server_mode(xoe_config_t *config) {
    int server_fd = 0;
    int new_socket = 0;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    client_info_t *client_info = NULL;
    pthread_t thread_id;

#if TLS_ENABLED
    /* Initialize TLS context before accepting connections (if encryption enabled) */
    if (config->encryption_mode != ENCRYPT_NONE) {
        g_tls_ctx = tls_context_init(config->cert_path, config->key_path,
                                      config->encryption_mode);
        if (g_tls_ctx == NULL) {
            fprintf(stderr, "Failed to initialize TLS: %s\n", tls_get_error_string());
            fprintf(stderr, "Make sure certificates exist at:\n");
            fprintf(stderr, "  %s\n", config->cert_path);
            fprintf(stderr, "  %s\n", config->key_path);
            fprintf(stderr, "Run './scripts/generate_test_certs.sh' to generate test certificates.\n");
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
        if (config->encryption_mode == ENCRYPT_TLS12) {
            printf("TLS 1.2 enabled\n");
        } else {
            printf("TLS 1.3 enabled\n");
        }
    } else {
        printf("Running in plain TCP mode (no encryption)\n");
    }
#endif

    /* Create socket file descriptor */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Set up address structure */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; /* Default to all interfaces */
    if (config->listen_address != NULL) {
        if (inet_pton(AF_INET, config->listen_address, &address.sin_addr) <= 0) {
            fprintf(stderr, "Invalid listen address: %s\n", config->listen_address);
            close(server_fd);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }
    address.sin_port = htons(config->listen_port);

    /* Bind the socket to the specified IP and port */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Listen for incoming connections */
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Initialize the client pool */
    init_client_pool();

    printf("Server listening on %s:%d\n",
           (config->listen_address == NULL) ? "0.0.0.0" : config->listen_address,
           config->listen_port);

    /* Main accept loop */
    while (TRUE) {
        client_info = acquire_client_slot();
        if (client_info == NULL) {
            fprintf(stderr, "Max clients (%d) reached, rejecting connection\n", MAX_CLIENTS);
            /* Still need to accept and close to prevent backlog */
            new_socket = accept(server_fd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen);
            if (new_socket >= 0) {
                close(new_socket);
            }
            continue;
        }

        new_socket = accept(server_fd,
                            (struct sockaddr *)&client_info->client_addr,
                            (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            release_client_slot(client_info);
            continue;
        }
        client_info->client_socket = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) != 0) {
            perror("pthread_create failed");
            close(new_socket);
            release_client_slot(client_info);
        }
        pthread_detach(thread_id); /* Detach thread to clean up resources automatically */
    }

#if TLS_ENABLED
    /* Cleanup TLS context on shutdown */
    tls_context_cleanup(g_tls_ctx);
#endif

    close(server_fd);
    return STATE_CLEANUP;
}
