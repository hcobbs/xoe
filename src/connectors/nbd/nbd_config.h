/**
 * @file nbd_config.h
 * @brief NBD connector configuration structures and constants
 *
 * Defines configuration structures for NBD (Network Block Device) server mode.
 * Follows serial connector configuration pattern for consistency.
 *
 * [LLM-ASSISTED]
 */

#ifndef NBD_CONFIG_H
#define NBD_CONFIG_H

#include "lib/common/types.h"

/* Path and name length limits */
#define NBD_PATH_MAX 256
#define NBD_NAME_MAX 64

/* Default values */
#define NBD_DEFAULT_EXPORT_NAME "xoe"
#define NBD_DEFAULT_BLOCK_SIZE  4096
#define NBD_DEFAULT_MAX_CONN    4

/* Backend type identifiers */
#define NBD_BACKEND_AUTO   0  /* Auto-detect from path */
#define NBD_BACKEND_FILE   1  /* Regular file */
#define NBD_BACKEND_ZVOL   2  /* ZFS zvol */
#define NBD_BACKEND_DEVICE 3  /* Raw block device */

/* Capability flags */
#define NBD_CAP_FLUSH     (1 << 0)  /* Supports NBD_CMD_FLUSH */
#define NBD_CAP_TRIM      (1 << 1)  /* Supports NBD_CMD_TRIM */
#define NBD_CAP_READONLY  (1 << 2)  /* Read-only export */

/**
 * NBD connector configuration structure
 *
 * Contains all configuration parameters for NBD server mode.
 * Initialized with defaults via nbd_config_init_defaults().
 */
typedef struct {
    char export_path[NBD_PATH_MAX];  /* Path to zvol/file/device */
    char export_name[NBD_NAME_MAX];  /* Export name for negotiation */
    int backend_type;                 /* NBD_BACKEND_* */
    uint64_t export_size;             /* Override size (0 = auto-detect) */
    uint32_t block_size;              /* Block size in bytes */
    int max_connections;              /* Max concurrent NBD sessions */
    uint32_t capabilities;            /* NBD_CAP_* flags */
} nbd_config_t;

/**
 * nbd_config_init_defaults - Initialize configuration with default values
 * @config: Pointer to configuration structure
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_config_init_defaults(nbd_config_t* config);

/**
 * nbd_config_validate - Validate configuration parameters
 * @config: Pointer to configuration structure
 *
 * Checks:
 * - Export path is not empty and exists
 * - Block size is power of 2 and within valid range (512-65536)
 * - Export size is multiple of block size (if specified)
 * - Backend type is valid
 *
 * Returns: 0 if valid, negative error code if invalid
 */
int nbd_config_validate(const nbd_config_t* config);

/**
 * nbd_config_auto_detect_backend - Auto-detect backend type from path
 * @path: Export path to analyze
 *
 * Detects backend type based on path characteristics:
 * - /dev/zvol/pool/dataset -> NBD_BACKEND_ZVOL
 * - /dev/sda (block device) -> NBD_BACKEND_DEVICE
 * - Regular file -> NBD_BACKEND_FILE
 *
 * Returns: NBD_BACKEND_* constant, or negative error code on failure
 */
int nbd_config_auto_detect_backend(const char* path);

#endif /* NBD_CONFIG_H */
