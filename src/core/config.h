#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include "lib/common/types.h"
#include <signal.h>

/* Global restart flag for management-triggered mode restarts */
extern volatile sig_atomic_t g_mgmt_restart_requested;

/* Global config manager for runtime configuration changes */
struct mgmt_config_manager_t;  /* Forward declaration */
extern struct mgmt_config_manager_t *g_config_manager;

/* Define the port number for the server to listen on. */
#define SERVER_PORT 12345
/* Define the port number for the management interface */
#define MGMT_PORT 6969
/* Define the maximum number of pending connections in the listen queue. */
#define MAX_PENDING_CONNECTIONS 5
/* Define a buffer size for network communication */
#define BUFFER_SIZE 1024
/* Define the maximum number of concurrent client connections */
#define MAX_CLIENTS 32
/* Define the maximum number of concurrent management sessions */
#define MAX_MGMT_SESSIONS 4

/* TLS certificate and key path maximum length */
#define TLS_CERT_PATH_MAX 256

/* Operating modes for the XOE application */
typedef enum {
    MODE_HELP,              /* Display help/usage information */
    MODE_SERVER,            /* Run as TCP/TLS server */
    MODE_CLIENT_STANDARD,   /* Run as standard client (stdin/stdout) */
    MODE_CLIENT_SERIAL,     /* Run as serial bridge client */
    MODE_CLIENT_USB         /* Run as USB bridge client */
} xoe_mode_t;

/* FSM states for application flow */
typedef enum {
    STATE_INIT,             /* Initialize configuration defaults */
    STATE_PARSE_ARGS,       /* Parse command-line arguments */
    STATE_VALIDATE_CONFIG,  /* Validate configuration */
    STATE_START_MGMT,       /* Start management interface (one-time) */
    STATE_MODE_SELECT,      /* Determine operating mode */
    STATE_SERVER_MODE,      /* Execute server mode */
    STATE_CLIENT_STD,       /* Execute standard client mode */
    STATE_CLIENT_SERIAL,    /* Execute serial bridge client mode */
    STATE_CLIENT_USB,       /* Execute USB bridge client mode */
    STATE_MODE_STOP,        /* Gracefully stop current mode for restart */
    STATE_APPLY_CONFIG,     /* Apply pending configuration */
    STATE_CLEANUP,          /* Cleanup resources */
    STATE_EXIT              /* Exit application */
} xoe_state_t;

/* Unified configuration structure for XOE application */
typedef struct {
    xoe_mode_t mode;                    /* Operating mode */
    char *listen_address;               /* Server listen address */
    int listen_port;                    /* Server listen port */
    char *connect_server_ip;            /* Client connection IP */
    int connect_server_port;            /* Client connection port */
    int encryption_mode;                /* TLS encryption mode (0=off, 1=on) */
    char cert_path[TLS_CERT_PATH_MAX];  /* TLS certificate path */
    char key_path[TLS_CERT_PATH_MAX];   /* TLS key path */
    int use_serial;                     /* Serial mode flag */
    void *serial_config;                /* Opaque pointer to serial_config_t */
    char *serial_device;                /* Serial device path */
    int use_usb;                        /* USB mode flag */
    void *usb_config;                   /* Opaque pointer to usb_multi_config_t */
    char *program_name;                 /* Program name for usage output */
    int exit_code;                      /* Exit code for application */
    int server_fd;                      /* Server connection file descriptor */
    int mgmt_port;                      /* Management interface port */
    char *mgmt_password;                /* Management password (NULL = no auth) */
    void *mgmt_server;                  /* Opaque pointer to mgmt_server_t */
} xoe_config_t;

/* Forward declarations for state handler functions */
xoe_state_t state_init(xoe_config_t *config);
xoe_state_t state_parse_args(xoe_config_t *config, int argc, char *argv[]);
xoe_state_t state_validate_config(xoe_config_t *config);
xoe_state_t state_start_mgmt(xoe_config_t *config);
xoe_state_t state_mode_select(xoe_config_t *config);
xoe_state_t state_server_mode(xoe_config_t *config);
xoe_state_t state_client_std(xoe_config_t *config);
xoe_state_t state_client_serial(xoe_config_t *config);
xoe_state_t state_client_usb(xoe_config_t *config);
xoe_state_t state_mode_stop(xoe_config_t *config);
xoe_state_t state_apply_config(xoe_config_t *config);
xoe_state_t state_cleanup(xoe_config_t *config);

/* Forward declarations for helper functions used by state handlers */
void print_usage(const char *program_name);

#endif /* CORE_CONFIG_H */
