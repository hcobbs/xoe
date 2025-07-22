#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h> // For fixed-width integer types like uint16_t

/**
 * @brief A container structure for the packet's payload.
 *
 * This holds a pointer to the actual data buffer and its length,
 * allowing for flexible and safe data handling.
 */
typedef struct {
    uint32_t len;
    void* data;
} xoe_payload_t;

/**
 * @brief Defines the logical structure for a XOE packet.
 *
 * This structure is used for in-memory representation and manipulation of
 * a packet before it's serialized for network transmission or after it's
 * been parsed from a received byte stream.
 */
typedef struct {
    // The unique identifier for the encapsulated protocol.
    uint16_t protocol_id;
    // The version of the encapsulated protocol.
    uint16_t protocol_version;
    // A pointer to the structure holding the payload data and its length.
    xoe_payload_t* payload;
    // A checksum calculated over the packet contents to ensure data integrity.
    uint32_t checksum;
} xoe_packet_t;

// An opaque pointer to a struct that will hold protocol-specific session data.
// The core server doesn't need to know the contents of this struct.
struct protocol_session;

// Defines the interface (a "contract") for any protocol handler.
typedef struct {
    // A human-readable name for the protocol.
    const char* name;
    // Called to initialize a new session when a client connects.
    struct protocol_session* (*init_session)(int client_socket);
    // Called when there is data to be read from the client.
    void (*handle_data)(struct protocol_session* session);
    // Called to clean up a session when the connection is closed.
    void (*cleanup_session)(struct protocol_session* session);
} protocol_handler_t;

#endif // PROTOCOL_H