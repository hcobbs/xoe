#include "mgmt_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Configuration Manager Implementation
 *
 * Provides thread-safe dual-configuration management for runtime
 * reconfiguration of XOE application.
 */

/* Helper function to copy string (handles NULL) */
static char* copy_string(const char *src) {
    char *dst;
    if (src == NULL) {
        return NULL;
    }
    dst = (char*)malloc(strlen(src) + 1);
    if (dst == NULL) {
        return NULL;
    }
    strcpy(dst, src);
    return dst;
}

/* Helper function to free string (NULL safe) */
static void free_string(char **str) {
    if (str != NULL && *str != NULL) {
        free(*str);
        *str = NULL;
    }
}

/**
 * Deep copy configuration structure
 *
 * Copies all fields from src to dst, including dynamic strings.
 * Does NOT copy opaque pointers (serial_config, usb_config).
 */
static int copy_config(xoe_config_t *dst, const xoe_config_t *src) {
    /* Copy simple fields */
    dst->mode = src->mode;
    dst->listen_port = src->listen_port;
    dst->connect_server_port = src->connect_server_port;
    dst->encryption_mode = src->encryption_mode;
    dst->use_serial = src->use_serial;
    dst->use_usb = src->use_usb;
    dst->exit_code = src->exit_code;
    dst->server_fd = src->server_fd;

    /* Copy string fields */
    free_string(&dst->listen_address);
    dst->listen_address = copy_string(src->listen_address);

    free_string(&dst->connect_server_ip);
    dst->connect_server_ip = copy_string(src->connect_server_ip);

    free_string(&dst->serial_device);
    dst->serial_device = copy_string(src->serial_device);

    free_string(&dst->program_name);
    dst->program_name = copy_string(src->program_name);

    /* Copy fixed-size arrays */
    memcpy(dst->cert_path, src->cert_path, TLS_CERT_PATH_MAX);
    memcpy(dst->key_path, src->key_path, TLS_CERT_PATH_MAX);

    /* Note: Opaque pointers (serial_config, usb_config) are NOT copied.
     * These must be managed separately by their respective modules. */
    dst->serial_config = src->serial_config;
    dst->usb_config = src->usb_config;

    return 0;
}

/**
 * Initialize configuration manager
 */
mgmt_config_manager_t* mgmt_config_init(const xoe_config_t *initial_config) {
    mgmt_config_manager_t *mgr;
    int mutex_result;

    if (initial_config == NULL) {
        return NULL;
    }

    mgr = (mgmt_config_manager_t*)malloc(sizeof(mgmt_config_manager_t));
    if (mgr == NULL) {
        return NULL;
    }

    /* Initialize strings to NULL for safe copying */
    mgr->active.listen_address = NULL;
    mgr->active.connect_server_ip = NULL;
    mgr->active.serial_device = NULL;
    mgr->active.program_name = NULL;
    mgr->pending.listen_address = NULL;
    mgr->pending.connect_server_ip = NULL;
    mgr->pending.serial_device = NULL;
    mgr->pending.program_name = NULL;

    /* Copy initial config to both active and pending */
    if (copy_config(&mgr->active, initial_config) != 0) {
        free(mgr);
        return NULL;
    }

    if (copy_config(&mgr->pending, initial_config) != 0) {
        free_string(&mgr->active.listen_address);
        free_string(&mgr->active.connect_server_ip);
        free_string(&mgr->active.serial_device);
        free_string(&mgr->active.program_name);
        free(mgr);
        return NULL;
    }

    mgr->has_pending = 0;

    /* Initialize mutex */
    mutex_result = pthread_mutex_init(&mgr->mutex, NULL);
    if (mutex_result != 0) {
        free_string(&mgr->active.listen_address);
        free_string(&mgr->active.connect_server_ip);
        free_string(&mgr->active.serial_device);
        free_string(&mgr->active.program_name);
        free_string(&mgr->pending.listen_address);
        free_string(&mgr->pending.connect_server_ip);
        free_string(&mgr->pending.serial_device);
        free_string(&mgr->pending.program_name);
        free(mgr);
        return NULL;
    }

    return mgr;
}

/**
 * Destroy configuration manager
 */
void mgmt_config_destroy(mgmt_config_manager_t *mgr) {
    if (mgr == NULL) {
        return;
    }

    /* Destroy mutex */
    pthread_mutex_destroy(&mgr->mutex);

    /* Free active config strings */
    free_string(&mgr->active.listen_address);
    free_string(&mgr->active.connect_server_ip);
    free_string(&mgr->active.serial_device);
    free_string(&mgr->active.program_name);

    /* Free pending config strings */
    free_string(&mgr->pending.listen_address);
    free_string(&mgr->pending.connect_server_ip);
    free_string(&mgr->pending.serial_device);
    free_string(&mgr->pending.program_name);

    /* Free manager */
    free(mgr);
}

