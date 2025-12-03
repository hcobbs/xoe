#include <stddef.h>
#include "xoe.h"

/**
 * @brief Determine operating mode
 *
 * Analyzes the configuration to determine which operating mode to enter.
 * The mode is determined by which parameters were specified on the command line.
 *
 * @param config Pointer to configuration structure
 * @return Appropriate state for the selected mode
 */
xoe_state_t state_mode_select(xoe_config_t *config) {
    /* Mode already set during parsing if -h flag was used */
    if (config->mode == MODE_HELP) {
        return STATE_CLEANUP;
    }

    /* If connect_server_ip is set, we're in client mode */
    if (config->connect_server_ip != NULL) {
        config->mode = MODE_CLIENT_STANDARD;
        return STATE_CLIENT_STD;
    }

    /* Otherwise, we're in server mode */
    config->mode = MODE_SERVER;
    return STATE_SERVER_MODE;
}
