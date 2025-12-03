/**
 * main.c
 *
 * XOE application entry point with finite state machine.
 */

#include <stdlib.h>
#include "core/config.h"

/**
 * main - XOE application entry point with FSM
 * @argc: Argument count
 * @argv: Argument vector
 *
 * Returns: Exit code from configuration (EXIT_SUCCESS or EXIT_FAILURE)
 *
 * Implements a finite state machine to manage application flow:
 * - INIT: Initialize configuration with defaults
 * - PARSE_ARGS: Parse command-line arguments
 * - VALIDATE_CONFIG: Validate configuration settings
 * - MODE_SELECT: Determine operating mode
 * - SERVER_MODE/CLIENT_STD/CLIENT_SERIAL: Execute mode-specific logic
 * - CLEANUP: Release resources
 * - EXIT: Terminal state
 */
int main(int argc, char *argv[]) {
    xoe_config_t config;
    xoe_state_t state = STATE_INIT;

    while (state != STATE_EXIT) {
        switch (state) {
            case STATE_INIT:
                state = state_init(&config);
                break;

            case STATE_PARSE_ARGS:
                state = state_parse_args(&config, argc, argv);
                break;

            case STATE_VALIDATE_CONFIG:
                state = state_validate_config(&config);
                break;

            case STATE_MODE_SELECT:
                state = state_mode_select(&config);
                break;

            case STATE_SERVER_MODE:
                state = state_server_mode(&config);
                break;

            case STATE_CLIENT_STD:
                state = state_client_std(&config);
                break;

            case STATE_CLIENT_SERIAL:
                state = state_client_serial(&config);
                break;

            case STATE_CLEANUP:
                state = state_cleanup(&config);
                break;

            default:
                /* Unknown state - exit with failure */
                config.exit_code = EXIT_FAILURE;
                state = STATE_EXIT;
                break;
        }
    }

    return config.exit_code;
}
