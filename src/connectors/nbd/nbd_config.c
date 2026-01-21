/**
 * @file nbd_config.c
 * @brief NBD connector configuration implementation
 *
 * [LLM-ASSISTED]
 */

#include "connectors/nbd/nbd_config.h"
#include "lib/common/definitions.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/**
 * nbd_config_init_defaults - Initialize configuration with default values
 */
int nbd_config_init_defaults(nbd_config_t* config) {
    if (config == NULL) {
        return E_NULL_POINTER;
    }

    memset(config, 0, sizeof(nbd_config_t));

    /* Set default export name */
    strncpy(config->export_name, NBD_DEFAULT_EXPORT_NAME, NBD_NAME_MAX - 1);
    config->export_name[NBD_NAME_MAX - 1] = '\0';

    /* Set default parameters */
    config->backend_type = NBD_BACKEND_AUTO;
    config->export_size = 0;  /* Auto-detect */
    config->block_size = NBD_DEFAULT_BLOCK_SIZE;
    config->max_connections = NBD_DEFAULT_MAX_CONN;

    /* Enable all capabilities by default */
    config->capabilities = NBD_CAP_FLUSH | NBD_CAP_TRIM;

    return SUCCESS;
}

/**
 * nbd_config_validate - Validate configuration parameters
 */
int nbd_config_validate(const nbd_config_t* config) {
    struct stat st;

    if (config == NULL) {
        return E_NULL_POINTER;
    }

    /* Check export path is not empty */
    if (config->export_path[0] == '\0') {
        fprintf(stderr, "NBD configuration error: export path is empty\n");
        return E_INVALID_ARGUMENT;
    }

    /* Check if export path exists */
    if (stat(config->export_path, &st) != 0) {
        fprintf(stderr, "NBD configuration error: export path '%s' does not exist\n",
                config->export_path);
        return E_FILE_NOT_FOUND;
    }

    /* Validate block size is power of 2 and within range */
    if (config->block_size < 512 || config->block_size > 65536) {
        fprintf(stderr, "NBD configuration error: block size %u out of range (512-65536)\n",
                config->block_size);
        return E_INVALID_ARGUMENT;
    }

    /* Check block size is power of 2 */
    if ((config->block_size & (config->block_size - 1)) != 0) {
        fprintf(stderr, "NBD configuration error: block size %u is not a power of 2\n",
                config->block_size);
        return E_INVALID_ARGUMENT;
    }

    /* If export size is specified, check it's a multiple of block size */
    if (config->export_size > 0) {
        if ((config->export_size % config->block_size) != 0) {
            fprintf(stderr, "NBD configuration error: export size %llu is not a multiple of block size %u\n",
                    (unsigned long long)config->export_size, config->block_size);
            return E_INVALID_ARGUMENT;
        }
    }

    /* Validate backend type */
    if (config->backend_type != NBD_BACKEND_AUTO &&
        config->backend_type != NBD_BACKEND_FILE &&
        config->backend_type != NBD_BACKEND_ZVOL &&
        config->backend_type != NBD_BACKEND_DEVICE) {
        fprintf(stderr, "NBD configuration error: invalid backend type %d\n",
                config->backend_type);
        return E_INVALID_ARGUMENT;
    }

    /* Validate max connections */
    if (config->max_connections < 1 || config->max_connections > 32) {
        fprintf(stderr, "NBD configuration error: max_connections %d out of range (1-32)\n",
                config->max_connections);
        return E_INVALID_ARGUMENT;
    }

    return SUCCESS;
}

/**
 * nbd_config_auto_detect_backend - Auto-detect backend type from path
 */
int nbd_config_auto_detect_backend(const char* path) {
    struct stat st;

    if (path == NULL) {
        return E_NULL_POINTER;
    }

    /* Check if path exists */
    if (stat(path, &st) != 0) {
        return E_FILE_NOT_FOUND;
    }

    /* Check for ZFS zvol pattern */
    if (strncmp(path, "/dev/zvol/", 10) == 0) {
        return NBD_BACKEND_ZVOL;
    }

    /* Check for device file */
    if (S_ISBLK(st.st_mode)) {
        return NBD_BACKEND_DEVICE;
    }

    /* Check for regular file */
    if (S_ISREG(st.st_mode)) {
        return NBD_BACKEND_FILE;
    }

    /* Unknown type */
    fprintf(stderr, "NBD backend auto-detection: unable to determine type for '%s'\n",
            path);
    return E_INVALID_ARGUMENT;
}
