#ifndef XOE_H
#define XOE_H

/* C89-compatible fixed-width integer types */
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

/* Define the port number for the server to listen on. */
#define SERVER_PORT 12345
/* Define the maximum number of pending connections in the listen queue. */
#define MAX_PENDING_CONNECTIONS 5
/* Define a buffer size for network communication */
#define BUFFER_SIZE 1024
/* Define the maximum number of concurrent client connections */
#define MAX_CLIENTS 32

/* Forward declaration for TLS configuration path (defined in tls_config.h) */
#ifndef TLS_CERT_PATH_MAX
#define TLS_CERT_PATH_MAX 256
#endif

/**
 * @brief Operating mode enumeration
 *
 * Defines the different modes the XOE application can run in.
 */
typedef enum {
    MODE_HELP,              /**< Display usage information and exit */
    MODE_SERVER,            /**< Run as TCP/TLS server */
    MODE_CLIENT_STANDARD    /**< Run as interactive client (stdin/stdout) */
} xoe_mode_t;

/**
 * @brief Application state enumeration for FSM
 *
 * Defines the states in the application's finite state machine.
 */
typedef enum {
    STATE_INIT,             /**< Initialize configuration with defaults */
    STATE_PARSE_ARGS,       /**< Parse command-line arguments */
    STATE_VALIDATE_CONFIG,  /**< Validate configuration */
    STATE_MODE_SELECT,      /**< Determine operating mode */
    STATE_SERVER_MODE,      /**< Execute server mode */
    STATE_CLIENT_STD,       /**< Execute standard client mode */
    STATE_CLEANUP,          /**< Cleanup resources */
    STATE_EXIT              /**< Exit application */
} xoe_state_t;

/**
 * @brief Unified configuration structure
 *
 * Contains all configuration parameters for the XOE application.
 * Replaces scattered variables in main() with a single cohesive structure.
 */
typedef struct {
    /* Operating mode */
    xoe_mode_t mode;

    /* Network configuration */
    char *listen_address;       /**< Server listen address (NULL = 0.0.0.0) */
    int listen_port;            /**< Server listen port */
    char *connect_server_ip;    /**< Client: server IP to connect to */
    int connect_server_port;    /**< Client: server port to connect to */

    /* TLS configuration (if TLS_ENABLED) */
    int encryption_mode;        /**< Encryption mode (ENCRYPT_NONE, etc.) */
    char cert_path[TLS_CERT_PATH_MAX];  /**< Path to TLS certificate file */
    char key_path[TLS_CERT_PATH_MAX];   /**< Path to TLS key file */

    /* Program name for usage output */
    char *program_name;

    /* Error tracking */
    int exit_code;              /**< Exit code for application */
} xoe_config_t;

/* State handler function prototypes */
xoe_state_t state_init(xoe_config_t *config);
xoe_state_t state_parse_args(xoe_config_t *config, int argc, char *argv[]);
xoe_state_t state_validate_config(xoe_config_t *config);
xoe_state_t state_mode_select(xoe_config_t *config);
xoe_state_t state_server_mode(xoe_config_t *config);
xoe_state_t state_client_std(xoe_config_t *config);
xoe_state_t state_cleanup(xoe_config_t *config);

#endif /* XOE_H */
