/**
 * @file serial_protocol.h
 * @brief Serial protocol definitions and encapsulation interface
 *
 * Defines the serial protocol packet format and provides functions for
 * encapsulating serial data into XOE packets and decapsulating received
 * packets back to serial data.
 *
 * [LLM-ASSISTED]
 */

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include "../../../core/packet_manager/h/protocol.h"

/* Protocol ID for serial protocol */
#define XOE_PROTOCOL_SERIAL 0x0001
#define XOE_PROTOCOL_SERIAL_VERSION 0x0001

/* Maximum payload size for serial data (excluding header) */
#define SERIAL_MAX_PAYLOAD_SIZE 1020

/* Serial protocol header size (flags + sequence) */
#define SERIAL_HEADER_SIZE 4

/* Serial protocol flags */
#define SERIAL_FLAG_PARITY_ERROR  0x0001
#define SERIAL_FLAG_FRAMING_ERROR 0x0002
#define SERIAL_FLAG_OVERRUN_ERROR 0x0004
#define SERIAL_FLAG_XON           0x0010
#define SERIAL_FLAG_XOFF          0x0020

/**
 * @brief Serial protocol packet header
 *
 * This header is prepended to the actual serial data within the
 * xoe_payload_t structure.
 */
typedef struct {
    uint16_t flags;     /* Status and error flags */
    uint16_t sequence;  /* Sequence number for packet ordering */
} serial_header_t;

/**
 * @brief Encapsulate serial data into XOE packet
 *
 * Creates an XOE packet containing the serial data along with protocol
 * header and checksum. The caller is responsible for allocating the
 * packet structure and freeing the payload after use.
 *
 * @param data Pointer to serial data to encapsulate
 * @param len Length of serial data (max SERIAL_MAX_PAYLOAD_SIZE bytes)
 * @param sequence Sequence number for this packet
 * @param flags Status flags (parity error, framing error, etc.)
 * @param packet Output parameter for encapsulated packet
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT - NULL pointer or invalid parameters
 *         E_BUFFER_TOO_SMALL - Data length exceeds maximum
 *         E_OUT_OF_MEMORY - Memory allocation failed
 */
int serial_protocol_encapsulate(const void* data, uint32_t len,
                                 uint16_t sequence, uint16_t flags,
                                 xoe_packet_t* packet);

/**
 * @brief Decapsulate XOE packet to serial data
 *
 * Extracts serial data from an XOE packet, validates the protocol ID
 * and checksum, and copies the data to the output buffer.
 *
 * @param packet Input packet to decapsulate
 * @param data Output buffer for serial data
 * @param max_len Maximum size of output buffer
 * @param actual_len Output parameter for actual data length
 * @param sequence Output parameter for packet sequence number
 * @param flags Output parameter for status flags
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT - NULL pointer or invalid parameters
 *         E_BUFFER_TOO_SMALL - Output buffer too small
 *         E_INVALID_STATE - Invalid protocol ID or checksum mismatch
 */
int serial_protocol_decapsulate(const xoe_packet_t* packet,
                                 void* data, uint32_t max_len,
                                 uint32_t* actual_len,
                                 uint16_t* sequence, uint16_t* flags);

/**
 * @brief Calculate checksum for serial packet
 *
 * Computes a simple checksum over the packet contents. This is a
 * basic sum implementation; can be upgraded to CRC32 later if needed.
 *
 * @param packet Packet structure (must have valid payload)
 * @return Calculated checksum value
 */
uint32_t serial_protocol_checksum(const xoe_packet_t* packet);

/**
 * @brief Validate serial packet checksum
 *
 * Recalculates the checksum and compares it with the stored value.
 *
 * @param packet Packet to validate
 * @return 0 if checksum is valid, E_INVALID_STATE if mismatch
 */
int serial_protocol_validate_checksum(const xoe_packet_t* packet);

/**
 * @brief Free resources allocated for serial packet
 *
 * Frees the payload data buffer allocated during encapsulation.
 * Does not free the packet structure itself.
 *
 * @param packet Packet whose payload should be freed
 */
void serial_protocol_free_payload(xoe_packet_t* packet);

#endif /* SERIAL_PROTOCOL_H */
