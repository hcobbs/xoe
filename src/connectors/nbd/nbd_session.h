/**
 * @file nbd_session.h
 * @brief NBD session state and handshake interface
 *
 * Manages NBD Fixed Newstyle handshake and per-connection session state.
 * Integrates with XOE server's handle_client() for protocol routing.
 *
 * [LLM-ASSISTED]
 */

#ifndef NBD_SESSION_H
#define NBD_SESSION_H

#include "lib/common/types.h"
#include "connectors/nbd/nbd_config.h"
#include "connectors/nbd/nbd_backend.h"

/**
 * NBD session state structure
 *
 * Tracks per-connection NBD session state.
 * Managed within handle_client() thread context.
 */
typedef struct {
    int client_socket;               /* Client socket FD */
    nbd_backend_t* backend;          /* Storage backend */
    nbd_config_t* config;            /* Configuration reference */
    int handshake_complete;          /* Handshake status flag */
    uint16_t transmission_flags;     /* NBD transmission flags */
    uint64_t export_size;            /* Export size in bytes */
    char export_name[NBD_NAME_MAX];  /* Negotiated export name */
} nbd_session_t;

/**
 * nbd_session_init - Initialize NBD session
 * @config: Configuration pointer
 * @backend: Backend pointer (already opened)
 * @client_socket: Client socket FD
 *
 * Allocates and initializes NBD session structure.
 *
 * Returns: Pointer to session on success, NULL on failure
 */
nbd_session_t* nbd_session_init(nbd_config_t* config,
                                nbd_backend_t* backend,
                                int client_socket);

/**
 * nbd_session_handshake - Perform NBD Fixed Newstyle handshake
 * @session: Session pointer
 *
 * Executes NBD Fixed Newstyle negotiation protocol:
 * 1. Send NBDMAGIC + IHAVEOPT
 * 2. Receive client flags
 * 3. Option negotiation (NBD_OPT_EXPORT_NAME)
 * 4. Send export info (size, flags)
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_session_handshake(nbd_session_t* session);

/**
 * nbd_session_handle_request - Handle single NBD request
 * @session: Session pointer
 *
 * Reads NBD request from socket and executes command:
 * - NBD_CMD_READ: Read from backend, send reply with data
 * - NBD_CMD_WRITE: Receive data, write to backend, send reply
 * - NBD_CMD_FLUSH: Flush backend, send reply
 * - NBD_CMD_DISC: Graceful disconnect
 * - NBD_CMD_TRIM: Trim backend, send reply
 *
 * Returns: 0 on success, E_IO_ERROR on disconnect, other negative on error
 */
int nbd_session_handle_request(nbd_session_t* session);

/**
 * nbd_session_cleanup - Clean up NBD session
 * @session: Pointer to session pointer (will be set to NULL)
 *
 * Frees session resources. Does NOT close backend or socket
 * (managed externally by server).
 */
void nbd_session_cleanup(nbd_session_t** session);

#endif /* NBD_SESSION_H */
