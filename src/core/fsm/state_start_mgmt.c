#include "core/config.h"
#include "core/mgmt/mgmt_server.h"
#include "core/mgmt/mgmt_config.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * State: Start Management Interface
 *
 * Initializes and starts the management server thread. This state is executed
 * once after configuration validation and before mode selection.
 *
 * The management server runs independently of the operational mode and
 * survives mode restarts.
 *
 * Parameters:
 *   config - Application configuration
 *
 * Returns:
 *   STATE_MODE_SELECT - Always proceeds to mode selection
 *                       (even if mgmt server fails - graceful degradation)
 */
xoe_state_t state_start_mgmt(xoe_config_t *config) {
    mgmt_server_t *server = NULL;

    /* Initialize global config manager (if not already initialized) */
    if (g_config_manager == NULL) {
        g_config_manager = mgmt_config_init(config);
        if (g_config_manager == NULL) {
            fprintf(stderr, "Fatal: Failed to initialize config manager\n");
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
        printf("Configuration manager initialized\n");
    }

    /* Check if management port is configured */
    if (config->mgmt_port == 0) {
        printf("Management interface disabled (port not configured)\n");
        return STATE_MODE_SELECT;
    }

    /* Start management server */
    server = mgmt_server_start(config);
    if (server == NULL) {
        fprintf(stderr, "Warning: Failed to start management interface\n");
        fprintf(stderr, "Continuing without management capabilities\n");
        config->mgmt_server = NULL;
    } else {
        /* Store server handle for later cleanup */
        config->mgmt_server = server;
    }

    /* Always proceed to mode selection (graceful degradation if mgmt fails) */
    return STATE_MODE_SELECT;
}
