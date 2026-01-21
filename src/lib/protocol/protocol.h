#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "lib/common/types.h"

/* Protocol ID constants */
#define XOE_PROTOCOL_RAW    0x0000  /* Raw/echo protocol (future) */
#define XOE_PROTOCOL_SERIAL 0x0001  /* Serial port protocol */
#define XOE_PROTOCOL_USB    0x0002  /* USB device protocol */
#define XOE_PROTOCOL_NBD    0x0003  /* NBD block device protocol */



/**
 * @brief A container structure for the packet's payload.
 *
 * This holds a pointer to the actual data buffer and its length,
 * allowing for flexible and safe data handling.
 *
 * MEMORY OWNERSHIP SEMANTICS:
 * ---------------------------
 * The owns_data flag indicates whether this payload structure owns
 * the data buffer and is responsible for freeing it. This prevents
 * double-free errors when the data buffer is a stack variable or
 * managed externally.
 *
 * Ownership rules:
 * - If owns_data == TRUE: The payload owns the data buffer.
 *   Caller must free both the data buffer (via free()) and the
 *   payload structure itself when done.
 * - If owns_data == FALSE: The payload does NOT own the data buffer.
 *   Caller must only free the payload structure, not the data buffer.
 *   The data buffer is either a stack variable or owned by another
 *   structure.
 *
 * Example (owned data):
 *   xoe_payload_t* p = malloc(sizeof(xoe_payload_t));
 *   p->data = malloc(1024);
 *   p->len = 1024;
 *   p->owns_data = TRUE;
 *   // Later: free(p->data); free(p);
 *
 * Example (non-owned data):
 *   char stack_buf[1024];
 *   xoe_payload_t* p = malloc(sizeof(xoe_payload_t));
 *   p->data = stack_buf;
 *   p->len = 1024;
 *   p->owns_data = FALSE;
 *   // Later: free(p); (do NOT free p->data)
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
 *
 * MEMORY OWNERSHIP SEMANTICS:
 * ---------------------------
 * The xoe_packet_t structure contains a pointer to a dynamically allocated
 * xoe_payload_t structure, which in turn may own its data buffer.
 *
 * Ownership rules:
 * - The packet structure does NOT own itself (caller allocates/frees)
 * - The packet DOES own the payload pointer (packet->payload)
 * - The payload's data ownership depends on payload->owns_data flag
 *
 * Proper cleanup sequence:
 *   1. If packet->payload != NULL:
 *      a. If packet->payload->owns_data == TRUE:
 *         free(packet->payload->data);
 *      b. free(packet->payload);
 *   2. If packet was heap-allocated: free(packet);
 *
 * Example (encapsulation creates owned payload):
 *   xoe_packet_t packet;
 *   usb_protocol_encapsulate(urb, data, len, &packet);
 *   // packet.payload and packet.payload->data are now malloc'd
 *   // Later: usb_protocol_free_payload(&packet);
 *
 * Thread safety: Packets are not thread-safe. Each thread must use
 *                separate packet structures or external synchronization.
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

/**
 * @brief Defines the interface (a "contract") for any protocol handler.
 *
 * MEMORY OWNERSHIP SEMANTICS:
 * ---------------------------
 * Protocol handlers must follow strict memory ownership rules to prevent
 * leaks and double-free errors.
 *
 * init_session():
 * - Allocates and returns a new protocol_session structure
 * - The returned session is owned by the caller (core server)
 * - Returns NULL on allocation failure
 * - Must allocate all session-specific resources
 *
 * handle_data():
 * - Receives a session pointer owned by the caller
 * - May allocate temporary resources (must free before returning)
 * - Must NOT free the session structure itself
 * - Packet structures created during handling must be cleaned up
 *
 * cleanup_session():
 * - Receives a session pointer owned by the caller
 * - MUST free all session-specific resources
 * - MUST free the session structure itself
 * - After cleanup_session() returns, session pointer is invalid
 *
 * Example protocol handler lifecycle:
 *   // Connection established
 *   struct protocol_session* sess = handler->init_session(socket);
 *
 *   // Data received
 *   handler->handle_data(sess);  // May create/free packets internally
 *
 *   // Connection closed
 *   handler->cleanup_session(sess);  // sess is now invalid
 *
 * Thread safety: Protocol handlers must be thread-safe if the server
 *                supports concurrent connections. Each session is
 *                accessed by only one thread, but init_session() may
 *                be called concurrently.
 */
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
