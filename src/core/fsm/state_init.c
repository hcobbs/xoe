/**
 * state_init.c
 *
 * Initializes the XOE configuration structure with default values.
 * This is the entry point state for the FSM.
 */

#include <stdlib.h>
#include <string.h>
#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/serial/serial_config.h"

#if TLS_ENABLED
#include "lib/security/tls_context.h"
#endif

/**
 * state_init - Initialize configuration with default values
 * @config: Pointer to configuration structure to initialize
 *
 * Returns: STATE_PARSE_ARGS (next state)
 *
 * Initializes all configuration fields with sensible defaults:
 * - Server mode as default operating mode
 * - Default listen port from SERVER_PORT
 * - NULL for optional string fields
 * - Default serial configuration
 * - Default TLS certificate/key paths (if TLS enabled)
 */
xoe_state_t state_init(xoe_config_t *config) {
    /* Set default operating mode */
    config->mode = MODE_SERVER;

    /* Initialize network configuration */
    config->listen_address = NULL;  /* Default to INADDR_ANY (0.0.0.0) */
    config->listen_port = SERVER_PORT;
    config->connect_server_ip = NULL;
    config->connect_server_port = 0;

    /* Initialize serial configuration */
    config->use_serial = FALSE;
    config->serial_device = NULL;
    config->serial_config = malloc(sizeof(serial_config_t));
    if (config->serial_config != NULL) {
        serial_config_init_defaults((serial_config_t*)config->serial_config);
    }

#if TLS_ENABLED
    /* Initialize TLS configuration with default paths */
    config->encryption_mode = ENCRYPT_NONE;
    strncpy(config->cert_path, TLS_DEFAULT_CERT_FILE, TLS_CERT_PATH_MAX - 1);
    config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(config->key_path, TLS_DEFAULT_KEY_FILE, TLS_CERT_PATH_MAX - 1);
    config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
#else
    config->encryption_mode = 0;
    config->cert_path[0] = '\0';
    config->key_path[0] = '\0';
#endif

    /* Initialize program metadata */
    config->program_name = NULL;
    config->exit_code = EXIT_SUCCESS;

    return STATE_PARSE_ARGS;
}
