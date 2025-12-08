#ifndef CORE_MGMT_CONFIG_H
#define CORE_MGMT_CONFIG_H

#include "core/config.h"
#include <pthread.h>

/**
 * Configuration Manager for XOE Runtime Reconfiguration
 *
 * Provides a dual-configuration model with active and pending configurations,
 * allowing queued configuration changes to be validated and applied
 * atomically during runtime mode restarts.
 *
 * Thread Safety: All functions use mutex protection for concurrent access
 * from operational and management threads.
 */

/**
 * Dual configuration structure
 *
 * Maintains two independent configuration instances:
 * - active: Currently running configuration (read by operational threads)
 * - pending: Staged configuration for next restart (modified by management)
 *
 * Note: Named struct allows forward declaration in config.h as
 * "struct mgmt_config_manager_t" while typedef provides convenience name.
 */
typedef struct mgmt_config_manager_t {
    xoe_config_t active;       /* Currently running configuration */
    xoe_config_t pending;      /* Staged configuration for next restart */
    int has_pending;           /* Flag: 1 if pending differs from active */
    pthread_mutex_t mutex;     /* Protects both configurations */
} mgmt_config_manager_t;

/**
 * Initialize configuration manager
 *
 * Creates a new configuration manager and copies the initial configuration
 * to both active and pending slots.
 *
 * Parameters:
 *   initial_config - Initial configuration to use (copied to active/pending)
 *
 * Returns:
 *   Pointer to allocated configuration manager, or NULL on failure
 *
 * Errors:
 *   Returns NULL if malloc fails or mutex init fails
 */
mgmt_config_manager_t* mgmt_config_init(const xoe_config_t *initial_config);

/**
 * Destroy configuration manager
 *
 * Frees all resources associated with the configuration manager,
 * including mutex destruction and memory deallocation.
 *
 * Parameters:
 *   mgr - Configuration manager to destroy (NULL safe)
 */
void mgmt_config_destroy(mgmt_config_manager_t *mgr);

/**
 * Check if pending configuration exists
 *
 * Parameters:
 *   mgr - Configuration manager
 *
 * Returns:
 *   1 if pending configuration differs from active, 0 otherwise
 *
 * Thread Safety: Mutex-protected read
 */
int mgmt_config_has_pending(mgmt_config_manager_t *mgr);

/**
 * Apply pending configuration
 *
 * Copies pending configuration to active configuration and clears
 * the has_pending flag. This is typically called during STATE_APPLY_CONFIG.
 *
 * Parameters:
 *   mgr - Configuration manager
 *
 * Returns:
 *   0 on success, -1 on error
 *
 * Thread Safety: Mutex-protected write
 */
int mgmt_config_apply_pending(mgmt_config_manager_t *mgr);

/**
 * Clear pending configuration
 *
 * Resets pending configuration to match active configuration,
 * discarding any queued changes.
 *
 * Parameters:
 *   mgr - Configuration manager
 *
 * Thread Safety: Mutex-protected write
 */
void mgmt_config_clear_pending(mgmt_config_manager_t *mgr);

/**
 * Validate pending configuration
 *
 * Checks pending configuration for logical consistency and validity:
 * - Port numbers in valid range (1-65535)
 * - Serial device path exists (if serial mode)
 * - TLS certificate/key paths exist (if encryption enabled)
 * - Mode-specific requirements met
 *
 * Parameters:
 *   mgr - Configuration manager
 *   error_buf - Buffer to receive error message (if validation fails)
 *   error_buf_size - Size of error buffer
 *
 * Returns:
 *   0 if valid, -1 if invalid (error_buf contains reason)
 *
 * Thread Safety: Mutex-protected read
 */
int mgmt_config_validate_pending(mgmt_config_manager_t *mgr,
                                  char *error_buf, size_t error_buf_size);

/* Configuration Getters (read active configuration) */

