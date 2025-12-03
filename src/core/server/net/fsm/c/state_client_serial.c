/**
 * state_client_serial.c
 *
 * Implements serial bridge client mode that connects a serial port to the
 * network server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../h/xoe.h"
#include "../../../../../common/h/commonDefinitions.h"
#include "../../../../../connector/serial/h/serial_config.h"
#include "../../../../../connector/serial/h/serial_client.h"

/* Global state for signal handler (C89 compatible) */
static volatile sig_atomic_t g_shutdown_requested = 0;
static serial_client_t* g_serial_client_ptr = NULL;

/**
 * signal_handler - Handle SIGINT and SIGTERM for graceful shutdown
 * @signum: Signal number received
 *
 * This handler sets a flag to request shutdown and notifies the serial
 * client to begin its cleanup process. The main thread will detect the
 * flag and perform orderly cleanup.
 *
 * Note: Only async-signal-safe operations are allowed in signal handlers.
 * We only set a flag and call a library function marked as async-safe.
 */
static void signal_handler(int signum) {
    (void)signum; /* Suppress unused parameter warning */
    g_shutdown_requested = 1;
    if (g_serial_client_ptr != NULL) {
        serial_client_request_shutdown(g_serial_client_ptr);
    }
}

/**
 * state_client_serial - Execute serial bridge client mode
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_CLEANUP when client exits
 *
 * Serial bridge client mode operation:
 * 1. Create socket and connect to server
 * 2. Initialize serial port with configured parameters
 * 3. Start bidirectional I/O threads:
 *    - Serial-to-network thread
 *    - Network-to-serial thread
 * 4. Wait for shutdown signal (Ctrl+C)
 * 5. Stop threads and cleanup
 *
 * This mode bridges a local serial port to a remote network server,
 * allowing serial communication over TCP/IP.
 */
xoe_state_t state_client_serial(xoe_config_t *config) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    serial_client_t* serial_client;
    serial_config_t *serial_cfg = (serial_config_t*)config->serial_config;
    int result;

    /* Create socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Setup server address structure */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config->connect_server_port);

    /* Convert IP address from text to binary */
    if (inet_pton(AF_INET, config->connect_server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        close(sock);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Connect to server */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    printf("Connected to server %s:%d\n",
           config->connect_server_ip, config->connect_server_port);

    if (serial_cfg == NULL) {
        fprintf(stderr, "Serial configuration not initialized\n");
        close(sock);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    printf("Serial mode enabled: %s at %d baud\n",
           serial_cfg->device_path, serial_cfg->baud_rate);

    /* Initialize serial client */
    serial_client = serial_client_init(serial_cfg, sock);
    if (serial_client == NULL) {
        fprintf(stderr, "Failed to initialize serial client\n");
        close(sock);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    printf("Serial port opened successfully\n");

    /* Install signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    g_serial_client_ptr = serial_client;
    g_shutdown_requested = 0;

    /* Start I/O threads */
    result = serial_client_start(serial_client);
    if (result != 0) {
        fprintf(stderr, "Failed to start serial client threads: %d\n", result);
        serial_client_cleanup(&serial_client);
        close(sock);
        config->exit_code = EXIT_FAILURE;
        g_serial_client_ptr = NULL;
        return STATE_CLEANUP;
    }

    printf("Serial bridge active (Ctrl+C to exit)\n");

    /* Main thread waits for shutdown signal (Ctrl+C or SIGTERM) */
    while (!g_shutdown_requested && !serial_client_should_shutdown(serial_client)) {
        sleep(1);
    }

    /* Clear global pointer before cleanup */
    g_serial_client_ptr = NULL;

    /* Stop threads and cleanup */
    printf("\nShutting down serial bridge...\n");
    serial_client_stop(serial_client);
    serial_client_cleanup(&serial_client);
    printf("Serial port closed\n");

    close(sock);
    printf("Client disconnected.\n");

    config->exit_code = EXIT_SUCCESS;
    return STATE_CLEANUP;
}
