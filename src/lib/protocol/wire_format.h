/*
 * wire_format.h - XOE Wire Protocol Format
 *
 * Defines the network wire format for XOE packets. This format uses fixed-size
 * fields with explicit byte ordering and NO POINTERS, ensuring cross-platform
 * compatibility between 32-bit and 64-bit systems.
 *
 * SECURITY FIX: Addresses LIB-001/NET-006 - eliminates pointer transmission
 *
 * Wire Format (12 bytes header + variable payload):
 *   Offset  Size  Field
 *   ------  ----  -----
 *   0       2     protocol_id (big-endian)
 *   2       2     protocol_version (big-endian)
 *   4       4     payload_length (big-endian)
 *   8       4     checksum (big-endian)
 *   12      N     payload data (N = payload_length)
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-24
 */

#ifndef WIRE_FORMAT_H
#define WIRE_FORMAT_H

#include "lib/common/types.h"
#include "lib/protocol/protocol.h"

/* Wire protocol version - increment on breaking changes */
#define XOE_WIRE_VERSION 2

/* Wire header size in bytes (fixed across all platforms) */
#define XOE_WIRE_HEADER_SIZE 12

/* Maximum payload size (1MB limit for safety) */
#define XOE_WIRE_MAX_PAYLOAD (1024 * 1024)

/**
 * @brief Wire format header structure (for documentation only)
 *
 * This structure represents the logical layout of the wire header.
 * Actual serialization uses explicit byte manipulation to ensure
 * correct byte ordering regardless of platform.
 *
 * DO NOT use sizeof() or cast network data to this structure.
 * Use xoe_wire_serialize_header() and xoe_wire_deserialize_header().
 */
typedef struct {
    uint16_t protocol_id;       /* Protocol identifier */
    uint16_t protocol_version;  /* Protocol version */
    uint32_t payload_length;    /* Length of payload data in bytes */
    uint32_t checksum;          /* CRC32 checksum of header + payload */
} xoe_wire_header_t;

/*
 * Byte order conversion helpers (big-endian / network byte order)
 */

/**
 * @brief Write 16-bit value in big-endian format
 */
void xoe_wire_write_uint16(uint8_t* buffer, uint16_t value);

/**
 * @brief Write 32-bit value in big-endian format
 */
void xoe_wire_write_uint32(uint8_t* buffer, uint32_t value);

/**
 * @brief Read 16-bit value from big-endian format
 */
uint16_t xoe_wire_read_uint16(const uint8_t* buffer);

/**
 * @brief Read 32-bit value from big-endian format
 */
uint32_t xoe_wire_read_uint32(const uint8_t* buffer);

/*
 * Header serialization
 */

/**
 * @brief Serialize wire header to byte buffer
 *
 * @param buffer    Output buffer (must be at least XOE_WIRE_HEADER_SIZE bytes)
 * @param header    Header structure to serialize
 */
void xoe_wire_serialize_header(uint8_t* buffer, const xoe_wire_header_t* header);

/**
 * @brief Deserialize wire header from byte buffer
 *
 * @param header    Output header structure
 * @param buffer    Input buffer (must be at least XOE_WIRE_HEADER_SIZE bytes)
 */
void xoe_wire_deserialize_header(xoe_wire_header_t* header, const uint8_t* buffer);

/*
 * Network I/O functions
 */

/**
 * @brief Send an XOE packet over a socket
 *
 * Serializes the packet to wire format and sends header + payload.
 * Calculates checksum over the entire packet.
 *
 * @param fd        Socket file descriptor
 * @param packet    Packet to send (internal representation)
 *
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT if packet or payload is NULL
 *         E_IO_ERROR on send failure
 */
int xoe_wire_send(int fd, const xoe_packet_t* packet);

/**
 * @brief Receive an XOE packet from a socket
 *
 * Receives wire format header + payload and deserializes to packet structure.
 * Validates checksum and allocates payload buffer.
 *
 * @param fd        Socket file descriptor
 * @param packet    Output packet (payload will be allocated)
 *
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT if packet is NULL
 *         E_IO_ERROR on receive failure
 *         E_CHECKSUM_MISMATCH if checksum validation fails
 *         E_OUT_OF_MEMORY if payload allocation fails
 *         E_PROTOCOL_ERROR if payload_length exceeds maximum
 *
 * @note Caller is responsible for freeing packet->payload and packet->payload->data
 */
int xoe_wire_recv(int fd, xoe_packet_t* packet);

/**
 * @brief Send an XOE packet over a TLS connection
 *
 * TLS-enabled version of xoe_wire_send().
 *
 * @param ssl       OpenSSL SSL pointer (cast to void* for header compatibility)
 * @param packet    Packet to send
 *
 * @return 0 on success, negative error code on failure
 */
int xoe_wire_send_tls(void* ssl, const xoe_packet_t* packet);

/**
 * @brief Receive an XOE packet from a TLS connection
 *
 * TLS-enabled version of xoe_wire_recv().
 *
 * @param ssl       OpenSSL SSL pointer
 * @param packet    Output packet
 *
 * @return 0 on success, negative error code on failure
 */
int xoe_wire_recv_tls(void* ssl, xoe_packet_t* packet);

/**
 * @brief Calculate CRC32 checksum of data
 *
 * Uses zlib crc32() for reliability.
 *
 * @param data      Data to checksum
 * @param len       Length of data in bytes
 *
 * @return CRC32 checksum value
 */
uint32_t xoe_wire_checksum(const void* data, uint32_t len);

/**
 * @brief Free packet payload allocated by xoe_wire_recv()
 *
 * @param packet    Packet whose payload should be freed
 */
void xoe_wire_free_payload(xoe_packet_t* packet);

#endif /* WIRE_FORMAT_H */
