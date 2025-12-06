#ifndef CORE_MGMT_COMMANDS_H
#define CORE_MGMT_COMMANDS_H

#include "mgmt_internal.h"

/**
 * Management Command Handlers
 *
 * Implements all CLI commands for runtime configuration and control.
 * Uses session's pre-allocated buffers for all I/O.
 */

/**
 * mgmt_command_loop - Main command processing loop
 *
 * Reads commands from session socket, parses, and dispatches to handlers.
 * Continues until 'quit' command or connection close.
 *
 * Parameters:
 *   session - Management session (contains socket, buffers, auth status)
 *
 * Uses session's pre-allocated read_buffer and write_buffer for all I/O.
 */
void mgmt_command_loop(mgmt_session_t *session);

#endif /* CORE_MGMT_COMMANDS_H */
