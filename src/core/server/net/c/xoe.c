#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* POSIX/Unix headers only - Windows support removed */
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 *  Definitions needed by Xoe only... Standard C libraries should be cross-compile
 *  safe and appropriately included above
 */
#include "commonDefinitions.h"
#include "xoe.h"

/* TLS includes - include config first to get TLS_ENABLED */
#include "tls_config.h"
#if TLS_ENABLED
#include "tls_context.h"
#include "tls_session.h"
#include "tls_io.h"
#include "tls_error.h"
#endif

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
    int in_use;
#if TLS_ENABLED
    SSL* tls_session;
#endif
} client_info_t;

/* Global fixed-size client pool */
static client_info_t client_pool[MAX_CLIENTS];
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global TLS context (read-only after initialization, thread-safe) */
#if TLS_ENABLED
static SSL_CTX* g_tls_ctx = NULL;
#endif

/* Acquire a client slot from the pool */
static client_info_t* acquire_client_slot(void) {
    int i;
    client_info_t *slot = NULL;

    pthread_mutex_lock(&pool_mutex);
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (!client_pool[i].in_use) {
            client_pool[i].in_use = 1;
            slot = &client_pool[i];
            break;
        }
    }
    pthread_mutex_unlock(&pool_mutex);

    return slot;
}

/* Release a client slot back to the pool */
static void release_client_slot(client_info_t *slot) {
    if (slot != NULL) {
        pthread_mutex_lock(&pool_mutex);
        slot->in_use = 0;
        slot->client_socket = -1;
        pthread_mutex_unlock(&pool_mutex);
    }
}

/* Initialize the client pool */
static void init_client_pool(void) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        client_pool[i].in_use = 0;
        client_pool[i].client_socket = -1;
    }
}

