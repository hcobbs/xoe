/**
 * state_mode_select.c
 *
 * Determines the operating mode based on configuration and routes to the
 * appropriate state handler.
 */

#include <stddef.h>
#include "../../h/xoe.h"
#include "../../../../../common/h/commonDefinitions.h"

/**
 * state_mode_select - Determine operating mode and select next state
 * @config: Pointer to configuration structure
 *
 * Returns: Appropriate state based on operating mode
 *   - STATE_CLEANUP if mode is MODE_HELP
 *   - STATE_CLIENT_SERIAL if client mode with serial enabled
 *   - STATE_CLIENT_STD if client mode without serial
 *   - STATE_SERVER_MODE for server mode (default)
 *
 * Mode selection logic:
 * 1. If help mode was set during arg parsing, cleanup and exit
 * 2. If connect_server_ip is set, operate as client
 *    a. If serial enabled, use serial bridge mode
 *    b. Otherwise, use standard client mode
 * 3. Default to server mode
 */
xoe_state_t state_mode_select(xoe_config_t *config) {
    /* If help mode was requested, proceed to cleanup */
    if (config->mode == MODE_HELP) {
        return STATE_CLEANUP;
    }

    /* Determine mode based on configuration */
    if (config->connect_server_ip != NULL) {
        /* Client mode */
        if (config->use_serial == TRUE) {
            config->mode = MODE_CLIENT_SERIAL;
            return STATE_CLIENT_SERIAL;
        } else {
            config->mode = MODE_CLIENT_STANDARD;
            return STATE_CLIENT_STD;
        }
    }

    /* Default to server mode */
    config->mode = MODE_SERVER;
    return STATE_SERVER_MODE;
}
