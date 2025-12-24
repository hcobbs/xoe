/*
 * wire_format.c - XOE Wire Protocol Implementation
 *
 * Implements the network wire format for XOE packets with explicit
 * serialization to ensure cross-platform compatibility.
 *
 * SECURITY FIX: Addresses LIB-001/NET-006 - eliminates pointer transmission
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-24
 */

#include "wire_format.h"
#include "lib/common/definitions.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <zlib.h>

#if TLS_ENABLED
#include <openssl/ssl.h>
#endif

/*
 * Byte order conversion helpers
 */

void xoe_wire_write_uint16(uint8_t* buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8);
    buffer[1] = (uint8_t)(value);
}

void xoe_wire_write_uint32(uint8_t* buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)(value);
}

uint16_t xoe_wire_read_uint16(const uint8_t* buffer)
{
    return ((uint16_t)buffer[0] << 8) | buffer[1];
}

uint32_t xoe_wire_read_uint32(const uint8_t* buffer)
{
    return ((uint32_t)buffer[0] << 24) |
           ((uint32_t)buffer[1] << 16) |
           ((uint32_t)buffer[2] << 8) |
           buffer[3];
}

/*
 * Header serialization
 */

void xoe_wire_serialize_header(uint8_t* buffer, const xoe_wire_header_t* header)
{
    xoe_wire_write_uint16(buffer + 0, header->protocol_id);
    xoe_wire_write_uint16(buffer + 2, header->protocol_version);
    xoe_wire_write_uint32(buffer + 4, header->payload_length);
    xoe_wire_write_uint32(buffer + 8, header->checksum);
}

void xoe_wire_deserialize_header(xoe_wire_header_t* header, const uint8_t* buffer)
{
    header->protocol_id = xoe_wire_read_uint16(buffer + 0);
    header->protocol_version = xoe_wire_read_uint16(buffer + 2);
    header->payload_length = xoe_wire_read_uint32(buffer + 4);
    header->checksum = xoe_wire_read_uint32(buffer + 8);
}

/*
 * Checksum calculation using zlib CRC32
 */

uint32_t xoe_wire_checksum(const void* data, uint32_t len)
{
    if (data == NULL || len == 0) {
        return 0;
    }
    return (uint32_t)crc32(0L, (const Bytef*)data, len);
}

/**
 * @brief Calculate checksum over header fields and payload
 *
 * Checksum covers: protocol_id, protocol_version, payload_length, payload_data
 * (checksum field itself is NOT included)
 */
static uint32_t calculate_packet_checksum(const xoe_wire_header_t* header,
                                          const void* payload_data)
{
    uint32_t crc;
    uint8_t header_bytes[8];  /* First 8 bytes of header (excluding checksum) */

    /* Serialize header fields for checksum calculation */
    xoe_wire_write_uint16(header_bytes + 0, header->protocol_id);
    xoe_wire_write_uint16(header_bytes + 2, header->protocol_version);
    xoe_wire_write_uint32(header_bytes + 4, header->payload_length);

    /* Start with header fields */
    crc = (uint32_t)crc32(0L, header_bytes, 8);

    /* Include payload if present */
    if (payload_data != NULL && header->payload_length > 0) {
        crc = (uint32_t)crc32(crc, (const Bytef*)payload_data,
                              header->payload_length);
    }

    return crc;
}

/*
 * Helper to receive exactly n bytes (handles partial reads)
 */

static int recv_exact(int fd, void* buffer, size_t len)
{
    size_t total = 0;
    ssize_t received;
    uint8_t* buf = (uint8_t*)buffer;

    while (total < len) {
        received = recv(fd, buf + total, len - total, 0);
        if (received <= 0) {
            return E_IO_ERROR;
        }
        total += (size_t)received;
    }
    return 0;
}

static int send_exact(int fd, const void* buffer, size_t len)
{
    size_t total = 0;
    ssize_t sent;
    const uint8_t* buf = (const uint8_t*)buffer;

    while (total < len) {
        sent = send(fd, buf + total, len - total, 0);
        if (sent <= 0) {
            return E_IO_ERROR;
        }
        total += (size_t)sent;
    }
    return 0;
}

