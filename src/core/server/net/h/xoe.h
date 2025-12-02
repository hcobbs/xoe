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



#endif /* XOE_H */
