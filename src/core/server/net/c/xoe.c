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

int main(int argc, char *argv[]) {
    int listen_port = SERVER_PORT;
    char *listen_address = NULL; /* Default to INADDR_ANY (0.0.0.0) */
    char *connect_server_ip = NULL;
    int connect_server_port = 0;
    int opt = 0;
    char *colon = NULL;
#if TLS_ENABLED
    int encryption_mode = ENCRYPT_NONE; /* Default to no encryption */
    char cert_path[TLS_CERT_PATH_MAX];
    char key_path[TLS_CERT_PATH_MAX];

    /* Initialize with default paths */
    strncpy(cert_path, TLS_DEFAULT_CERT_FILE, TLS_CERT_PATH_MAX - 1);
    cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(key_path, TLS_DEFAULT_KEY_FILE, TLS_CERT_PATH_MAX - 1);
    key_path[TLS_CERT_PATH_MAX - 1] = '\0';
#endif

    while ((opt = getopt(argc, argv, "i:p:c:e:h")) != -1) {
        switch (opt) {
            case 'i':
                listen_address = optarg;
                break;
            case 'p':
                listen_port = atoi(optarg);
                if (listen_port <= 0 || listen_port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'c':
                colon = strchr(optarg, ':');
                if (colon == NULL) {
                    fprintf(stderr, "Invalid server address format. Expected <ip>:<port>.\n");
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                *colon = '\0';
                connect_server_ip = optarg;
                connect_server_port = atoi(colon + 1);
                if (connect_server_port <= 0 || connect_server_port > 65535) {
                    fprintf(stderr, "Invalid server port number: %s\n", colon + 1);
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'e':
#if TLS_ENABLED
                if (strcmp(optarg, "none") == 0) {
                    encryption_mode = ENCRYPT_NONE;
                } else if (strcmp(optarg, "tls12") == 0) {
                    encryption_mode = ENCRYPT_TLS12;
                } else if (strcmp(optarg, "tls13") == 0) {
                    encryption_mode = ENCRYPT_TLS13;
                } else {
                    fprintf(stderr, "Invalid encryption mode: %s\n", optarg);
                    fprintf(stderr, "Valid modes: none, tls12, tls13\n");
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
#else
                fprintf(stderr, "TLS support not compiled in. Rebuild with TLS_ENABLED=1\n");
                exit(EXIT_FAILURE);
#endif
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

#if TLS_ENABLED
    /* Parse remaining arguments for long options (-cert and -key) */
    while (optind < argc) {
        if (strcmp(argv[optind], "-cert") == 0 || strcmp(argv[optind], "--cert") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            strncpy(cert_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else if (strcmp(argv[optind], "-key") == 0 || strcmp(argv[optind], "--key") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option %s requires an argument\n", argv[optind]);
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            strncpy(key_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
            key_path[TLS_CERT_PATH_MAX - 1] = '\0';
            optind += 2;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[optind]);
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
#endif

    if (connect_server_ip != NULL) {
        /* Act as a client */
        int sock = 0;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};
        int bytes_received = 0;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error");
            exit(EXIT_FAILURE);
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(connect_server_port);

        if (inet_pton(AF_INET, connect_server_ip, &serv_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid address/ Address not supported\n");
            exit(EXIT_FAILURE);
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection Failed");
            exit(EXIT_FAILURE);
        }

        printf("Connected to server %s:%d\n", connect_server_ip, connect_server_port);
        printf("Enter messages to send (type 'exit' to quit):\n");

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

            send(sock, buffer, strlen(buffer), 0);
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

    } else {
        /* Act as a server */
        int server_fd = 0;
        int new_socket = 0;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        client_info_t *client_info = NULL;
        pthread_t thread_id;

#if TLS_ENABLED
        /* Initialize TLS context before accepting connections (if encryption enabled) */
        if (encryption_mode != ENCRYPT_NONE) {
            g_tls_ctx = tls_context_init(cert_path, key_path, encryption_mode);
            if (g_tls_ctx == NULL) {
                fprintf(stderr, "Failed to initialize TLS: %s\n", tls_get_error_string());
                fprintf(stderr, "Make sure certificates exist at:\n");
                fprintf(stderr, "  %s\n", cert_path);
                fprintf(stderr, "  %s\n", key_path);
                fprintf(stderr, "Run './scripts/generate_test_certs.sh' to generate test certificates.\n");
                exit(EXIT_FAILURE);
            }
            if (encryption_mode == ENCRYPT_TLS12) {
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
            exit(EXIT_FAILURE);
        }

        /* Set up address structure */
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; /* Default to all interfaces */
        if (listen_address != NULL) {
            if (inet_pton(AF_INET, listen_address, &address.sin_addr) <= 0) {
                fprintf(stderr, "Invalid listen address: %s\n", listen_address);
                exit(EXIT_FAILURE);
            }
        }
        address.sin_port = htons(listen_port);

        /* Bind the socket to the specified IP and port */
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        /* Listen for incoming connections */
        if (listen(server_fd, 10) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        /* Initialize the client pool */
        init_client_pool();

        printf("Server listening on %s:%d\n", (listen_address == NULL) ? "0.0.0.0" : listen_address, listen_port);

        while (TRUE) {
            client_info = acquire_client_slot();
            if (client_info == NULL) {
                fprintf(stderr, "Max clients (%d) reached, rejecting connection\n", MAX_CLIENTS);
                /* Still need to accept and close to prevent backlog */
                new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
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

            if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) != 0) {
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
    }

    return 0;
}
