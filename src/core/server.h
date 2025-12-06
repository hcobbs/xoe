#ifndef CORE_SERVER_H
#define CORE_SERVER_H

#include <netinet/in.h>
#include "lib/security/tls_config.h"

#if TLS_ENABLED
#include <openssl/ssl.h>
#endif

/**
 * client_info_t - Structure to hold client connection information
 *
 * Used by the server to manage multiple concurrent client connections
 * in a fixed-size pool.
 */
typedef struct {
    int client_socket;              /* Client socket file descriptor */
    struct sockaddr_in client_addr; /* Client address information */
    int in_use;                     /* Pool slot in-use flag */
#if TLS_ENABLED
    SSL* tls_session;               /* TLS session for encrypted connections */
#endif
} client_info_t;

/* Forward declaration for USB server (incomplete type) */
struct usb_server_t;

/* Global TLS context (declared extern for use in state handlers) */
#if TLS_ENABLED
extern SSL_CTX* g_tls_ctx;
#endif

/* Global USB server (declared extern for use in state handlers) */
extern struct usb_server_t* g_usb_server;

/**
 * acquire_client_slot - Acquire a client slot from the pool
 *
 * Returns: Pointer to available client_info_t slot, or NULL if pool is full
 *
 * Thread-safe function to acquire a client connection slot from the
 * fixed-size global client pool.
 */
client_info_t* acquire_client_slot(void);

/**
 * release_client_slot - Release a client slot back to the pool
 * @slot: Pointer to client slot to release
 *
 * Thread-safe function to release a client connection slot back to
 * the global pool for reuse.
 */
void release_client_slot(client_info_t *slot);

/**
 * init_client_pool - Initialize the global client pool
 *
 * Must be called before accepting any client connections. Initializes
 * all slots in the pool to an available state.
 */
void init_client_pool(void);

/**
 * handle_client - Thread function to handle a single client connection
 * @arg: Pointer to client_info_t structure (void* for pthread compatibility)
 *
 * Returns: NULL (pthread thread function requirement)
 *
 * Handles bidirectional communication with a client, including optional
 * TLS encryption. Runs in a detached thread, one per client connection.
 */
void *handle_client(void *arg);

/**
 * disconnect_all_clients - Close all active client connections
 *
 * Closes sockets for all clients currently in the pool. Threads will
 * exit when they detect the closed socket. Does NOT wait for threads
 * to exit - use wait_for_clients() for that.
 *
 * Thread-safe: Uses pool mutex for protection.
 */
void disconnect_all_clients(void);

/**
 * wait_for_clients - Wait for all client threads to exit
 * @timeout_sec: Maximum seconds to wait (0 = no wait, just check)
 *
 * Returns: Number of clients still active after timeout
 *
 * Polls the client pool for up to timeout_sec seconds, waiting for
 * all in_use flags to become 0 (indicating threads have exited).
 * Returns early if all clients disconnect before timeout.
 */
int wait_for_clients(int timeout_sec);

/**
 * get_active_client_count - Get count of currently active clients
 *
 * Returns: Number of client slots currently in use
 *
 * Thread-safe function to count active client connections.
 */
int get_active_client_count(void);

#endif /* CORE_SERVER_H */
