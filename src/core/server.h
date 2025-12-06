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

/* Forward declaration for USB server */
typedef struct usb_server_t usb_server_t;

/* Global TLS context (declared extern for use in state handlers) */
#if TLS_ENABLED
extern SSL_CTX* g_tls_ctx;
#endif

/* Global USB server (declared extern for use in state handlers) */
extern usb_server_t* g_usb_server;

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

#endif /* CORE_SERVER_H */
