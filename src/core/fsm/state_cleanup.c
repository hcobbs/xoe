/**
 * state_cleanup.c
 *
 * Cleanup state that releases resources before application exit.
 */

#include <stdlib.h>
#include <stddef.h>
#include "core/config.h"
#include "connectors/usb/usb_config.h"

#if TLS_ENABLED
#include "core/server.h"
#include "lib/security/tls_context.h"
#endif

/**
 * state_cleanup - Cleanup resources before exit
 * @config: Pointer to configuration structure
 *
 * Returns: STATE_EXIT (terminal state)
 *
 * Cleanup operations:
 * - Free dynamically allocated serial configuration
 * - Cleanup global TLS context (if TLS enabled)
 * - Release other dynamically allocated resources
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

    /* Free dynamically allocated USB configuration */
    if (config->usb_config != NULL) {
        usb_multi_config_free((usb_multi_config_t*)config->usb_config);
        config->usb_config = NULL;
    }

#if TLS_ENABLED
    /* Cleanup global TLS context if still allocated */
    if (g_tls_ctx != NULL) {
        tls_context_cleanup(g_tls_ctx);
        g_tls_ctx = NULL;
    }
#endif

    /* Additional cleanup can be added here as needed:
     * - Close open sockets
     * - Release other dynamically allocated resources
     */

    return STATE_EXIT;
}
