/**
 * @file state_server_nbd.c
 * @brief NBD (Network Block Device) server mode implementation
 *
 * Implements dedicated NBD server mode that exports storage backend
 * as network block device using NBD Fixed Newstyle protocol.
 *
 * [LLM-ASSISTED]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#include "core/config.h"
#include "lib/common/definitions.h"
#include "lib/net/net_resolve.h"
#include "connectors/nbd/nbd_config.h"
#include "connectors/nbd/nbd_backend.h"
#include "connectors/nbd/nbd_session.h"

/* Global shutdown flag for signal handling */
static volatile sig_atomic_t g_nbd_shutdown = 0;

/**
 * nbd_signal_handler - Signal handler for graceful NBD server shutdown
 * @signum: Signal number received
 *
 * Sets global shutdown flag when SIGINT or SIGTERM is received.
 */
static void nbd_signal_handler(int signum) {
    (void)signum;
    g_nbd_shutdown = 1;
}

/**
 * state_server_nbd - Execute NBD server mode operation
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_CLEANUP when server exits
 *
 * NBD server mode operation:
 * 1. Open backend storage (file/zvol/device)
 * 2. Create and bind server socket
 * 3. Listen for NBD client connection
 * 4. Accept connection and perform NBD handshake
 * 5. Enter request/response loop until disconnect
 * 6. Return to listening for next connection
 *
 * The server runs until interrupted by signal.
 */
xoe_state_t state_server_nbd(xoe_config_t *config) {
    int server_fd = 0;
    int client_socket = 0;
    struct sockaddr_in address = {0};
    int addrlen = sizeof(address);
    nbd_config_t *nbd_config = NULL;
    nbd_backend_t *nbd_backend = NULL;
    nbd_session_t *nbd_session = NULL;
    int result = 0;
    char client_ip[INET_ADDRSTRLEN] = {0};
    struct sockaddr_in client_addr = {0};

    printf("Starting NBD server mode...\n");

    /* Retrieve NBD configuration */
    nbd_config = (nbd_config_t *)config->nbd_config;
    if (nbd_config == NULL) {
        fprintf(stderr, "NBD configuration not initialized\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Open backend storage */
    nbd_backend = (nbd_backend_t *)config->nbd_backend;
    if (nbd_backend == NULL) {
        nbd_backend = (nbd_backend_t *)malloc(sizeof(nbd_backend_t));
        if (nbd_backend == NULL) {
            fprintf(stderr, "Failed to allocate NBD backend\n");
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }

        result = nbd_backend_open(nbd_backend, nbd_config->export_path,
                                  nbd_config->backend_type,
                                  (nbd_config->capabilities & NBD_CAP_READONLY));
        if (result != SUCCESS) {
            fprintf(stderr, "Failed to open NBD backend: %s\n",
                    nbd_config->export_path);
            free(nbd_backend);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }

        config->nbd_backend = nbd_backend;
    }

    /* Create server socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        nbd_backend_close(nbd_backend);
        free(nbd_backend);
        config->nbd_backend = NULL;
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Set SO_REUSEADDR to allow quick restart */
    {
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                       sizeof(opt)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
        }
    }

    /* Bind socket to address */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((uint16_t)config->listen_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        nbd_backend_close(nbd_backend);
        free(nbd_backend);
        config->nbd_backend = NULL;
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Start listening */
    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        close(server_fd);
        nbd_backend_close(nbd_backend);
        free(nbd_backend);
        config->nbd_backend = NULL;
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    printf("NBD server listening on port %d\n", config->listen_port);
    printf("Exporting: %s (%llu bytes, %s)\n",
           nbd_config->export_path,
           (unsigned long long)nbd_backend_get_size(nbd_backend),
           nbd_backend_is_read_only(nbd_backend) ? "read-only" : "read-write");

    /* Install signal handler for graceful shutdown */
    signal(SIGINT, nbd_signal_handler);
    signal(SIGTERM, nbd_signal_handler);

    /* Main server loop: accept connections and handle NBD protocol */
    while (!g_nbd_shutdown) {
        addrlen = sizeof(client_addr);
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr,
                               (socklen_t *)&addrlen);

        if (client_socket < 0) {
            if (g_nbd_shutdown) {
                break;
            }
            perror("Accept failed");
            continue;
        }

        /* Convert client IP to string */
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("NBD client connected from %s:%d\n",
               client_ip, ntohs(client_addr.sin_port));

        /* Initialize NBD session for this connection */
        nbd_session = nbd_session_init(nbd_config, nbd_backend, client_socket);
        if (nbd_session == NULL) {
            fprintf(stderr, "Failed to initialize NBD session\n");
            close(client_socket);
            continue;
        }

        /* Perform NBD Fixed Newstyle handshake */
        result = nbd_session_handshake(nbd_session);
        if (result != SUCCESS) {
            fprintf(stderr, "NBD handshake failed with %s:%d\n",
                    client_ip, ntohs(client_addr.sin_port));
            nbd_session_cleanup(&nbd_session);
            close(client_socket);
            continue;
        }

        /* Handle NBD requests until disconnect */
        while (!g_nbd_shutdown) {
            result = nbd_session_handle_request(nbd_session);
            if (result == E_IO_ERROR) {
                /* Normal disconnect */
                printf("NBD client %s:%d disconnected\n",
                       client_ip, ntohs(client_addr.sin_port));
                break;
            } else if (result != SUCCESS) {
                fprintf(stderr, "NBD request error from %s:%d: %d\n",
                        client_ip, ntohs(client_addr.sin_port), result);
                break;
            }
        }

        /* Cleanup session and close connection */
        nbd_session_cleanup(&nbd_session);
        close(client_socket);
        printf("NBD session with %s:%d closed\n",
               client_ip, ntohs(client_addr.sin_port));
    }

    /* Cleanup */
    printf("Shutting down NBD server...\n");
    close(server_fd);
    nbd_backend_close(nbd_backend);
    free(nbd_backend);
    config->nbd_backend = NULL;

    return STATE_CLEANUP;
}
