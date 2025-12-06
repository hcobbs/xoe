#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "lib/common/types.h"

/* Protocol ID constants */
#define XOE_PROTOCOL_RAW    0x0000  /* Raw/echo protocol (future) */
#define XOE_PROTOCOL_SERIAL 0x0001  /* Serial port protocol */
#define XOE_PROTOCOL_USB    0x0002  /* USB device protocol */



/**
 * @brief A container structure for the packet's payload.
 *
 * This holds a pointer to the actual data buffer and its length,
 * allowing for flexible and safe data handling.
 *
 * The owns_data flag indicates whether this payload structure owns
 * the data buffer and is responsible for freeing it. This prevents
 * double-free errors when the data buffer is a stack variable or
 * managed externally.
 */
typedef struct {
    uint32_t len;       /* Length of data in bytes */
    void* data;         /* Pointer to data buffer */
    int owns_data;      /* TRUE if data was malloc'd and should be freed */
} xoe_payload_t;

/**
 * @brief Defines the logical structure for a XOE packet.
 *
 * This structure is used for in-memory representation and manipulation of
 * a packet before it's serialized for network transmission or after it's
 * been parsed from a received byte stream.
 */
typedef struct {
    /* The unique identifier for the encapsulated protocol. */
    uint16_t protocol_id;
    /* The version of the encapsulated protocol. */
    uint16_t protocol_version;
    /* A pointer to the structure holding the payload data and its length. */
    xoe_payload_t* payload;
    /* A checksum calculated over the packet contents to ensure data integrity. */
    uint32_t checksum;
} xoe_packet_t;

/* An opaque pointer to a struct that will hold protocol-specific session data. */
/* The core server doesn't need to know the contents of this struct. */
struct protocol_session;

/* Defines the interface (a "contract") for any protocol handler. */
typedef struct {
    /* A human-readable name for the protocol. */
    const char* name;
    /* Called to initialize a new session when a client connects. */
    struct protocol_session* (*init_session)(int client_socket);
    /* Called when there is data to be read from the client. */
    void (*handle_data)(struct protocol_session* session);
    /* Called to clean up a session when the connection is closed. */
    void (*cleanup_session)(struct protocol_session* session);
} protocol_handler_t;

#endif /* PROTOCOL_H */
