/**
 * state_init.c
 *
 * Initializes the XOE configuration structure with default values.
 * This is the entry point state for the FSM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "core/config.h"
#include "lib/common/definitions.h"
#include "connectors/serial/serial_config.h"
#include "connectors/usb/usb_config.h"

#if TLS_ENABLED
#include "lib/security/tls_config.h"
#include "lib/security/tls_context.h"
#endif

/* Default length for generated management password */
#define MGMT_PASSWORD_GEN_LEN 16

/**
 * generate_random_password - Generate a random alphanumeric password
 * @length: Desired password length
 *
 * Returns: Dynamically allocated password string, or NULL on failure.
 *          Caller is responsible for freeing the returned string.
 *
 * Uses /dev/urandom for secure random bytes, with fallback to rand().
 */
static char* generate_random_password(int length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char *password;
    int i;
    int fd;
    unsigned char random_bytes[64];  /* Buffer for random data */

    if (length <= 0 || length > 64) {
        return NULL;
    }

    password = (char*)malloc((size_t)(length + 1));
    if (password == NULL) {
        return NULL;
    }

    /* Try to read from /dev/urandom for secure random bytes */
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, random_bytes, (size_t)length);
        close(fd);

        if (bytes_read == (ssize_t)length) {
            /* Use secure random bytes */
            for (i = 0; i < length; i++) {
                password[i] = charset[random_bytes[i] % (sizeof(charset) - 1)];
            }
            password[length] = '\0';
            return password;
        }
    }

    /* Fallback to rand() (less secure, but functional) */
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
    for (i = 0; i < length; i++) {
        password[i] = charset[rand() % (int)(sizeof(charset) - 1)];
    }
    password[length] = '\0';

    return password;
}

/**
 * state_init - Initialize configuration with default values
 * @config: Pointer to configuration structure to initialize
 *
 * Returns: STATE_PARSE_ARGS on success, STATE_CLEANUP on allocation failure
 *
 * Initializes all configuration fields with sensible defaults:
 * - Server mode as default operating mode
 * - Default listen port from SERVER_PORT
 * - NULL for optional string fields
 * - Default serial configuration
 * - Default TLS certificate/key paths (if TLS enabled)
 */
xoe_state_t state_init(xoe_config_t *config) {
    /* Set default operating mode */
    config->mode = MODE_SERVER;

    /* Initialize network configuration */
    config->listen_address = NULL;  /* Default to INADDR_ANY (0.0.0.0) */
    config->listen_port = SERVER_PORT;
    config->connect_server_ip = NULL;
    config->connect_server_port = 0;

    /* Initialize serial configuration */
    config->use_serial = FALSE;
    config->serial_device = NULL;
    config->serial_config = malloc(sizeof(serial_config_t));
    if (config->serial_config == NULL) {
        fprintf(stderr, "Error: Failed to allocate serial configuration\n");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }
    serial_config_init_defaults((serial_config_t*)config->serial_config);

    /* Initialize USB configuration */
    config->use_usb = FALSE;
    config->usb_config = usb_multi_config_init(USB_MAX_DEVICES);
    if (config->usb_config == NULL) {
        fprintf(stderr, "Error: Failed to allocate USB configuration\n");
        free(config->serial_config);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Initialize connection file descriptor */
    config->server_fd = -1;

    /* Initialize management interface configuration */
    config->mgmt_port = MGMT_PORT;  /* Default port 6969 */
    config->mgmt_server = NULL;      /* Management server not started yet */

    /* Generate random management password for security */
    /* User can override via command-line option */
    config->mgmt_password = generate_random_password(MGMT_PASSWORD_GEN_LEN);
    if (config->mgmt_password == NULL) {
        fprintf(stderr, "Error: Failed to generate management password\n");
        free(config->serial_config);
        usb_multi_config_free((usb_multi_config_t*)config->usb_config);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }
    fprintf(stderr, "Management password: %s\n", config->mgmt_password);

#if TLS_ENABLED
    /* Initialize TLS configuration with default paths */
    config->encryption_mode = ENCRYPT_NONE;
    strncpy(config->cert_path, TLS_DEFAULT_CERT_FILE, TLS_CERT_PATH_MAX - 1);
    config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(config->key_path, TLS_DEFAULT_KEY_FILE, TLS_CERT_PATH_MAX - 1);
    config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(config->ca_path, TLS_DEFAULT_CA_FILE, TLS_CERT_PATH_MAX - 1);
    config->ca_path[TLS_CERT_PATH_MAX - 1] = '\0';
    config->tls_verify_mode = TLS_DEFAULT_VERIFY_MODE;
#else
    config->encryption_mode = 0;
    config->cert_path[0] = '\0';
    config->key_path[0] = '\0';
    config->ca_path[0] = '\0';
    config->tls_verify_mode = 0;
#endif

    /* Initialize program metadata */
    config->program_name = NULL;
    config->exit_code = EXIT_SUCCESS;

    return STATE_PARSE_ARGS;
}
