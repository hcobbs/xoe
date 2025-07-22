#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "xoe.h"

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

void *handle_client(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_addr = client_info->client_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

#ifdef _WIN32
    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
#else
    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
#endif

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("Received from %s:%d: %s", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);
        send(client_socket, buffer, bytes_received, 0); // Echo back
    }

    if (bytes_received == 0) {
        printf("Client %s:%d disconnected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    } else if (bytes_received == -1) {
        perror("recv failed");
    }

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
    free(client_info);
    pthread_exit(NULL);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [-i <interface>] [-p <port>] [-c <server_ip>:<server_port>]\n", prog_name);
    printf("  -i <interface>  Specify the network interface to listen on (e.g., eth0, lo).\n");
    printf("                  If not specified, listens on all available interfaces (0.0.0.0).\n");
    printf("  -p <port>       Specify the port to listen on (default: %d).\n", DEFAULT_PORT);
    printf("  -c <server_ip>:<server_port> Connect to another server as a client.\n");
    printf("                  If this option is used, the program will act as a client instead of a server.\n");
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
#endif
    int listen_port = DEFAULT_PORT;
    char *listen_address = NULL; // Default to INADDR_ANY (0.0.0.0)
    char *connect_server_ip = NULL;
    int connect_server_port = 0;

    int opt;
    while ((opt = getopt(argc, argv, "i:p:c:")) != -1) {
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
            case 'c': {
                char *colon = strchr(optarg, ':');
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
            }
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (connect_server_ip != NULL) {
        // Act as a client
        int sock = 0;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};

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

        while (1) {
            printf("> ");
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                break; // EOF or error
            }

            // Remove newline character if present
            buffer[strcspn(buffer, "\n")] = 0;

            if (strcmp(buffer, "exit") == 0) {
                break;
            }

            send(sock, buffer, strlen(buffer), 0);
            int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
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

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        printf("Client disconnected.\n");

    } else {
        // Act as a server
        int server_fd, new_socket;
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        // Create socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // Set up address structure
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY; // Default to all interfaces
        if (listen_address != NULL) {
            if (inet_pton(AF_INET, listen_address, &address.sin_addr) <= 0) {
                fprintf(stderr, "Invalid listen address: %s\n", listen_address);
                exit(EXIT_FAILURE);
            }
        }
        address.sin_port = htons(listen_port);

        // Bind the socket to the specified IP and port
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        // Listen for incoming connections
        if (listen(server_fd, 10) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        printf("Server listening on %s:%d\n", (listen_address == NULL) ? "0.0.0.0" : listen_address, listen_port);

        while (TRUE) {
            client_info_t *client_info = (client_info_t *)malloc(sizeof(client_info_t));
            if (client_info == NULL) {
                perror("malloc failed");
                continue;
            }

            if ((new_socket = accept(server_fd, (struct sockaddr *)&client_info->client_addr, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                free(client_info);
                continue;
            }
            client_info->client_socket = new_socket;

            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) != 0) {
                perror("pthread_create failed");
                close(new_socket);
                free(client_info);
            }
            pthread_detach(thread_id); // Detach thread to clean up resources automatically
        }

#ifdef _WIN32
        closesocket(server_fd);
#else
        close(server_fd);
#endif
    }

    return 0;
}
                