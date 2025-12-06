#ifndef CORE_MGMT_SERVER_H
#define CORE_MGMT_SERVER_H

#include "core/config.h"

/**
 * Management Server
 *
 * TCP-based management interface for runtime configuration and control.
 * Listens on port 6969 (configurable), supports optional password auth,
 * and handles up to MAX_MGMT_SESSIONS concurrent sessions.
 *
 * Protocol: Line-oriented text commands (telnet-compatible)
 * Threading: Dedicated listener thread + per-session handler threads
 */

/* Opaque server structure (implementation in mgmt_server.c) */
typedef struct mgmt_server_t mgmt_server_t;

/**
 * mgmt_server_start - Start management server
 *
 * Creates and initializes management server, binds to specified port,
 * and spawns listener thread. Returns immediately - server runs in background.
 *
 * Parameters:
 *   config - Application configuration (port, password)
 *
 * Returns:
 *   Pointer to management server instance, or NULL on failure
 *
 * Errors:
 *   - Returns NULL if port bind fails
 *   - Returns NULL if thread creation fails
 *   - Logs detailed error messages to stderr
 *
 * Thread Safety:
 *   Safe to call from main thread. Server runs independently.
 */
mgmt_server_t* mgmt_server_start(xoe_config_t *config);

/**
 * mgmt_server_stop - Stop management server
 *
 * Signals shutdown to listener thread and all session threads,
 * waits for graceful exit, and frees all resources.
 *
 * Parameters:
 *   server - Management server instance (NULL safe)
 *
 * Thread Safety:
 *   Blocks until all threads exit. Safe to call from main thread.
 */
void mgmt_server_stop(mgmt_server_t *server);

/**
 * mgmt_server_get_active_sessions - Get count of active sessions
 *
 * Parameters:
 *   server - Management server instance
 *
 * Returns:
 *   Number of currently active management sessions (0 if server NULL)
 */
int mgmt_server_get_active_sessions(mgmt_server_t *server);

#endif /* CORE_MGMT_SERVER_H */
