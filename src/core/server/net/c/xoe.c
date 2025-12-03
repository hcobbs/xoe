#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* POSIX/Unix headers only - Windows support removed */
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 *  Definitions needed by Xoe only... Standard C libraries should be cross-compile
 *  safe and appropriately included above
 */
#include "commonDefinitions.h"
#include "xoe.h"

/* TLS includes - include config first to get TLS_ENABLED */
#include "tls_config.h"
#if TLS_ENABLED
#include "tls_context.h"
#include "tls_session.h"
#include "tls_io.h"
#include "tls_error.h"
#endif

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
    int in_use;
#if TLS_ENABLED
    SSL* tls_session;
#endif
} client_info_t;

/* Global fixed-size client pool */
static client_info_t client_pool[MAX_CLIENTS];
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global TLS context (read-only after initialization, thread-safe, non-static for FSM use) */
#if TLS_ENABLED
SSL_CTX* g_tls_ctx = NULL;
#endif

/* Acquire a client slot from the pool (non-static for FSM use) */
client_info_t* acquire_client_slot(void) {
    int i;
    client_info_t *slot = NULL;

    pthread_mutex_lock(&pool_mutex);
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (!client_pool[i].in_use) {
            client_pool[i].in_use = 1;
            slot = &client_pool[i];
            break;
        }
    }
    pthread_mutex_unlock(&pool_mutex);

    return slot;
}

/* Release a client slot back to the pool (non-static for FSM use) */
void release_client_slot(client_info_t *slot) {
    if (slot != NULL) {
        pthread_mutex_lock(&pool_mutex);
        slot->in_use = 0;
        slot->client_socket = -1;
        pthread_mutex_unlock(&pool_mutex);
    }
}

/* Initialize the client pool (non-static for FSM use) */
void init_client_pool(void) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        client_pool[i].in_use = 0;
        client_pool[i].client_socket = -1;
    }
}

void *handle_client(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_addr = client_info->client_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

#if TLS_ENABLED
    SSL* tls = NULL;
#endif

    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

#if TLS_ENABLED
    /* Perform TLS handshake (if encryption enabled) */
    if (g_tls_ctx != NULL) {
        tls = tls_session_create(g_tls_ctx, client_socket);
        if (tls == NULL) {
            fprintf(stderr, "TLS handshake failed with %s:%d: %s\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                    tls_get_error_string());
            goto cleanup;
        }
        client_info->tls_session = tls;
        printf("TLS handshake successful with %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }
#endif

    /* Echo loop - use TLS or plain TCP based on runtime mode */
    while (1) {
#if TLS_ENABLED
        if (tls != NULL) {
            bytes_received = tls_read(tls, buffer, BUFFER_SIZE - 1);
        } else {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        }
#else
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
#endif
        if (bytes_received <= 0) {
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Received from %s:%d: %s", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

#if TLS_ENABLED
        if (tls != NULL) {
            if (tls_write(tls, buffer, bytes_received) <= 0) {
                fprintf(stderr, "TLS write failed\n");
                break;
            }
        } else {
            send(client_socket, buffer, bytes_received, 0);
        }
#else
        send(client_socket, buffer, bytes_received, 0);
#endif
    }

    if (bytes_received == 0) {
        printf("Client %s:%d disconnected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    } else if (bytes_received == -1) {
#if TLS_ENABLED
        if (tls != NULL) {
            fprintf(stderr, "TLS read error: %s\n", tls_get_error_string());
        } else {
            perror("recv failed");
        }
#else
        perror("recv failed");
#endif
    }

cleanup:
#if TLS_ENABLED
    if (tls != NULL) {
        tls_session_shutdown(tls);
        tls_session_destroy(tls);
    }
#endif

    close(client_socket);
    release_client_slot(client_info);
    pthread_exit(NULL);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Server Mode Options:\n");
    printf("  -i <interface>    Network interface to bind (default: 0.0.0.0, all interfaces)\n");
    printf("                    Examples: 127.0.0.1, eth0, 192.168.1.100\n\n");
    printf("  -p <port>         Port to listen on (default: %d)\n", SERVER_PORT);
    printf("                    Range: 1-65535\n\n");
#if TLS_ENABLED
    printf("  -e <mode>         Encryption mode (default: none)\n");
    printf("                    none  - Plain TCP (no encryption)\n");
    printf("                    tls12 - TLS 1.2 encryption\n");
    printf("                    tls13 - TLS 1.3 encryption (recommended)\n\n");
    printf("  -cert <path>      Path to server certificate (default: %s)\n", TLS_DEFAULT_CERT_FILE);
    printf("                    Required for TLS modes\n\n");
    printf("  -key <path>       Path to server private key (default: %s)\n", TLS_DEFAULT_KEY_FILE);
    printf("                    Required for TLS modes\n\n");
#endif
    printf("Client Mode Options:\n");
    printf("  -c <ip>:<port>    Connect to server as client\n");
    printf("                    Example: -c 192.168.1.100:12345\n\n");
    printf("General Options:\n");
    printf("  -h                Show this help message\n\n");
    printf("Examples:\n");
#if TLS_ENABLED
    printf("  %s -e none                          # Plain TCP server\n", prog_name);
    printf("  %s -e tls13 -p 8443                 # TLS 1.3 server on port 8443\n", prog_name);
#else
    printf("  %s -p 12345                         # TCP server on port 12345\n", prog_name);
#endif
    printf("  %s -c 127.0.0.1:12345               # Connect as client\n", prog_name);
}

/**
 * @brief Main entry point - FSM-based architecture
 *
 * Implements a finite state machine to manage application flow.
 * Each state is implemented in a separate file under fsm/c/ for modularity.
 * This approach improves code organization, maintainability, and testability.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code (EXIT_SUCCESS or EXIT_FAILURE)
 */
int main(int argc, char *argv[]) {
    xoe_config_t config;
    xoe_state_t state = STATE_INIT;

    /* FSM loop - execute states until STATE_EXIT is reached */
    while (state != STATE_EXIT) {
        switch (state) {
            case STATE_INIT:
                state = state_init(&config);
                break;

            case STATE_PARSE_ARGS:
                state = state_parse_args(&config, argc, argv);
                break;

            case STATE_VALIDATE_CONFIG:
                state = state_validate_config(&config);
                break;

            case STATE_MODE_SELECT:
                state = state_mode_select(&config);
                break;

            case STATE_SERVER_MODE:
                state = state_server_mode(&config);
                break;

            case STATE_CLIENT_STD:
                state = state_client_std(&config);
                break;

            case STATE_CLEANUP:
                state = state_cleanup(&config);
                break;

            default:
                /* Invalid state - should never happen */
                fprintf(stderr, "Error: Invalid state %d\n", state);
                config.exit_code = EXIT_FAILURE;
                state = STATE_EXIT;
                break;
        }
    }

    return config.exit_code;
}