void *handle_client(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_addr = client_info->client_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

#if TLS_ENABLED
    SSL* tls = NULL;
#endif

    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

#if TLS_ENABLED
    /* Perform TLS handshake (if encryption enabled) */
    if (g_tls_ctx != NULL) {
        tls = tls_session_create(g_tls_ctx, client_socket);
        if (tls == NULL) {
            fprintf(stderr, "TLS handshake failed with %s:%d: %s\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                    tls_get_error_string());
            goto cleanup;
        }
        client_info->tls_session = tls;
        printf("TLS handshake successful with %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }
#endif

    /* Echo loop - use TLS or plain TCP based on runtime mode */
    while (1) {
#if TLS_ENABLED
        if (tls != NULL) {
            bytes_received = tls_read(tls, buffer, BUFFER_SIZE - 1);
        } else {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        }
#else
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
#endif
        if (bytes_received <= 0) {
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Received from %s:%d: %s", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

#if TLS_ENABLED
        if (tls != NULL) {
            if (tls_write(tls, buffer, bytes_received) <= 0) {
                fprintf(stderr, "TLS write failed\n");
                break;
            }
        } else {
            send(client_socket, buffer, bytes_received, 0);
        }
#else
        send(client_socket, buffer, bytes_received, 0);
#endif
    }

    if (bytes_received == 0) {
        printf("Client %s:%d disconnected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    } else if (bytes_received == -1) {
#if TLS_ENABLED
        if (tls != NULL) {
            fprintf(stderr, "TLS read error: %s\n", tls_get_error_string());
        } else {
            perror("recv failed");
        }
#else
        perror("recv failed");
#endif
    }

cleanup:
#if TLS_ENABLED
    if (tls != NULL) {
        tls_session_shutdown(tls);
        tls_session_destroy(tls);
    }
#endif

    close(client_socket);
    release_client_slot(client_info);
    pthread_exit(NULL);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Server Mode Options:\n");
    printf("  -i <interface>    Network interface to bind (default: 0.0.0.0, all interfaces)\n");
    printf("                    Examples: 127.0.0.1, eth0, 192.168.1.100\n\n");
    printf("  -p <port>         Port to listen on (default: %d)\n", SERVER_PORT);
    printf("                    Range: 1-65535\n\n");
#if TLS_ENABLED
    printf("  -e <mode>         Encryption mode (default: none)\n");
    printf("                    none  - Plain TCP (no encryption)\n");
    printf("                    tls12 - TLS 1.2 encryption\n");
    printf("                    tls13 - TLS 1.3 encryption (recommended)\n\n");
    printf("  -cert <path>      Path to server certificate (default: %s)\n", TLS_DEFAULT_CERT_FILE);
    printf("                    Required for TLS modes\n\n");
    printf("  -key <path>       Path to server private key (default: %s)\n", TLS_DEFAULT_KEY_FILE);
    printf("                    Required for TLS modes\n\n");
#endif
    printf("Client Mode Options:\n");
    printf("  -c <ip>:<port>    Connect to server as client\n");
    printf("                    Example: -c 192.168.1.100:12345\n\n");
    printf("General Options:\n");
    printf("  -h                Show this help message\n\n");
    printf("Examples:\n");
#if TLS_ENABLED
    printf("  %s -e none                          # Plain TCP server\n", prog_name);
    printf("  %s -e tls13 -p 8443                 # TLS 1.3 server on port 8443\n", prog_name);
#else
    printf("  %s -p 12345                         # TCP server on port 12345\n", prog_name);
#endif
    printf("  %s -c 127.0.0.1:12345               # Connect as client\n", prog_name);
}

/**
 * @brief Initialize configuration with default values
 *
 * Sets up the configuration structure with default values for all fields.
 * This is the first state in the FSM.
 *
 * @param config Pointer to configuration structure to initialize
 * @return STATE_PARSE_ARGS to proceed to argument parsing
 */
xoe_state_t state_init(xoe_config_t *config) {
    /* Initialize all fields to defaults */
    config->mode = MODE_SERVER;  /* Default mode is server */
    config->listen_address = NULL;  /* Default to INADDR_ANY (0.0.0.0) */
    config->listen_port = SERVER_PORT;
    config->connect_server_ip = NULL;
    config->connect_server_port = 0;
    config->program_name = NULL;  /* Will be set during argument parsing */
    config->exit_code = EXIT_SUCCESS;

#if TLS_ENABLED
    /* Initialize TLS configuration with defaults */
    config->encryption_mode = ENCRYPT_NONE;
    strncpy(config->cert_path, TLS_DEFAULT_CERT_FILE, TLS_CERT_PATH_MAX - 1);
    config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(config->key_path, TLS_DEFAULT_KEY_FILE, TLS_CERT_PATH_MAX - 1);
    config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
#else
    config->encryption_mode = 0;  /* No TLS support */
    config->cert_path[0] = '\0';
    config->key_path[0] = '\0';
#endif

    return STATE_PARSE_ARGS;
}

/**
 * @brief Parse command-line arguments
 *
 * Processes all command-line arguments and populates the configuration
 * structure. Handles both short options (-p, -i, etc.) and long options
 * (--cert, --key) in a unified manner.
 *
 * @param config Pointer to configuration structure to populate
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @return STATE_VALIDATE_CONFIG on success, STATE_CLEANUP on error or -h flag
 */
xoe_state_t state_parse_args(xoe_config_t *config, int argc, char *argv[]) {
    int opt = 0;
    char *colon = NULL;

    /* Save program name for usage messages */
    config->program_name = argv[0];

    /* Parse short options using getopt() */
    while ((opt = getopt(argc, argv, "i:p:c:e:h")) != -1) {
        switch (opt) {
            case 'i':
                config->listen_address = optarg;
                break;

            case 'p':
                config->listen_port = atoi(optarg);
                if (config->listen_port <= 0 || config->listen_port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                break;

            case 'c':
                colon = strchr(optarg, ':');
                if (colon == NULL) {
                    fprintf(stderr, "Invalid server address format. Expected <ip>:<port>.\n");
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                *colon = '\0';
                config->connect_server_ip = optarg;
                config->connect_server_port = atoi(colon + 1);
                if (config->connect_server_port <= 0 ||
                    config->connect_server_port > 65535) {
                    fprintf(stderr, "Invalid server port number: %s\n", colon + 1);
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
                break;

            case 'e':
#if TLS_ENABLED
                if (strcmp(optarg, "none") == 0) {
                    config->encryption_mode = ENCRYPT_NONE;
                } else if (strcmp(optarg, "tls12") == 0) {
                    config->encryption_mode = ENCRYPT_TLS12;
                } else if (strcmp(optarg, "tls13") == 0) {
                    config->encryption_mode = ENCRYPT_TLS13;
                } else {
                    fprintf(stderr, "Invalid encryption mode: %s\n", optarg);
                    fprintf(stderr, "Valid modes: none, tls12, tls13\n");
                    print_usage(argv[0]);
                    config->exit_code = EXIT_FAILURE;
                    return STATE_CLEANUP;
                }
#else
                fprintf(stderr, "TLS support not compiled in. Rebuild with TLS_ENABLED=1\n");
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
#endif
                break;

            case 'h':
                config->mode = MODE_HELP;
                print_usage(argv[0]);
                config->exit_code = EXIT_SUCCESS;
                return STATE_CLEANUP;

            default:
                print_usage(argv[0]);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
        }
    }

#if TLS_ENABLED
    /* Parse remaining arguments for long options (--cert and --key) */
    while (optind < argc) {
        if (strcmp(argv[optind], "-cert") == 0 ||
            strcmp(argv[optind], "--cert") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(argv[0]);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            strncpy(config->cert_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            config->cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else if (strcmp(argv[optind], "-key") == 0 ||
                   strcmp(argv[optind], "--key") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(argv[0]);
                config->exit_code = EXIT_FAILURE;
                return STATE_CLEANUP;
            }
            strncpy(config->key_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            config->key_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[optind]);
            print_usage(argv[0]);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }
#endif

    return STATE_VALIDATE_CONFIG;
}

/**
 * @brief Validate configuration
 *
 * Performs validation of the configuration to ensure all settings are
 * consistent and valid. Currently minimal validation since most is done
 * during parsing.
 *
 * @param config Pointer to configuration structure to validate
 * @return STATE_MODE_SELECT to proceed to mode selection
 */
xoe_state_t state_validate_config(xoe_config_t *config) {
    /* Most validation is done during argument parsing */
    /* Add any cross-field validation here if needed in the future */
    (void)config;  /* Suppress unused parameter warning */
    return STATE_MODE_SELECT;
}

/**
 * @brief Determine operating mode
 *
 * Analyzes the configuration to determine which operating mode to enter.
 * The mode is determined by which parameters were specified on the command line.
 *
 * @param config Pointer to configuration structure
 * @return Appropriate state for the selected mode
 */
xoe_state_t state_mode_select(xoe_config_t *config) {
    /* Mode already set during parsing if -h flag was used */
    if (config->mode == MODE_HELP) {
        return STATE_CLEANUP;
    }

    /* If connect_server_ip is set, we're in client mode */
    if (config->connect_server_ip != NULL) {
        config->mode = MODE_CLIENT_STANDARD;
        return STATE_CLIENT_STD;
    }

    /* Otherwise, we're in server mode */
    config->mode = MODE_SERVER;
    return STATE_SERVER_MODE;
}

/**
 * @brief Cleanup resources
 *
 * Performs any cleanup needed before exiting the application.
 * Currently minimal since most resources are automatically cleaned up.
 *
 * @param config Pointer to configuration structure
 * @return STATE_EXIT to signal application should exit
 */
xoe_state_t state_cleanup(xoe_config_t *config) {
    /* Cleanup would go here if we had dynamically allocated resources */
    /* For now, just transition to exit state */
    (void)config;  /* Suppress unused parameter warning */
    return STATE_EXIT;
}

/**
 * @brief Execute standard client mode
 *
 * Connects to the specified server and provides an interactive stdin/stdout
 * interface for sending and receiving messages. The user can type messages
 * which are sent to the server, and responses are displayed.
 *
 * @param config Pointer to configuration structure
 * @return STATE_CLEANUP when client exits
 */
xoe_state_t state_client_std(xoe_config_t *config) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = 0;

    /* Create socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Set up server address structure */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config->connect_server_port);

    /* Convert IP address from text to binary */
    if (inet_pton(AF_INET, config->connect_server_ip,
                  &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        close(sock);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Connect to server */
    if (connect(sock, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    printf("Connected to server %s:%d\n",
           config->connect_server_ip, config->connect_server_port);
    printf("Enter messages to send (type 'exit' to quit):\n");

    /* Main client loop - read from stdin, send to server, display response */
    while (TRUE) {
        printf("> ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break; /* EOF or error */
        }

        /* Remove newline character if present */
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        /* Send message to server */
        send(sock, buffer, strlen(buffer), 0);

        /* Receive response from server */
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Server response: %s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Server disconnected.\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
    }

    close(sock);
    printf("Client disconnected.\n");

    return STATE_CLEANUP;
}

/**
 * @brief Execute server mode
 *
 * Creates a TCP server socket, optionally initializes TLS, and listens for
 * incoming client connections. Spawns a new thread for each client connection.
 * Runs until interrupted.
 *
 * @param config Pointer to configuration structure
 * @return STATE_CLEANUP when server exits
 */
xoe_state_t state_server_mode(xoe_config_t *config) {
    int server_fd = 0;
    int new_socket = 0;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    client_info_t *client_info = NULL;
    pthread_t thread_id;

#if TLS_ENABLED
    /* Initialize TLS context before accepting connections (if encryption enabled) */
    if (config->encryption_mode != ENCRYPT_NONE) {
        g_tls_ctx = tls_context_init(config->cert_path, config->key_path,
                                      config->encryption_mode);
        if (g_tls_ctx == NULL) {
            fprintf(stderr, "Failed to initialize TLS: %s\n",
                    tls_get_error_string());
            fprintf(stderr, "Make sure certificates exist at:\n");
            fprintf(stderr, "  %s\n", config->cert_path);
            fprintf(stderr, "  %s\n", config->key_path);
            fprintf(stderr, "Run './scripts/generate_test_certs.sh' to generate test certificates.\n");
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
        if (config->encryption_mode == ENCRYPT_TLS12) {
            printf("TLS 1.2 enabled\n");
        } else {
            printf("TLS 1.3 enabled\n");
        }
    } else {
        printf("Running in plain TCP mode (no encryption)\n");
    }
#endif

    /* Create socket file descriptor */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Set up address structure */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; /* Default to all interfaces */
    if (config->listen_address != NULL) {
        if (inet_pton(AF_INET, config->listen_address,
                      &address.sin_addr) <= 0) {
            fprintf(stderr, "Invalid listen address: %s\n",
                    config->listen_address);
            close(server_fd);
            config->exit_code = EXIT_FAILURE;
            return STATE_CLEANUP;
        }
    }
    address.sin_port = htons(config->listen_port);

    /* Bind the socket to the specified IP and port */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Listen for incoming connections */
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        config->exit_code = EXIT_FAILURE;
        return STATE_CLEANUP;
    }

    /* Initialize the client pool */
    init_client_pool();

    printf("Server listening on %s:%d\n",
           (config->listen_address == NULL) ? "0.0.0.0" : config->listen_address,
           config->listen_port);

    /* Main server loop - accept connections and spawn threads */
    while (TRUE) {
        client_info = acquire_client_slot();
        if (client_info == NULL) {
            fprintf(stderr, "Max clients (%d) reached, rejecting connection\n",
                    MAX_CLIENTS);
            /* Still need to accept and close to prevent backlog */
            new_socket = accept(server_fd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen);
            if (new_socket >= 0) {
                close(new_socket);
            }
            continue;
        }

        new_socket = accept(server_fd,
                            (struct sockaddr *)&client_info->client_addr,
                            (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            release_client_slot(client_info);
            continue;
        }
        client_info->client_socket = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client,
                           (void *)client_info) != 0) {
            perror("pthread_create failed");
            close(new_socket);
            release_client_slot(client_info);
        }
        pthread_detach(thread_id); /* Detach thread to clean up resources automatically */
    }

#if TLS_ENABLED
    /* Cleanup TLS context on shutdown */
    tls_context_cleanup(g_tls_ctx);
#endif

    close(server_fd);
    return STATE_CLEANUP;
}

/**
 * @brief Main entry point - FSM-based architecture
 *
 * Implements a finite state machine to manage application flow.
 * This replaces the previous monolithic 267-line main() function with
 * a clean, testable, and maintainable state-based architecture.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code (EXIT_SUCCESS or EXIT_FAILURE)
 */
int main(int argc, char *argv[]) {
    xoe_config_t config;
    xoe_state_t state = STATE_INIT;

    /* FSM loop - execute states until STATE_EXIT is reached */
    while (state != STATE_EXIT) {
        switch (state) {
            case STATE_INIT:
                state = state_init(&config);
                break;

            case STATE_PARSE_ARGS:
                state = state_parse_args(&config, argc, argv);
                break;

            case STATE_VALIDATE_CONFIG:
                state = state_validate_config(&config);
                break;

            case STATE_MODE_SELECT:
                state = state_mode_select(&config);
                break;

            case STATE_SERVER_MODE:
                state = state_server_mode(&config);
                break;

            case STATE_CLIENT_STD:
                state = state_client_std(&config);
                break;

            case STATE_CLEANUP:
                state = state_cleanup(&config);
                break;

            default:
                /* Invalid state - should never happen */
                fprintf(stderr, "Error: Invalid state %d\n", state);
                config.exit_code = EXIT_FAILURE;
                state = STATE_EXIT;
                break;
        }
    }

    return config.exit_code;
}