#if TLS_ENABLED
static int tls_recv_exact(SSL* ssl, void* buffer, size_t len)
{
    size_t total = 0;
    int received;
    uint8_t* buf = (uint8_t*)buffer;

    while (total < len) {
        received = SSL_read(ssl, buf + total, (int)(len - total));
        if (received <= 0) {
            return E_IO_ERROR;
        }
        total += (size_t)received;
    }
    return 0;
}

static int tls_send_exact(SSL* ssl, const void* buffer, size_t len)
{
    size_t total = 0;
    int sent;
    const uint8_t* buf = (const uint8_t*)buffer;

    while (total < len) {
        sent = SSL_write(ssl, buf + total, (int)(len - total));
        if (sent <= 0) {
            return E_IO_ERROR;
        }
        total += (size_t)sent;
    }
    return 0;
}
#endif

/*
 * Network I/O functions
 */

int xoe_wire_send(int fd, const xoe_packet_t* packet)
{
    xoe_wire_header_t header;
    uint8_t header_buffer[XOE_WIRE_HEADER_SIZE];
    int result;

    if (packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Build wire header from packet */
    header.protocol_id = packet->protocol_id;
    header.protocol_version = packet->protocol_version;

    if (packet->payload != NULL && packet->payload->data != NULL) {
        header.payload_length = packet->payload->len;
    } else {
        header.payload_length = 0;
    }

    /* Calculate checksum over header fields + payload */
    header.checksum = calculate_packet_checksum(&header,
        (packet->payload != NULL) ? packet->payload->data : NULL);

    /* Serialize header to buffer */
    xoe_wire_serialize_header(header_buffer, &header);

    /* Send header */
    result = send_exact(fd, header_buffer, XOE_WIRE_HEADER_SIZE);
    if (result != 0) {
        return result;
    }

    /* Send payload if present */
    if (header.payload_length > 0) {
        result = send_exact(fd, packet->payload->data, header.payload_length);
        if (result != 0) {
            return result;
        }
    }

    return 0;
}

int xoe_wire_recv(int fd, xoe_packet_t* packet)
{
    xoe_wire_header_t header;
    uint8_t header_buffer[XOE_WIRE_HEADER_SIZE];
    uint32_t calculated_checksum;
    int result;

    if (packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Initialize packet */
    memset(packet, 0, sizeof(xoe_packet_t));

    /* Receive header */
    result = recv_exact(fd, header_buffer, XOE_WIRE_HEADER_SIZE);
    if (result != 0) {
        return result;
    }

    /* Deserialize header */
    xoe_wire_deserialize_header(&header, header_buffer);

    /* Validate payload length */
    if (header.payload_length > XOE_WIRE_MAX_PAYLOAD) {
        return E_PROTOCOL_ERROR;
    }

    /* Copy header fields to packet */
    packet->protocol_id = header.protocol_id;
    packet->protocol_version = header.protocol_version;
    packet->checksum = header.checksum;

    /* Allocate and receive payload if present */
    if (header.payload_length > 0) {
        packet->payload = (xoe_payload_t*)malloc(sizeof(xoe_payload_t));
        if (packet->payload == NULL) {
            return E_OUT_OF_MEMORY;
        }

        packet->payload->data = malloc(header.payload_length);
        if (packet->payload->data == NULL) {
            free(packet->payload);
            packet->payload = NULL;
            return E_OUT_OF_MEMORY;
        }

        packet->payload->len = header.payload_length;
        packet->payload->owns_data = TRUE;

        result = recv_exact(fd, packet->payload->data, header.payload_length);
        if (result != 0) {
            xoe_wire_free_payload(packet);
            return result;
        }
    }

    /* Validate checksum */
    calculated_checksum = calculate_packet_checksum(&header,
        (packet->payload != NULL) ? packet->payload->data : NULL);

    if (calculated_checksum != header.checksum) {
        xoe_wire_free_payload(packet);
        return E_CHECKSUM_MISMATCH;
    }

    return 0;
}

#if TLS_ENABLED
int xoe_wire_send_tls(void* ssl_ptr, const xoe_packet_t* packet)
{
    SSL* ssl = (SSL*)ssl_ptr;
    xoe_wire_header_t header;
    uint8_t header_buffer[XOE_WIRE_HEADER_SIZE];
    int result;

    if (packet == NULL || ssl == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Build wire header from packet */
    header.protocol_id = packet->protocol_id;
    header.protocol_version = packet->protocol_version;

    if (packet->payload != NULL && packet->payload->data != NULL) {
        header.payload_length = packet->payload->len;
    } else {
        header.payload_length = 0;
    }

    /* Calculate checksum */
    header.checksum = calculate_packet_checksum(&header,
        (packet->payload != NULL) ? packet->payload->data : NULL);

    /* Serialize and send header */
    xoe_wire_serialize_header(header_buffer, &header);
    result = tls_send_exact(ssl, header_buffer, XOE_WIRE_HEADER_SIZE);
    if (result != 0) {
        return result;
    }

    /* Send payload if present */
    if (header.payload_length > 0) {
        result = tls_send_exact(ssl, packet->payload->data, header.payload_length);
        if (result != 0) {
            return result;
        }
    }

    return 0;
}

int xoe_wire_recv_tls(void* ssl_ptr, xoe_packet_t* packet)
{
    SSL* ssl = (SSL*)ssl_ptr;
    xoe_wire_header_t header;
    uint8_t header_buffer[XOE_WIRE_HEADER_SIZE];
    uint32_t calculated_checksum;
    int result;

    if (packet == NULL || ssl == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Initialize packet */
    memset(packet, 0, sizeof(xoe_packet_t));

    /* Receive header */
    result = tls_recv_exact(ssl, header_buffer, XOE_WIRE_HEADER_SIZE);
    if (result != 0) {
        return result;
    }

    /* Deserialize header */
    xoe_wire_deserialize_header(&header, header_buffer);

    /* Validate payload length */
    if (header.payload_length > XOE_WIRE_MAX_PAYLOAD) {
        return E_PROTOCOL_ERROR;
    }

    /* Copy header fields to packet */
    packet->protocol_id = header.protocol_id;
    packet->protocol_version = header.protocol_version;
    packet->checksum = header.checksum;

    /* Allocate and receive payload if present */
    if (header.payload_length > 0) {
        packet->payload = (xoe_payload_t*)malloc(sizeof(xoe_payload_t));
        if (packet->payload == NULL) {
            return E_OUT_OF_MEMORY;
        }

        packet->payload->data = malloc(header.payload_length);
        if (packet->payload->data == NULL) {
            free(packet->payload);
            packet->payload = NULL;
            return E_OUT_OF_MEMORY;
        }

        packet->payload->len = header.payload_length;
        packet->payload->owns_data = TRUE;

        result = tls_recv_exact(ssl, packet->payload->data, header.payload_length);
        if (result != 0) {
            xoe_wire_free_payload(packet);
            return result;
        }
    }

    /* Validate checksum */
    calculated_checksum = calculate_packet_checksum(&header,
        (packet->payload != NULL) ? packet->payload->data : NULL);

    if (calculated_checksum != header.checksum) {
        xoe_wire_free_payload(packet);
        return E_CHECKSUM_MISMATCH;
    }

    return 0;
}
#else
/* Stub implementations when TLS is disabled */
int xoe_wire_send_tls(void* ssl_ptr, const xoe_packet_t* packet)
{
    (void)ssl_ptr;
    (void)packet;
    return E_NOT_SUPPORTED;
}

int xoe_wire_recv_tls(void* ssl_ptr, xoe_packet_t* packet)
{
    (void)ssl_ptr;
    (void)packet;
    return E_NOT_SUPPORTED;
}
#endif

void xoe_wire_free_payload(xoe_packet_t* packet)
{
    if (packet == NULL) {
        return;
    }

    if (packet->payload != NULL) {
        if (packet->payload->owns_data && packet->payload->data != NULL) {
            free(packet->payload->data);
        }
        free(packet->payload);
        packet->payload = NULL;
    }
}
