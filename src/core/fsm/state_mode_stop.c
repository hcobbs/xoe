#include "core/config.h"
#include "core/server.h"
#include <stdio.h>
#include <unistd.h>

#if TLS_ENABLED
#include "lib/security/tls_context.h"
#endif

/**
 * State: Mode Stop
 *
 * Gracefully shuts down the current operational mode in preparation for
 * a restart with new configuration. This state is triggered when the
 * management interface requests a mode restart via mgmt_restart_request().
 *
 * Server Mode Shutdown:
 * - Close server socket to stop accepting new connections
 * - Disconnect all active clients (close sockets)
 * - Wait up to 5 seconds for client threads to exit
 * - Force-clear any remaining client slots
 * - Clean up TLS context and USB server
 *
 * Client Mode Shutdown:
 * - For serial client: call serial_client_request_shutdown()
 * - For standard client: close network socket
 * - Join any spawned threads
 * - Close file descriptors
 *
 * Parameters:
 *   config - Application configuration
 *
 * Returns:
 *   STATE_APPLY_CONFIG - Always proceeds to apply pending configuration
 */
xoe_state_t state_mode_stop(xoe_config_t *config) {
    printf("Mode stop: gracefully shutting down %s mode\n",
           config->mode == MODE_SERVER ? "server" :
           config->mode == MODE_CLIENT_SERIAL ? "serial client" :
           config->mode == MODE_CLIENT_STANDARD ? "standard client" :
           config->mode == MODE_CLIENT_USB ? "USB client" : "unknown");

    /* Mode-specific shutdown logic */
    switch (config->mode) {
        case MODE_SERVER:
            printf("Disconnecting all clients...\n");
            disconnect_all_clients();

            printf("Waiting up to 5 seconds for client threads to exit...\n");
            wait_for_clients(5);

#if TLS_ENABLED
            /* Clean up TLS context if it exists */
            if (g_tls_ctx != NULL) {
                printf("Cleaning up TLS context...\n");
                tls_context_cleanup(g_tls_ctx);
                g_tls_ctx = NULL;
            }
#endif

            /* Close server socket if open */
            if (config->server_fd != -1) {
                close(config->server_fd);
                config->server_fd = -1;
            }

            printf("Server mode shutdown complete\n");
            break;

        case MODE_CLIENT_SERIAL:
            /* TODO: Implement when serial client refactored to support shutdown
             * For now, just close the socket */
            if (config->server_fd != -1) {
                close(config->server_fd);
                config->server_fd = -1;
            }
            printf("Serial client mode shutdown complete\n");
            break;

        case MODE_CLIENT_STANDARD:
            /* Close network socket */
            if (config->server_fd != -1) {
                close(config->server_fd);
                config->server_fd = -1;
            }
            printf("Standard client mode shutdown complete\n");
            break;

        case MODE_CLIENT_USB:
            /* TODO: Implement when USB client exists */
            if (config->server_fd != -1) {
                close(config->server_fd);
                config->server_fd = -1;
            }
            printf("USB client mode shutdown complete\n");
            break;

        default:
            printf("Warning: Unknown mode during shutdown\n");
            break;
    }

    /* Clear restart flag (NET-007 fix: thread-safe clear) */
    mgmt_restart_clear();

    /* Proceed to apply configuration */
    return STATE_APPLY_CONFIG;
}
