#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

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

/* Serial connector includes */
#include "serial_config.h"
#include "serial_port.h"
#include "serial_protocol.h"
#include "serial_client.h"

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

/* Global TLS context (read-only after initialization, thread-safe) */
#if TLS_ENABLED
SSL_CTX* g_tls_ctx = NULL;
#endif

/* Acquire a client slot from the pool */
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

/* Release a client slot back to the pool */
void release_client_slot(client_info_t *slot) {
    if (slot != NULL) {
        pthread_mutex_lock(&pool_mutex);
        slot->in_use = 0;
        slot->client_socket = -1;
        pthread_mutex_unlock(&pool_mutex);
    }
}

/* Initialize the client pool */
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
    char client_ip[INET_ADDRSTRLEN];
    int bytes_received;

#if TLS_ENABLED
    SSL* tls = NULL;
#endif

    /* Convert client IP to string (thread-safe) */
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Connection accepted from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

#if TLS_ENABLED
    /* Perform TLS handshake (if encryption enabled) */
    if (g_tls_ctx != NULL) {
        tls = tls_session_create(g_tls_ctx, client_socket);
        if (tls == NULL) {
            fprintf(stderr, "TLS handshake failed with %s:%d: %s\n",
                    client_ip, ntohs(client_addr.sin_port),
                    tls_get_error_string());
            goto cleanup;
        }
        client_info->tls_session = tls;
        printf("TLS handshake successful with %s:%d\n",
               client_ip, ntohs(client_addr.sin_port));
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
        printf("Received from %s:%d: %s", client_ip, ntohs(client_addr.sin_port), buffer);

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
        printf("Client %s:%d disconnected\n", client_ip, ntohs(client_addr.sin_port));
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
    printf("Serial Connector Options (requires -c for client mode):\n");
    printf("  -s <device>       Serial device path (e.g., /dev/ttyUSB0)\n");
    printf("                    Enables serial-to-network bridging\n\n");
    printf("  -b <baud>         Baud rate (default: 9600)\n");
    printf("                    Common rates: 9600, 19200, 38400, 57600, 115200\n\n");
    printf("  --parity <mode>   Parity (default: none)\n");
    printf("                    Options: none, even, odd\n\n");
    printf("  --databits <n>    Data bits (default: 8)\n");
    printf("                    Options: 7, 8\n\n");
    printf("  --stopbits <n>    Stop bits (default: 1)\n");
    printf("                    Options: 1, 2\n\n");
    printf("  --flow <mode>     Flow control (default: none)\n");
    printf("                    Options: none, xonxoff, rtscts\n\n");
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
    printf("  %s -c 192.168.1.100:12345 -s /dev/ttyUSB0 -b 115200\n", prog_name);
    printf("                                      # Serial bridge at 115200 baud\n");
}

/**
 * main - XOE application entry point with FSM
 * @argc: Argument count
 * @argv: Argument vector
 *
 * Returns: Exit code from configuration (EXIT_SUCCESS or EXIT_FAILURE)
 *
 * Implements a finite state machine to manage application flow:
 * - INIT: Initialize configuration with defaults
 * - PARSE_ARGS: Parse command-line arguments
 * - VALIDATE_CONFIG: Validate configuration settings
 * - MODE_SELECT: Determine operating mode
 * - SERVER_MODE/CLIENT_STD/CLIENT_SERIAL: Execute mode-specific logic
 * - CLEANUP: Release resources
 * - EXIT: Terminal state
 */
int main(int argc, char *argv[]) {
    xoe_config_t config;
    xoe_state_t state = STATE_INIT;

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

            case STATE_CLIENT_SERIAL:
                state = state_client_serial(&config);
                break;

            case STATE_CLEANUP:
                state = state_cleanup(&config);
                break;

            default:
                /* Unknown state - exit with failure */
                config.exit_code = EXIT_FAILURE;
                state = STATE_EXIT;
                break;
        }
    }

    return config.exit_code;
}
