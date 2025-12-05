/*
 * usb_protocol.c - XOE USB Protocol Implementation
 *
 * Implements USB Request Block (URB) encapsulation and decapsulation
 * for transmission over the XOE network protocol.
 *
 * Network byte order (big-endian) is used for all multi-byte fields
 * to ensure cross-platform compatibility.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_protocol.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>

/*
 * Byte order conversion helpers
 * Network protocols use big-endian (network byte order)
 */

static void write_uint16_be(uint8_t* buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8);
    buffer[1] = (uint8_t)(value);
}

static void write_uint32_be(uint8_t* buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)(value);
}

static void write_int32_be(uint8_t* buffer, int32_t value)
{
    write_uint32_be(buffer, (uint32_t)value);
}

static uint16_t read_uint16_be(const uint8_t* buffer)
{
    return ((uint16_t)buffer[0] << 8) | buffer[1];
}

static uint32_t read_uint32_be(const uint8_t* buffer)
{
    return ((uint32_t)buffer[0] << 24) |
           ((uint32_t)buffer[1] << 16) |
           ((uint32_t)buffer[2] << 8) |
           buffer[3];
}

static int32_t read_int32_be(const uint8_t* buffer)
{
    return (int32_t)read_uint32_be(buffer);
}

/**
 * @brief Serialize URB header to network byte order
 *
 * Converts all multi-byte fields to big-endian for network transmission.
 */
static void serialize_urb_header(uint8_t* buffer, const usb_urb_header_t* header)
{
    int i;

    write_uint16_be(buffer + 0, header->command);
    write_uint16_be(buffer + 2, header->flags);
    write_uint32_be(buffer + 4, header->seqnum);
    write_uint32_be(buffer + 8, header->device_id);
    buffer[12] = header->endpoint;
    buffer[13] = header->transfer_type;
    write_uint16_be(buffer + 14, header->reserved);
    write_uint32_be(buffer + 16, header->transfer_length);
    write_uint32_be(buffer + 20, header->actual_length);
    write_int32_be(buffer + 24, header->status);
    for (i = 0; i < 8; i++) {
        buffer[28 + i] = header->setup[i];
    }
}

/**
 * @brief Deserialize URB header from network byte order
 *
 * Converts all multi-byte fields from big-endian to host byte order.
 */
static void deserialize_urb_header(usb_urb_header_t* header, const uint8_t* buffer)
{
    int i;

    header->command = read_uint16_be(buffer + 0);
    header->flags = read_uint16_be(buffer + 2);
    header->seqnum = read_uint32_be(buffer + 4);
    header->device_id = read_uint32_be(buffer + 8);
    header->endpoint = buffer[12];
    header->transfer_type = buffer[13];
    header->reserved = read_uint16_be(buffer + 14);
    header->transfer_length = read_uint32_be(buffer + 16);
    header->actual_length = read_uint32_be(buffer + 20);
    header->status = read_int32_be(buffer + 24);
    for (i = 0; i < 8; i++) {
        header->setup[i] = buffer[28 + i];
    }
}

/**
 * @brief Calculate checksum over URB header and data
 *
 * Implementation note: Uses simple sum checksum for now. Can be upgraded
 * to CRC32 if stronger integrity checking is needed in production.
 */
uint32_t usb_protocol_checksum(
    const usb_urb_header_t* urb_header,
    const void* data,
    uint32_t data_len)
{
    uint32_t sum = 0;
    const uint8_t* ptr;
    uint32_t i;

    /* Validate input */
    if (urb_header == NULL) {
        return 0;
    }

    /* Checksum the URB header */
    ptr = (const uint8_t*)urb_header;
    for (i = 0; i < sizeof(usb_urb_header_t); i++) {
        sum += ptr[i];
    }

    /* Checksum the data if present */
    if (data != NULL && data_len > 0) {
        ptr = (const uint8_t*)data;
        for (i = 0; i < data_len; i++) {
            sum += ptr[i];
        }
    }

    return sum;
}

/**
 * @brief Encapsulate USB URB into XOE packet
 *
 * This function creates a XOE packet containing a USB URB header and
 * optional transfer data. The packet structure is:
 *   - protocol_id: XOE_PROTOCOL_USB
 *   - protocol_version: XOE_PROTOCOL_USB_VERSION
 *   - payload: URB header + transfer data
 *   - checksum: calculated over URB header and data
 */
