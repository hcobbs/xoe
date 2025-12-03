#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "commonDefinitions.h"
#include "xoe.h"

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