/**
 * Get active mode
 *
 * Returns:
 *   Current operating mode (MODE_SERVER, MODE_CLIENT_*, MODE_HELP)
 */
xoe_mode_t mgmt_config_get_mode(mgmt_config_manager_t *mgr);

/**
 * Get active listen address
 *
 * Returns:
 *   Server listen address (NULL for 0.0.0.0, caller must not free)
 */
const char* mgmt_config_get_listen_address(mgmt_config_manager_t *mgr);

/**
 * Get active listen port
 *
 * Returns:
 *   Server listen port number
 */
int mgmt_config_get_listen_port(mgmt_config_manager_t *mgr);

/**
 * Get active connect address
 *
 * Returns:
 *   Client connection IP address (caller must not free)
 */
const char* mgmt_config_get_connect_address(mgmt_config_manager_t *mgr);

/**
 * Get active connect port
 *
 * Returns:
 *   Client connection port number
 */
int mgmt_config_get_connect_port(mgmt_config_manager_t *mgr);

/**
 * Get active encryption mode
 *
 * Returns:
 *   Encryption mode (0=none, 1=TLS 1.2, 2=TLS 1.3)
 */
int mgmt_config_get_encryption(mgmt_config_manager_t *mgr);

/* Configuration Setters (modify pending configuration) */

/**
 * Set pending mode
 *
 * Parameters:
 *   mgr - Configuration manager
 *   mode - New operating mode
 *
 * Returns:
 *   0 on success, -1 on invalid mode
 */
int mgmt_config_set_mode(mgmt_config_manager_t *mgr, xoe_mode_t mode);

/**
 * Set pending listen address
 *
 * Parameters:
 *   mgr - Configuration manager
 *   address - Listen address string (NULL for 0.0.0.0, copied internally)
 *
 * Returns:
 *   0 on success, -1 on error
 */
int mgmt_config_set_listen_address(mgmt_config_manager_t *mgr,
                                    const char *address);

/**
 * Set pending listen port
 *
 * Parameters:
 *   mgr - Configuration manager
 *   port - Listen port number (1-65535)
 *
 * Returns:
 *   0 on success, -1 on invalid port
 */
int mgmt_config_set_listen_port(mgmt_config_manager_t *mgr, int port);

/**
 * Set pending connect address
 *
 * Parameters:
 *   mgr - Configuration manager
 *   address - Connection IP address (copied internally)
 *
 * Returns:
 *   0 on success, -1 on error
 */
int mgmt_config_set_connect_address(mgmt_config_manager_t *mgr,
                                     const char *address);

/**
 * Set pending connect port
 *
 * Parameters:
 *   mgr - Configuration manager
 *   port - Connection port number (1-65535)
 *
 * Returns:
 *   0 on success, -1 on invalid port
 */
int mgmt_config_set_connect_port(mgmt_config_manager_t *mgr, int port);

/**
 * Set pending encryption mode
 *
 * Parameters:
 *   mgr - Configuration manager
 *   mode - Encryption mode (0=none, 1=TLS 1.2, 2=TLS 1.3)
 *
 * Returns:
 *   0 on success, -1 on invalid mode
 */
int mgmt_config_set_encryption(mgmt_config_manager_t *mgr, int mode);

/**
 * Set pending serial device
 *
 * Parameters:
 *   mgr - Configuration manager
 *   device - Serial device path (e.g., /dev/ttyUSB0, copied internally)
 *
 * Returns:
 *   0 on success, -1 on error
 */
int mgmt_config_set_serial_device(mgmt_config_manager_t *mgr,
                                   const char *device);

/**
 * Set pending serial baud rate
 *
 * Parameters:
 *   mgr - Configuration manager
 *   baud - Baud rate (9600, 19200, 38400, 57600, 115200, 230400)
 *
 * Returns:
 *   0 on success, -1 on invalid baud rate
 */
int mgmt_config_set_serial_baud(mgmt_config_manager_t *mgr, int baud);

#endif /* CORE_MGMT_CONFIG_H */