int usb_protocol_encapsulate(
    const usb_urb_header_t* urb_header,
    const void* transfer_data,
    uint32_t data_len,
    xoe_packet_t* packet)
{
    uint8_t* payload_buffer;
    uint32_t total_size;
    xoe_payload_t* payload;

    /* Validate inputs */
    if (urb_header == NULL || packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Check data length */
    if (data_len > USB_MAX_DATA_SIZE) {
        return E_INVALID_ARGUMENT;
    }

    /* Calculate total payload size */
    total_size = sizeof(usb_urb_header_t) + data_len;
    if (total_size > USB_MAX_PAYLOAD_SIZE) {
        return E_INVALID_ARGUMENT;
    }

    /* Allocate payload buffer */
    payload_buffer = (uint8_t*)malloc(total_size);
    if (payload_buffer == NULL) {
        return E_OUT_OF_MEMORY;
    }

    /* Serialize URB header to network byte order */
    serialize_urb_header(payload_buffer, urb_header);

    /* Copy transfer data if present */
    if (transfer_data != NULL && data_len > 0) {
        memcpy(payload_buffer + sizeof(usb_urb_header_t),
               transfer_data,
               data_len);
    }

    /* Allocate payload structure */
    payload = (xoe_payload_t*)malloc(sizeof(xoe_payload_t));
    if (payload == NULL) {
        free(payload_buffer);
        return E_OUT_OF_MEMORY;
    }

    /* Initialize payload structure */
    payload->len = total_size;
    payload->data = payload_buffer;
    payload->owns_data = TRUE;

    /* Initialize packet structure */
    packet->protocol_id = XOE_PROTOCOL_USB;
    packet->protocol_version = XOE_PROTOCOL_USB_VERSION;
    packet->payload = payload;
    packet->checksum = usb_protocol_checksum(urb_header,
                                              transfer_data,
                                              data_len);

    return 0;
}

/**
 * @brief Decapsulate XOE packet into USB URB
 *
 * This function extracts a USB URB header and transfer data from
 * a XOE packet. It validates the protocol ID, version, and checksum.
 */
int usb_protocol_decapsulate(
    const xoe_packet_t* packet,
    usb_urb_header_t* urb_header,
    void* transfer_data,
    uint32_t* data_len)
{
    const uint8_t* payload_buffer;
    uint32_t calculated_checksum;
    uint32_t payload_data_len;

    /* Validate inputs */
    if (packet == NULL || urb_header == NULL || data_len == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate protocol ID */
    if (packet->protocol_id != XOE_PROTOCOL_USB) {
        return E_PROTOCOL_ERROR;
    }

    /* Validate protocol version */
    if (packet->protocol_version != XOE_PROTOCOL_USB_VERSION) {
        return E_PROTOCOL_ERROR;
    }

    /* Validate payload */
    if (packet->payload == NULL || packet->payload->data == NULL) {
        return E_PROTOCOL_ERROR;
    }

    /* Validate payload size */
    if (packet->payload->len < sizeof(usb_urb_header_t)) {
        return E_PROTOCOL_ERROR;
    }

    if (packet->payload->len > USB_MAX_PAYLOAD_SIZE) {
        return E_PROTOCOL_ERROR;
    }

    /* Extract payload buffer */
    payload_buffer = (const uint8_t*)packet->payload->data;

    /* Deserialize URB header from network byte order */
    deserialize_urb_header(urb_header, payload_buffer);

    /* Calculate data length */
    payload_data_len = packet->payload->len - sizeof(usb_urb_header_t);

    /* Verify checksum BEFORE copying data (use original packet data) */
    calculated_checksum = usb_protocol_checksum(
        urb_header,
        payload_data_len > 0 ? (payload_buffer + sizeof(usb_urb_header_t)) : NULL,
        payload_data_len
    );

    if (calculated_checksum != packet->checksum) {
        return E_CHECKSUM_MISMATCH;
    }

    /* Copy transfer data if present and buffer provided */
    if (payload_data_len > 0 && transfer_data != NULL) {
        /* Check if output buffer is large enough */
        if (*data_len < payload_data_len) {
            return E_BUFFER_TOO_SMALL;
        }

        memcpy(transfer_data,
               payload_buffer + sizeof(usb_urb_header_t),
               payload_data_len);
    }

    /* Set actual data length */
    *data_len = payload_data_len;

    return 0;
}

/**
 * @brief Free resources allocated for USB packet
 *
 * Cleans up memory allocated during encapsulation. This function
 * safely handles NULL pointers and respects the owns_data flag.
 */
void usb_protocol_free_payload(xoe_packet_t* packet)
{
    if (packet == NULL) {
        return;
    }

    if (packet->payload != NULL) {
        /* Free data buffer if owned by payload */
        if (packet->payload->owns_data && packet->payload->data != NULL) {
            free(packet->payload->data);
            packet->payload->data = NULL;
        }

        /* Free payload structure */
        free(packet->payload);
        packet->payload = NULL;
    }
}
