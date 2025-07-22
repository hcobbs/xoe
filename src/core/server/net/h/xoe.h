#ifndef XOE_H
#define XOE_H

#include <stdint.h> // For fixed-width integer types like uint16_t

// Define the port number for the server to listen on.
#define SERVER_PORT 12345
// Define the maximum number of pending connections in the listen queue.
#define MAX_PENDING_CONNECTIONS 5
// Define a buffer size for network communication
#define BUFFER_SIZE 1024

#endif // XOE_H