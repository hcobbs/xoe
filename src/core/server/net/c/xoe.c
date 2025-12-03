#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

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

/* Serial connector includes */
#include "serial_config.h"
#include "serial_port.h"
#include "serial_protocol.h"

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
    printf("Serial Connector Options (requires -c for client mode):\n");
    printf("  -s <device>       Serial device path (e.g., /dev/ttyUSB0)\n");
    printf("                    Enables serial-to-network bridging\n\n");
    printf("  -b <baud>         Baud rate (default: 9600)\n");
    printf("                    Common rates: 9600, 19200, 38400, 57600, 115200\n\n");
    printf("  --parity <mode>   Parity (default: none)\n");
    printf("                    Options: none, even, odd\n\n");
    printf("  --databits <n>    Data bits (default: 8)\n");
    printf("                    Options: 7, 8\n\n");
    printf("  --stopbits <n>    Stop bits (default: 1)\n");
    printf("                    Options: 1, 2\n\n");
    printf("  --flow <mode>     Flow control (default: none)\n");
    printf("                    Options: none, xonxoff, rtscts\n\n");
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
    printf("  %s -c 192.168.1.100:12345 -s /dev/ttyUSB0 -b 115200\n", prog_name);
    printf("                                      # Serial bridge at 115200 baud\n");
}

