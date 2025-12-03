#include <stdlib.h>
#include <string.h>
#include "xoe.h"

/* TLS includes for configuration */
#include "tls_config.h"

/**
 * @brief Initialize configuration with default values
 *
 * Sets up the configuration structure with default values for all fields.
 * This is the first state in the FSM.
 *
 * @param config Pointer to configuration structure to initialize
 * @return STATE_PARSE_ARGS to proceed to argument parsing
 */
xoe_state_t state_init(xoe_config_t *config) {
    /* Initialize all fields to defaults */
    config->mode = MODE_SERVER;  /* Default mode is server */
    config->listen_address = NULL;  /* Default to INADDR_ANY (0.0.0.0) */
    config->listen_port = SERVER_PORT;
    config->connect_server_ip = NULL;
    config->connect_server_port = 0;
    config->program_name = NULL;  /* Will be set during argument parsing */
    config->exit_code = EXIT_SUCCESS;

#if TLS_ENABLED
    /* Initialize TLS configuration with defaults */
    config->encryption_mode = ENCRYPT_NONE;
    strncpy(config->cert_path, TLS_DEFAULT_CERT_FILE, TLS_CERT_PATH_MAX - 1);
    config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(config->key_path, TLS_DEFAULT_KEY_FILE, TLS_CERT_PATH_MAX - 1);
    config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
#else
    config->encryption_mode = 0;  /* No TLS support */
    config->cert_path[0] = '\0';
    config->key_path[0] = '\0';
#endif

    return STATE_PARSE_ARGS;
}
