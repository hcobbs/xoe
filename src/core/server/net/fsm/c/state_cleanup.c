#include <stddef.h>
#include "xoe.h"

/**
 * @brief Cleanup resources
 *
 * Performs any cleanup needed before exiting the application.
 * Currently minimal since most resources are automatically cleaned up.
 *
 * @param config Pointer to configuration structure
 * @return STATE_EXIT to signal application should exit
 */
xoe_state_t state_cleanup(xoe_config_t *config) {
    /* Cleanup would go here if we had dynamically allocated resources */
    /* For now, just transition to exit state */
    (void)config;  /* Suppress unused parameter warning */
    return STATE_EXIT;
}
