/**
 * @file serial_protocol.c
 * @brief Serial protocol encapsulation/decapsulation implementation
 *
 * Implements functions for converting serial data to/from XOE packets,
 * including checksum calculation and validation.
 *
 * [LLM-ASSISTED]
 */

#include "../h/serial_protocol.h"
#include "../../../common/h/commonDefinitions.h"

#include <stdlib.h>
#include <string.h>

/* Internal helper functions */
static uint32_t calculate_simple_checksum(const void* data, uint32_t len);

/**
 * @brief Encapsulate serial data into XOE packet
 */
int serial_protocol_encapsulate(const void* data, uint32_t len,
                                 uint16_t sequence, uint16_t flags,
                                 xoe_packet_t* packet)
{
    xoe_payload_t* payload;
    serial_header_t* header;
    unsigned char* payload_data;
    uint32_t total_payload_size;

    /* Validate parameters */
    if (data == NULL || packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (len > SERIAL_MAX_PAYLOAD_SIZE) {
        return E_BUFFER_TOO_SMALL;
    }

    /* Calculate total payload size (header + data) */
    total_payload_size = SERIAL_HEADER_SIZE + len;

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

    /* Build serial header */
    header = (serial_header_t*)payload_data;
    header->flags = flags;
    header->sequence = sequence;

    /* Copy serial data after header */
    if (len > 0) {
        memcpy(payload_data + SERIAL_HEADER_SIZE, data, len);
    }

    /* Set payload fields */
    payload->data = payload_data;
    payload->len = total_payload_size;
    payload->owns_data = TRUE;  /* We malloc'd this data */

    /* Set packet fields */
    packet->protocol_id = XOE_PROTOCOL_SERIAL;
    packet->protocol_version = XOE_PROTOCOL_SERIAL_VERSION;
    packet->payload = payload;
    packet->checksum = serial_protocol_checksum(packet);

    return 0;
}

/**
 * @brief Decapsulate XOE packet to serial data
 */
int serial_protocol_decapsulate(const xoe_packet_t* packet,
                                 void* data, uint32_t max_len,
                                 uint32_t* actual_len,
                                 uint16_t* sequence, uint16_t* flags)
{
    const serial_header_t* header;
    const unsigned char* payload_data;
    uint32_t data_len;
    int result;

    /* Validate parameters */
    if (packet == NULL || data == NULL || actual_len == NULL ||
        sequence == NULL || flags == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (packet->payload == NULL || packet->payload->data == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate protocol ID */
    if (packet->protocol_id != XOE_PROTOCOL_SERIAL) {
        return E_INVALID_STATE;
    }

    /* Validate checksum */
    result = serial_protocol_validate_checksum(packet);
    if (result != 0) {
        return result;
    }

    /* Validate payload size */
    if (packet->payload->len < SERIAL_HEADER_SIZE) {
        return E_INVALID_STATE;
    }

    /* Extract header */
    payload_data = (const unsigned char*)packet->payload->data;
    header = (const serial_header_t*)payload_data;

    /* Calculate actual data length */
    data_len = packet->payload->len - SERIAL_HEADER_SIZE;

    /* Check if output buffer is large enough */
    if (data_len > max_len) {
        return E_BUFFER_TOO_SMALL;
    }

    /* Extract sequence and flags */
    *sequence = header->sequence;
    *flags = header->flags;
    *actual_len = data_len;

    /* Copy data to output buffer */
    if (data_len > 0) {
        memcpy(data, payload_data + SERIAL_HEADER_SIZE, data_len);
    }

    return 0;
}

/**
 * @brief Calculate checksum for serial packet
 */
uint32_t serial_protocol_checksum(const xoe_packet_t* packet)
{
    uint32_t checksum = 0;

    if (packet == NULL || packet->payload == NULL ||
        packet->payload->data == NULL) {
        return 0;
    }

    /* Simple checksum: sum of all bytes in payload */
    checksum = calculate_simple_checksum(packet->payload->data,
                                         packet->payload->len);

    /* Include protocol ID and version in checksum */
    checksum += packet->protocol_id;
    checksum += packet->protocol_version;

    return checksum;
}

/**
 * @brief Validate serial packet checksum
 */
int serial_protocol_validate_checksum(const xoe_packet_t* packet)
{
    uint32_t calculated_checksum;

    if (packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    calculated_checksum = serial_protocol_checksum(packet);

    if (calculated_checksum != packet->checksum) {
        return E_INVALID_STATE;
    }

    return 0;
}

/**
 * @brief Free resources allocated for serial packet
 *
 * This function respects the owns_data flag to prevent double-free errors.
 * If owns_data is TRUE, the data buffer was malloc'd and will be freed.
 * If owns_data is FALSE, the data buffer is managed externally (e.g., stack
 * variable) and must not be freed.
 */
void serial_protocol_free_payload(xoe_packet_t* packet)
{
    if (packet == NULL) {
        return;
    }

    if (packet->payload != NULL) {
        /* Only free data buffer if this payload owns it */
        if (packet->payload->owns_data && packet->payload->data != NULL) {
            free(packet->payload->data);
            packet->payload->data = NULL;
        }
        free(packet->payload);
        packet->payload = NULL;
    }
}

/**
 * @brief Calculate simple checksum over data buffer
 *
 * Computes a simple sum of all bytes in the buffer. This provides
 * basic error detection. Can be upgraded to CRC32 if needed.
 *
 * @param data Pointer to data buffer
 * @param len Length of data in bytes
 * @return Checksum value
 */
static uint32_t calculate_simple_checksum(const void* data, uint32_t len)
{
    const unsigned char* bytes = (const unsigned char*)data;
    uint32_t checksum = 0;
    uint32_t i;

    if (data == NULL || len == 0) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        checksum += bytes[i];
    }

    return checksum;
}
