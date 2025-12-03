#include <stddef.h>
#include "xoe.h"

/**
 * @brief Validate configuration
 *
 * Performs validation of the configuration to ensure all settings are
 * consistent and valid. Currently minimal validation since most is done
 * during parsing.
 *
 * @param config Pointer to configuration structure to validate
 * @return STATE_MODE_SELECT to proceed to mode selection
 */
xoe_state_t state_validate_config(xoe_config_t *config) {
    /* Most validation is done during argument parsing */
    /* Add any cross-field validation here if needed in the future */
    (void)config;  /* Suppress unused parameter warning */
    return STATE_MODE_SELECT;
}
