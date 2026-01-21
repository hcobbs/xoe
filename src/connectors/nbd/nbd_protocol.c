/**
 * @file nbd_protocol.c
 * @brief NBD protocol encapsulation/decapsulation implementation
 *
 * Implements functions for converting NBD commands/replies to/from XOE packets.
 * Follows serial connector encapsulation pattern for consistency.
 *
 * [LLM-ASSISTED]
 */

#include "connectors/nbd/nbd_protocol.h"
#include "lib/common/definitions.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <arpa/inet.h>  /* htonl, ntohl for endianness conversion */

/**
 * nbd_protocol_encapsulate - Encapsulate NBD command into XOE packet
 */
int nbd_protocol_encapsulate(uint8_t command, uint8_t flags,
                              uint64_t cookie, uint64_t offset,
                              uint32_t length, uint32_t error,
                              const void* data, uint32_t data_len,
                              xoe_packet_t* packet)
{
    xoe_payload_t* payload;
    nbd_xoe_header_t* header;
    unsigned char* payload_data;
    uint32_t total_payload_size;

    /* Validate parameters */
    if (packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate data pointer if data_len > 0 */
    if (data_len > 0 && data == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate payload size */
    if (data_len > NBD_MAX_PAYLOAD_SIZE) {
        return E_BUFFER_TOO_SMALL;
    }

    /* Calculate total payload size (header + data) */
    total_payload_size = NBD_XOE_HEADER_SIZE + data_len;

    /* Allocate payload structure */
    payload = (xoe_payload_t*)malloc(sizeof(xoe_payload_t));
    if (payload == NULL) {
        return E_OUT_OF_MEMORY;
    }

    /* Allocate payload data buffer */
    payload_data = (unsigned char*)malloc(total_payload_size);
    if (payload_data == NULL) {
        free(payload);
        return E_OUT_OF_MEMORY;
    }

    /* Build NBD XOE header (network byte order for multi-byte fields) */
    header = (nbd_xoe_header_t*)payload_data;
    header->command = command;
    header->flags = flags;
    header->reserved = 0;

    /* Convert 64-bit fields to network byte order (use htonl twice for portability) */
    {
        uint32_t* cookie_parts = (uint32_t*)&header->cookie;
        uint32_t* offset_parts = (uint32_t*)&header->offset;
        const uint32_t* src_cookie = (const uint32_t*)&cookie;
        const uint32_t* src_offset = (const uint32_t*)&offset;

        cookie_parts[0] = htonl(src_cookie[0]);
        cookie_parts[1] = htonl(src_cookie[1]);
        offset_parts[0] = htonl(src_offset[0]);
        offset_parts[1] = htonl(src_offset[1]);
    }

    header->length = htonl(length);
    header->error = htonl(error);

    /* Copy data after header (if present) */
    if (data_len > 0) {
        memcpy(payload_data + NBD_XOE_HEADER_SIZE, data, data_len);
    }

    /* Set payload fields */
    payload->data = payload_data;
    payload->len = total_payload_size;
    payload->owns_data = TRUE;  /* We malloc'd this data */

    /* Set packet fields */
    packet->protocol_id = XOE_PROTOCOL_NBD;
    packet->protocol_version = XOE_PROTOCOL_NBD_VERSION;
    packet->payload = payload;
    packet->checksum = nbd_protocol_checksum(packet);

    return 0;
}

/**
 * nbd_protocol_decapsulate - Decapsulate XOE packet into NBD components
 */
int nbd_protocol_decapsulate(const xoe_packet_t* packet,
                              uint8_t* command, uint8_t* flags,
                              uint64_t* cookie, uint64_t* offset,
                              uint32_t* length, uint32_t* error,
                              void* data_buf, uint32_t data_buf_size,
                              uint32_t* actual_data_len)
{
    const nbd_xoe_header_t* header;
    const unsigned char* payload_data;
    uint32_t data_len;

    /* Validate parameters */
    if (packet == NULL || command == NULL || flags == NULL ||
        cookie == NULL || offset == NULL || length == NULL ||
        error == NULL || actual_data_len == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (packet->payload == NULL || packet->payload->data == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate protocol ID */
    if (packet->protocol_id != XOE_PROTOCOL_NBD) {
        return E_INVALID_STATE;
    }

    /* Validate payload size */
    if (packet->payload->len < NBD_XOE_HEADER_SIZE) {
        return E_INVALID_STATE;
    }

    /* Extract header */
    payload_data = (const unsigned char*)packet->payload->data;
    header = (const nbd_xoe_header_t*)payload_data;

    /* Calculate actual data length */
    data_len = packet->payload->len - NBD_XOE_HEADER_SIZE;

    /* Check if output buffer is large enough (if data present) */
    if (data_len > 0) {
        if (data_buf == NULL || data_len > data_buf_size) {
            return E_BUFFER_TOO_SMALL;
        }
    }

    /* Extract header fields */
    *command = header->command;
    *flags = header->flags;
    *actual_data_len = data_len;

    /* Convert multi-byte fields from network byte order */
    {
        uint32_t* dst_cookie = (uint32_t*)cookie;
        uint32_t* dst_offset = (uint32_t*)offset;
        const uint32_t* src_cookie = (const uint32_t*)&header->cookie;
        const uint32_t* src_offset = (const uint32_t*)&header->offset;

        dst_cookie[0] = ntohl(src_cookie[0]);
        dst_cookie[1] = ntohl(src_cookie[1]);
        dst_offset[0] = ntohl(src_offset[0]);
        dst_offset[1] = ntohl(src_offset[1]);
    }

    *length = ntohl(header->length);
    *error = ntohl(header->error);

    /* Copy data to output buffer (if present) */
    if (data_len > 0 && data_buf != NULL) {
        memcpy(data_buf, payload_data + NBD_XOE_HEADER_SIZE, data_len);
    }

    return 0;
}

/**
 * nbd_protocol_free_payload - Free payload allocated by encapsulate
 */
void nbd_protocol_free_payload(xoe_packet_t* packet) {
    if (packet == NULL || packet->payload == NULL) {
        return;
    }

    if (packet->payload->owns_data && packet->payload->data != NULL) {
        free(packet->payload->data);
        packet->payload->data = NULL;
    }

    free(packet->payload);
    packet->payload = NULL;
}

/**
 * nbd_protocol_checksum - Calculate checksum for NBD XOE packet
 */
uint32_t nbd_protocol_checksum(const xoe_packet_t* packet) {
    uint32_t checksum = 0;
    uint16_t protocol_id_net;
    uint16_t protocol_version_net;

    if (packet == NULL || packet->payload == NULL) {
        return 0;
    }

    /* Convert protocol fields to network byte order for consistent checksum */
    protocol_id_net = htons(packet->protocol_id);
    protocol_version_net = htons(packet->protocol_version);

    /* Initialize CRC32 */
    checksum = crc32(0L, Z_NULL, 0);

    /* CRC over protocol_id */
    checksum = crc32(checksum, (const unsigned char*)&protocol_id_net,
                     sizeof(protocol_id_net));

    /* CRC over protocol_version */
    checksum = crc32(checksum, (const unsigned char*)&protocol_version_net,
                     sizeof(protocol_version_net));

    /* CRC over payload data */
    if (packet->payload->data != NULL && packet->payload->len > 0) {
        checksum = crc32(checksum, (const unsigned char*)packet->payload->data,
                         packet->payload->len);
    }

    return checksum;
}
