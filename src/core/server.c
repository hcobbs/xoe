/**
 * server.c
 *
 * Server implementation for XOE including client pool management and
 * per-client thread handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "lib/common/definitions.h"
#include "core/config.h"
#include "core/server.h"

/* TLS includes - include config first to get TLS_ENABLED */
#include "lib/security/tls_config.h"
#if TLS_ENABLED
#include "lib/security/tls_context.h"
#include "lib/security/tls_session.h"
#include "lib/security/tls_io.h"
#include "lib/security/tls_error.h"
#endif

/* USB server includes */
#include "connectors/usb/usb_server.h"
#include "lib/protocol/protocol.h"

/* Global fixed-size client pool */
static client_info_t client_pool[MAX_CLIENTS];
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global TLS context (read-only after initialization, thread-safe) */
#if TLS_ENABLED
SSL_CTX* g_tls_ctx = NULL;
#endif

/* Global USB server (NULL if not initialized) */
usb_server_t* g_usb_server = NULL;

/**
 * acquire_client_slot - Acquire a client slot from the pool
 *
 * Returns: Pointer to available client_info_t slot, or NULL if pool is full
 */
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

/**
 * release_client_slot - Release a client slot back to the pool
 * @slot: Pointer to client slot to release
 */
void release_client_slot(client_info_t *slot) {
    if (slot != NULL) {
        pthread_mutex_lock(&pool_mutex);
        slot->in_use = 0;
        slot->client_socket = -1;
        pthread_mutex_unlock(&pool_mutex);
    }
}

/**
 * init_client_pool - Initialize the global client pool
 */
void init_client_pool(void) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        client_pool[i].in_use = 0;
        client_pool[i].client_socket = -1;
#if TLS_ENABLED
        client_pool[i].tls_session = NULL;
#endif
    }
}

/**
 * handle_client - Thread function to handle a single client connection
 * @arg: Pointer to client_info_t structure (void* for pthread compatibility)
 *
 * Returns: NULL (pthread thread function requirement)
 *
 * Handles bidirectional communication with a client, including optional
 * TLS encryption. This function runs in a detached thread for each
 * connected client.
 */
void *handle_client(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_addr = client_info->client_addr;
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

    /* Protocol handling loop - detect and route based on protocol */
    while (1) {
        xoe_packet_t packet;

#if TLS_ENABLED
        if (tls != NULL) {
            bytes_received = tls_read(tls, (char*)&packet, sizeof(xoe_packet_t));
        } else {
            bytes_received = recv(client_socket, &packet, sizeof(xoe_packet_t), 0);
        }
#else
        bytes_received = recv(client_socket, &packet, sizeof(xoe_packet_t), 0);
#endif
        if (bytes_received <= 0) {
            break;
        }

        /* Check if this is a USB protocol packet */
        if (bytes_received == sizeof(xoe_packet_t) &&
            packet.protocol_id == XOE_PROTOCOL_USB) {

            /* Route via USB server if available */
            if (g_usb_server != NULL) {
                int result = usb_server_handle_urb(g_usb_server, &packet,
                                                   client_socket);
                if (result != 0) {
                    fprintf(stderr, "USB routing error from %s:%d: %d\n",
                            client_ip, ntohs(client_addr.sin_port), result);
                }
            } else {
                fprintf(stderr, "USB packet received but USB server not initialized\n");
            }
        } else {
            /* Legacy echo mode for non-USB packets */
            printf("Received from %s:%d (%d bytes)\n",
                   client_ip, ntohs(client_addr.sin_port), bytes_received);

#if TLS_ENABLED
            if (tls != NULL) {
                if (tls_write(tls, (char*)&packet, bytes_received) <= 0) {
                    fprintf(stderr, "TLS write failed\n");
                    break;
                }
            } else {
                if (send(client_socket, &packet, bytes_received, 0) < 0) {
                    perror("send failed");
                    break;
                }
            }
#else
            if (send(client_socket, &packet, bytes_received, 0) < 0) {
                perror("send failed");
                break;
            }
#endif
        }
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
    /* Unregister client from USB server if initialized */
    if (g_usb_server != NULL) {
        usb_server_unregister_client(g_usb_server, client_socket);
    }

#if TLS_ENABLED
    if (tls != NULL) {
        tls_session_shutdown(tls);
        tls_session_destroy(tls);
        /* Only shutdown socket if TLS was successful */
        shutdown(client_socket, SHUT_RDWR);
    }
#endif

    close(client_socket);
    release_client_slot(client_info);
    pthread_exit(NULL);
}

/**
 * disconnect_all_clients - Close all active client connections
 *
 * Closes sockets for all clients currently in the pool. Threads will
 * exit when they detect the closed socket.
 */
void disconnect_all_clients(void) {
    int i;
    int disconnected = 0;

    pthread_mutex_lock(&pool_mutex);
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (client_pool[i].in_use && client_pool[i].client_socket != -1) {
#if TLS_ENABLED
            /* Clean up TLS session if present */
            if (client_pool[i].tls_session != NULL) {
                tls_session_shutdown(client_pool[i].tls_session);
                tls_session_destroy(client_pool[i].tls_session);
                client_pool[i].tls_session = NULL;
            }
#endif
            /* Close the socket (will cause thread to exit) */
            close(client_pool[i].client_socket);
            client_pool[i].client_socket = -1;
            disconnected++;
        }
    }
    pthread_mutex_unlock(&pool_mutex);

    if (disconnected > 0) {
        printf("Disconnected %d client(s)\n", disconnected);
    }
}

/**
 * wait_for_clients - Wait for all client threads to exit
 * @timeout_sec: Maximum seconds to wait
 *
 * Returns: Number of clients still active after timeout
 */
int wait_for_clients(int timeout_sec) {
    int elapsed = 0;
    int active = 0;
    int i;

    while (elapsed < timeout_sec) {
        active = 0;

        pthread_mutex_lock(&pool_mutex);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_pool[i].in_use) {
                active++;
            }
        }
        pthread_mutex_unlock(&pool_mutex);

        if (active == 0) {
            return 0;  /* All clients exited */
        }

        sleep(1);
        elapsed++;
    }

    /* Timeout reached, force clear any remaining slots */
    if (active > 0) {
        printf("Warning: %d client(s) still active after %d second timeout, force-clearing\n",
               active, timeout_sec);
        pthread_mutex_lock(&pool_mutex);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_pool[i].in_use) {
                client_pool[i].in_use = 0;
                client_pool[i].client_socket = -1;
#if TLS_ENABLED
                client_pool[i].tls_session = NULL;
#endif
            }
        }
        pthread_mutex_unlock(&pool_mutex);
    }

    return active;
}

/**
 * get_active_client_count - Get count of currently active clients
 *
 * Returns: Number of client slots currently in use
 */
int get_active_client_count(void) {
    int count = 0;
    int i;

    pthread_mutex_lock(&pool_mutex);
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (client_pool[i].in_use) {
            count++;
        }
    }
    pthread_mutex_unlock(&pool_mutex);

    return count;
}

/**
 * print_usage - Print usage information
 * @prog_name: Program name from argv[0]
 *
 * Displays command-line usage and examples for the XOE application.
 */
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