/**
 * Check if pending configuration exists
 */
int mgmt_config_has_pending(mgmt_config_manager_t *mgr) {
    int result;

    if (mgr == NULL) {
        return 0;
    }

    pthread_mutex_lock(&mgr->mutex);
    result = mgr->has_pending;
    pthread_mutex_unlock(&mgr->mutex);

    return result;
}

/**
 * Apply pending configuration
 */
int mgmt_config_apply_pending(mgmt_config_manager_t *mgr) {
    if (mgr == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);

    /* Copy pending to active */
    if (copy_config(&mgr->active, &mgr->pending) != 0) {
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    /* Clear pending flag */
    mgr->has_pending = 0;

    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

/**
 * Clear pending configuration
 */
void mgmt_config_clear_pending(mgmt_config_manager_t *mgr) {
    if (mgr == NULL) {
        return;
    }

    pthread_mutex_lock(&mgr->mutex);

    /* Copy active to pending (discard changes) */
    copy_config(&mgr->pending, &mgr->active);

    /* Clear pending flag */
    mgr->has_pending = 0;

    pthread_mutex_unlock(&mgr->mutex);
}

/**
 * Validate pending configuration
 */
int mgmt_config_validate_pending(mgmt_config_manager_t *mgr,
                                  char *error_buf, size_t error_buf_size) {
    xoe_config_t temp_config;

    if (mgr == NULL || error_buf == NULL || error_buf_size == 0) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);

    /* Copy pending config for validation (avoid holding lock during checks) */
    temp_config.listen_address = NULL;
    temp_config.connect_server_ip = NULL;
    temp_config.serial_device = NULL;
    temp_config.program_name = NULL;
    copy_config(&temp_config, &mgr->pending);

    pthread_mutex_unlock(&mgr->mutex);

    /* Validate port numbers */
    if (temp_config.listen_port < 1 || temp_config.listen_port > 65535) {
        snprintf(error_buf, error_buf_size,
                 "Invalid listen port: %d (must be 1-65535)",
                 temp_config.listen_port);
        free_string(&temp_config.listen_address);
        free_string(&temp_config.connect_server_ip);
        free_string(&temp_config.serial_device);
        free_string(&temp_config.program_name);
        return -1;
    }

    if (temp_config.mode == MODE_CLIENT_STANDARD ||
        temp_config.mode == MODE_CLIENT_SERIAL ||
        temp_config.mode == MODE_CLIENT_USB) {
        if (temp_config.connect_server_port < 1 ||
            temp_config.connect_server_port > 65535) {
            snprintf(error_buf, error_buf_size,
                     "Invalid connect port: %d (must be 1-65535)",
                     temp_config.connect_server_port);
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }

        if (temp_config.connect_server_ip == NULL ||
            strlen(temp_config.connect_server_ip) == 0) {
            snprintf(error_buf, error_buf_size,
                     "Client mode requires connect address");
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }
    }

    /* Validate serial configuration */
    if (temp_config.mode == MODE_CLIENT_SERIAL) {
        if (temp_config.serial_device == NULL ||
            strlen(temp_config.serial_device) == 0) {
            snprintf(error_buf, error_buf_size,
                     "Serial mode requires device path");
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }

        /* Check if serial device exists (non-blocking check) */
        if (access(temp_config.serial_device, F_OK) != 0) {
            snprintf(error_buf, error_buf_size,
                     "Serial device does not exist: %s",
                     temp_config.serial_device);
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }
    }

    /* Validate TLS configuration */
    if (temp_config.encryption_mode != 0) {
        if (strlen(temp_config.cert_path) == 0) {
            snprintf(error_buf, error_buf_size,
                     "TLS enabled but no certificate path specified");
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }

        if (access(temp_config.cert_path, R_OK) != 0) {
            snprintf(error_buf, error_buf_size,
                     "TLS certificate not readable: %s",
                     temp_config.cert_path);
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }

        if (strlen(temp_config.key_path) == 0) {
            snprintf(error_buf, error_buf_size,
                     "TLS enabled but no key path specified");
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }

        if (access(temp_config.key_path, R_OK) != 0) {
            snprintf(error_buf, error_buf_size,
                     "TLS key not readable: %s",
                     temp_config.key_path);
            free_string(&temp_config.listen_address);
            free_string(&temp_config.connect_server_ip);
            free_string(&temp_config.serial_device);
            free_string(&temp_config.program_name);
            return -1;
        }
    }

    /* Free temporary strings */
    free_string(&temp_config.listen_address);
    free_string(&temp_config.connect_server_ip);
    free_string(&temp_config.serial_device);
    free_string(&temp_config.program_name);

    /* Validation passed */
    snprintf(error_buf, error_buf_size, "Configuration is valid");
    return 0;
}

/* Configuration Getters (read active configuration) */

xoe_mode_t mgmt_config_get_mode(mgmt_config_manager_t *mgr) {
    xoe_mode_t mode;

    if (mgr == NULL) {
        return MODE_HELP;
    }

    pthread_mutex_lock(&mgr->mutex);
    mode = mgr->active.mode;
    pthread_mutex_unlock(&mgr->mutex);

    return mode;
}

const char* mgmt_config_get_listen_address(mgmt_config_manager_t *mgr) {
    const char *address;

    if (mgr == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&mgr->mutex);
    address = mgr->active.listen_address;
    pthread_mutex_unlock(&mgr->mutex);

    return address;
}

int mgmt_config_get_listen_port(mgmt_config_manager_t *mgr) {
    int port;

    if (mgr == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    port = mgr->active.listen_port;
    pthread_mutex_unlock(&mgr->mutex);

    return port;
}

const char* mgmt_config_get_connect_address(mgmt_config_manager_t *mgr) {
    const char *address;

    if (mgr == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&mgr->mutex);
    address = mgr->active.connect_server_ip;
    pthread_mutex_unlock(&mgr->mutex);

    return address;
}

int mgmt_config_get_connect_port(mgmt_config_manager_t *mgr) {
    int port;

    if (mgr == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    port = mgr->active.connect_server_port;
    pthread_mutex_unlock(&mgr->mutex);

    return port;
}

int mgmt_config_get_encryption(mgmt_config_manager_t *mgr) {
    int mode;

    if (mgr == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    mode = mgr->active.encryption_mode;
    pthread_mutex_unlock(&mgr->mutex);

    return mode;
}

/* Configuration Setters (modify pending configuration) */

int mgmt_config_set_mode(mgmt_config_manager_t *mgr, xoe_mode_t mode) {
    if (mgr == NULL) {
        return -1;
    }

    /* Validate mode */
    if (mode != MODE_HELP && mode != MODE_SERVER &&
        mode != MODE_CLIENT_STANDARD && mode != MODE_CLIENT_SERIAL &&
        mode != MODE_CLIENT_USB) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    mgr->pending.mode = mode;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_listen_address(mgmt_config_manager_t *mgr,
                                    const char *address) {
    char *new_address;

    if (mgr == NULL) {
        return -1;
    }

    new_address = copy_string(address);

    pthread_mutex_lock(&mgr->mutex);
    free_string(&mgr->pending.listen_address);
    mgr->pending.listen_address = new_address;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_listen_port(mgmt_config_manager_t *mgr, int port) {
    if (mgr == NULL) {
        return -1;
    }

    /* Validate port */
    if (port < 1 || port > 65535) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    mgr->pending.listen_port = port;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_connect_address(mgmt_config_manager_t *mgr,
                                     const char *address) {
    char *new_address;

    if (mgr == NULL || address == NULL) {
        return -1;
    }

    new_address = copy_string(address);
    if (new_address == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    free_string(&mgr->pending.connect_server_ip);
    mgr->pending.connect_server_ip = new_address;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_connect_port(mgmt_config_manager_t *mgr, int port) {
    if (mgr == NULL) {
        return -1;
    }

    /* Validate port */
    if (port < 1 || port > 65535) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    mgr->pending.connect_server_port = port;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_encryption(mgmt_config_manager_t *mgr, int mode) {
    if (mgr == NULL) {
        return -1;
    }

    /* Validate encryption mode (0=none, 1=TLS 1.2, 2=TLS 1.3) */
    if (mode < 0 || mode > 2) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    mgr->pending.encryption_mode = mode;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_serial_device(mgmt_config_manager_t *mgr,
                                   const char *device) {
    char *new_device;

    if (mgr == NULL || device == NULL) {
        return -1;
    }

    new_device = copy_string(device);
    if (new_device == NULL) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    free_string(&mgr->pending.serial_device);
    mgr->pending.serial_device = new_device;
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

int mgmt_config_set_serial_baud(mgmt_config_manager_t *mgr, int baud) {
    if (mgr == NULL) {
        return -1;
    }

    /* Validate baud rate */
    if (baud != 9600 && baud != 19200 && baud != 38400 &&
        baud != 57600 && baud != 115200 && baud != 230400) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);
    /* Note: Baud rate is stored in serial_config opaque pointer.
     * This setter would need to access the serial_config structure.
     * For now, we'll mark pending but actual storage depends on
     * serial_config implementation. */
    mgr->has_pending = 1;
    pthread_mutex_unlock(&mgr->mutex);

    /* TODO: Access serial_config and set baud rate when available */
    return 0;
}
