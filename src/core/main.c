/**
 * main.c
 *
 * XOE application entry point with finite state machine.
 */

#include <stdlib.h>
#include "core/config.h"

/* Global variables are defined in globals.c */

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
 * - START_MGMT: Start management interface (one-time)
 * - MODE_SELECT: Determine operating mode
 * - SERVER_MODE/CLIENT_STD/CLIENT_SERIAL/CLIENT_USB: Execute mode-specific logic
 * - MODE_STOP: Gracefully stop current mode (triggered by restart request)
 * - APPLY_CONFIG: Apply pending configuration (after mode stop)
 * - CLEANUP: Release resources
 * - EXIT: Terminal state
 *
 * The FSM supports runtime reconfiguration via the management interface:
 * MODE_SELECT → (mode) → MODE_STOP → APPLY_CONFIG → MODE_SELECT (loop)
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

            case STATE_START_MGMT:
                state = state_start_mgmt(&config);
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

            case STATE_CLIENT_USB:
                state = state_client_usb(&config);
                break;

            case STATE_MODE_STOP:
                state = state_mode_stop(&config);
                break;

            case STATE_APPLY_CONFIG:
                state = state_apply_config(&config);
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