int main(int argc, char *argv[]) {
    int listen_port = SERVER_PORT;
    char *listen_address = NULL; /* Default to INADDR_ANY (0.0.0.0) */
    char *connect_server_ip = NULL;
    int connect_server_port = 0;
    int opt = 0;
    char *colon = NULL;
    serial_config_t serial_config;
    char *serial_device = NULL;
    int use_serial = FALSE;
#if TLS_ENABLED
    int encryption_mode = ENCRYPT_NONE; /* Default to no encryption */
    char cert_path[TLS_CERT_PATH_MAX];
    char key_path[TLS_CERT_PATH_MAX];
#endif

    /* Initialize serial config with defaults */
    serial_config_init_defaults(&serial_config);

#if TLS_ENABLED
    /* Initialize with default paths */
    strncpy(cert_path, TLS_DEFAULT_CERT_FILE, TLS_CERT_PATH_MAX - 1);
    cert_path[TLS_CERT_PATH_MAX - 1] = '\0';
    strncpy(key_path, TLS_DEFAULT_KEY_FILE, TLS_CERT_PATH_MAX - 1);
    key_path[TLS_CERT_PATH_MAX - 1] = '\0';
#endif

    while ((opt = getopt(argc, argv, "i:p:c:e:s:b:h")) != -1) {
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
            case 's':
                serial_device = optarg;
                use_serial = TRUE;
                break;
            case 'b':
                serial_config.baud_rate = atoi(optarg);
                if (serial_config.baud_rate <= 0) {
                    fprintf(stderr, "Invalid baud rate: %s\n", optarg);
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /* Parse remaining arguments for long options */
    while (optind < argc) {
#if TLS_ENABLED
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
        } else
#endif
        if (strcmp(argv[optind], "--parity") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --parity requires an argument\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (strcmp(argv[optind + 1], "none") == 0) {
                serial_config.parity = SERIAL_PARITY_NONE;
            } else if (strcmp(argv[optind + 1], "even") == 0) {
                serial_config.parity = SERIAL_PARITY_EVEN;
            } else if (strcmp(argv[optind + 1], "odd") == 0) {
                serial_config.parity = SERIAL_PARITY_ODD;
            } else {
                fprintf(stderr, "Invalid parity: %s (use none, even, or odd)\n", argv[optind + 1]);
                exit(EXIT_FAILURE);
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--databits") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --databits requires an argument\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            serial_config.data_bits = atoi(argv[optind + 1]);
            if (serial_config.data_bits != 7 && serial_config.data_bits != 8) {
                fprintf(stderr, "Invalid data bits: %s (use 7 or 8)\n", argv[optind + 1]);
                exit(EXIT_FAILURE);
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--stopbits") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --stopbits requires an argument\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            serial_config.stop_bits = atoi(argv[optind + 1]);
            if (serial_config.stop_bits != 1 && serial_config.stop_bits != 2) {
                fprintf(stderr, "Invalid stop bits: %s (use 1 or 2)\n", argv[optind + 1]);
                exit(EXIT_FAILURE);
            }
            optind += 2;
        } else if (strcmp(argv[optind], "--flow") == 0) {
            if (optind + 1 >= argc) {
                fprintf(stderr, "Option --flow requires an argument\n");
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (strcmp(argv[optind + 1], "none") == 0) {
                serial_config.flow_control = SERIAL_FLOW_NONE;
            } else if (strcmp(argv[optind + 1], "xonxoff") == 0) {
                serial_config.flow_control = SERIAL_FLOW_XONXOFF;
            } else if (strcmp(argv[optind + 1], "rtscts") == 0) {
                serial_config.flow_control = SERIAL_FLOW_RTSCTS;
            } else {
                fprintf(stderr, "Invalid flow control: %s (use none, xonxoff, or rtscts)\n",
                        argv[optind + 1]);
                exit(EXIT_FAILURE);
            }
            optind += 2;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[optind]);
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* Validate serial mode configuration */
    if (use_serial) {
        if (connect_server_ip == NULL) {
            fprintf(stderr, "Serial mode (-s) requires client mode (-c)\n");
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        if (serial_device == NULL) {
            fprintf(stderr, "Serial device not specified\n");
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        /* Set device path in config */
        strncpy(serial_config.device_path, serial_device, SERIAL_DEVICE_PATH_MAX - 1);
        serial_config.device_path[SERIAL_DEVICE_PATH_MAX - 1] = '\0';
    }

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

        if (use_serial) {
            /* Serial client mode - bridge serial port to network */
            int serial_fd = 0;
            int result = 0;
            int serial_bytes = 0;
            int net_bytes = 0;
            unsigned char serial_buffer[SERIAL_READ_CHUNK_SIZE];
            xoe_packet_t tx_packet;
            xoe_packet_t rx_packet;
            uint32_t actual_len = 0;
            uint16_t sequence_tx = 0;
            uint16_t sequence_rx = 0;
            uint16_t flags = 0;

            printf("Serial mode enabled: %s at %d baud\n",
                   serial_config.device_path, serial_config.baud_rate);

            /* Open serial port */
            result = serial_port_open(&serial_config, &serial_fd);
            if (result != 0) {
                fprintf(stderr, "Failed to open serial port %s: error %d\n",
                        serial_config.device_path, result);
                close(sock);
                exit(EXIT_FAILURE);
            }

            printf("Serial port opened successfully\n");
            printf("Serial bridge active (Ctrl+C to exit)\n");

            /* Simple single-threaded loop for Phase 3 */
            /* TODO: Phase 4 will add multi-threading and proper flow control */
            while (TRUE) {
                /* Read from serial port (non-blocking with short timeout) */
                serial_bytes = serial_port_read(serial_fd, serial_buffer,
                                                SERIAL_READ_CHUNK_SIZE, 10);
                if (serial_bytes > 0) {
                    /* Encapsulate and send to network */
                    result = serial_protocol_encapsulate(serial_buffer, serial_bytes,
                                                         sequence_tx++, 0, &tx_packet);
                    if (result == 0) {
                        /* For now, send raw payload data (Phase 4 will send full packet) */
                        send(sock, tx_packet.payload->data, tx_packet.payload->len, 0);
                        serial_protocol_free_payload(&tx_packet);
                    }
                } else if (serial_bytes < 0) {
                    fprintf(stderr, "Serial read error: %d\n", serial_bytes);
                    break;
                }

                /* Read from network (non-blocking) */
                net_bytes = recv(sock, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
                if (net_bytes > 0) {
                    /* For now, write raw data to serial (Phase 4 will decapsulate packets) */
                    serial_port_write(serial_fd, buffer, net_bytes);
                } else if (net_bytes == 0) {
                    printf("Server disconnected.\n");
                    break;
                } else if (net_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("recv failed");
                    break;
                }
            }

            serial_port_close(serial_fd);
            printf("Serial port closed\n");
        } else {
            /* Standard client mode - stdin/stdout */
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
