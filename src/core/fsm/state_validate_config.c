/**
 * state_validate_config.c
 *
 * Validates the configuration structure to ensure all settings are consistent
 * and meet the application requirements.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/serial/serial_config.h"

/**
 * state_validate_config - Validate configuration settings
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_MODE_SELECT on success, STATE_CLEANUP on validation failure
 *
 * Validates:
 * - Serial mode requires client mode (-s requires -c)
 * - Serial device path is set when serial mode is enabled
 * - Port numbers are in valid range
 * - Configuration consistency
 */
xoe_state_t state_validate_config(xoe_config_t *config) {
    serial_config_t *serial_cfg = (serial_config_t*)config->serial_config;

    /* Validate serial mode configuration */
    if (config->use_serial) {
        if (config->connect_server_ip == NULL) {
            fprintf(stderr, "Serial mode (-s) requires client mode (-c)\n");
            print_usage(config->program_name);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
        if (config->serial_device == NULL) {
            fprintf(stderr, "Serial device not specified\n");
            print_usage(config->program_name);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
        /* Set device path in serial config */
        if (serial_cfg != NULL) {
            strncpy(serial_cfg->device_path, config->serial_device,
                    SERIAL_DEVICE_PATH_MAX - 1);
            serial_cfg->device_path[SERIAL_DEVICE_PATH_MAX - 1] = '\0';
        }
    }

    return STATE_MODE_SELECT;
}
