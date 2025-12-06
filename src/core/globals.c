/**
 * globals.c
 *
 * Global variables shared across the application.
 * Separated from main.c to allow tests to link without main().
 */

#include <stddef.h>
#include "core/config.h"

/* Global restart flag for management-triggered mode restarts */
volatile sig_atomic_t g_mgmt_restart_requested = 0;

/* Global config manager for runtime configuration changes */
struct mgmt_config_manager_t *g_config_manager = NULL;
