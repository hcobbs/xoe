/**
 * globals.c
 *
 * Global variables shared across the application.
 * Separated from main.c to allow tests to link without main().
 *
 * Thread safety (NET-007, FSM-014 fix):
 * All global state access uses mutex protection for thread-safe
 * check-then-act patterns.
 */

#include <stddef.h>
#include <pthread.h>
#include "core/config.h"

/* Global restart flag for management-triggered mode restarts */
static volatile sig_atomic_t g_mgmt_restart_requested_internal = 0;
static pthread_mutex_t g_restart_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global config manager for runtime configuration changes */
struct mgmt_config_manager_t *g_config_manager = NULL;

/**
 * Set the restart request flag (thread-safe)
 */
void mgmt_restart_request(void) {
    pthread_mutex_lock(&g_restart_mutex);
    g_mgmt_restart_requested_internal = 1;
    pthread_mutex_unlock(&g_restart_mutex);
}

/**
 * Clear the restart request flag (thread-safe)
 */
void mgmt_restart_clear(void) {
    pthread_mutex_lock(&g_restart_mutex);
    g_mgmt_restart_requested_internal = 0;
    pthread_mutex_unlock(&g_restart_mutex);
}

/**
 * Check if restart is requested (thread-safe)
 */
int mgmt_restart_is_requested(void) {
    int result;
    pthread_mutex_lock(&g_restart_mutex);
    result = g_mgmt_restart_requested_internal;
    pthread_mutex_unlock(&g_restart_mutex);
    return result;
}

/**
 * Check and clear restart flag atomically (thread-safe)
 * Returns 1 if restart was requested, 0 otherwise.
 * Clears the flag if it was set.
 */
int mgmt_restart_check_and_clear(void) {
    int result;
    pthread_mutex_lock(&g_restart_mutex);
    result = g_mgmt_restart_requested_internal;
    if (result) {
        g_mgmt_restart_requested_internal = 0;
    }
    pthread_mutex_unlock(&g_restart_mutex);
    return result;
}
