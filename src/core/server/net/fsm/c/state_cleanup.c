/**
 * state_cleanup.c
 *
 * Cleanup state that releases resources before application exit.
 */

#include <stdlib.h>
#include <stddef.h>
#include "../../h/xoe.h"

/**
 * state_cleanup - Cleanup resources before exit
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_EXIT (terminal state)
 *
 * Cleanup operations:
 * - Free dynamically allocated serial configuration
 * - Close any open file descriptors
 * - Release other resources
 *
 * This is the final state before application exit. All resources
 * allocated during state_init() or subsequent states must be freed here.
 */
xoe_state_t state_cleanup(xoe_config_t *config) {
    /* Validate config pointer */
    if (config == NULL) {
        return STATE_EXIT;
    }

    /* Free dynamically allocated serial configuration */
    if (config->serial_config != NULL) {
        free(config->serial_config);
        config->serial_config = NULL;
    }

    /* Additional cleanup can be added here as needed:
     * - Close open sockets
     * - Free TLS contexts
     * - Release other dynamically allocated resources
     */

    return STATE_EXIT;
}
