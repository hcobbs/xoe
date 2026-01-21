#include "core/config.h"
#include "core/mgmt/mgmt_config.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * State: Apply Configuration
 *
 * Applies pending configuration changes that were queued via the management
 * interface. This state is entered after the current mode has been gracefully
 * shut down via STATE_MODE_STOP.
 *
 * The pending configuration is validated and then atomically swapped to become
 * the active configuration. The FSM then returns to STATE_MODE_SELECT to
 * restart the (potentially new) operational mode.
 *
 * Parameters:
 *   config - Application configuration
 *
 * Returns:
 *   STATE_MODE_SELECT - Always proceeds to mode selection with new config
 *   STATE_CLEANUP - If configuration validation fails (should not happen)
 */
xoe_state_t state_apply_config(xoe_config_t *config) {
    char error_buf[256] = {0};
    int result = 0;

    if (config == NULL) {
        fprintf(stderr, "Error: NULL config in state_apply_config\n");
        return STATE_CLEANUP;
    }

    if (g_config_manager == NULL) {
        fprintf(stderr, "Error: Config manager not initialized\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    printf("Applying pending configuration...\n");

    /* Validate pending configuration (double-check) */
    result = mgmt_config_validate_pending(g_config_manager, error_buf, sizeof(error_buf));
    if (result != 0) {
        fprintf(stderr, "Error: Invalid pending config: %s\n", error_buf);
        fprintf(stderr, "This should not happen - restart command validates first\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Apply pending configuration (atomically swaps pending -> active) */
    result = mgmt_config_apply_pending(g_config_manager);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to apply pending configuration\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Update local config structure from newly active configuration */
    config->mode = mgmt_config_get_mode(g_config_manager);
    config->listen_port = mgmt_config_get_listen_port(g_config_manager);
    config->encryption_mode = mgmt_config_get_encryption(g_config_manager);

    /* Note: Other config fields (listen_address, connect addresses, etc.)
     * are managed internally by the config manager and accessed via getters.
     * The local config structure is primarily used for mode selection logic. */

    printf("Configuration applied successfully\n");
    printf("Restarting with new configuration...\n");

    /* Reset restart flag (NET-007 fix: thread-safe clear) */
    mgmt_restart_clear();

    /* Return to mode selection to restart with new config */
    return STATE_MODE_SELECT;
}
