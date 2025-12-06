/*
 * usb_protocol.h - XOE USB Protocol Definitions
 *
 * Defines the USB protocol for encapsulating USB Request Blocks (URBs)
 * over the XOE network protocol. Inspired by USB/IP but adapted for
 * the XOE architecture.
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include "lib/common/types.h"
#include "lib/protocol/protocol.h"

/* Protocol identifier and version */
#define XOE_PROTOCOL_USB_VERSION 0x0001

/* USB command types (inspired by USB/IP) */
#define USB_CMD_SUBMIT      0x0001  /* Submit URB */
#define USB_CMD_UNLINK      0x0002  /* Cancel URB */
#define USB_RET_SUBMIT      0x0003  /* URB completion */
#define USB_RET_UNLINK      0x0004  /* Unlink response */
#define USB_CMD_REGISTER    0x0005  /* Register device with server */
#define USB_RET_REGISTER    0x0006  /* Registration response */
#define USB_CMD_UNREGISTER  0x0007  /* Unregister device from server */
#define USB_RET_UNREGISTER  0x0008  /* Unregister response */
#define USB_CMD_ENUM        0x0010  /* Device enumeration */
#define USB_RET_ENUM        0x0011  /* Enumeration response */

/* USB transfer types (subset of libusb - isochronous not supported) */
#define USB_TRANSFER_CONTROL        0
#define USB_TRANSFER_BULK           2
#define USB_TRANSFER_INTERRUPT      3

/* USB transfer flags */
#define USB_FLAG_NO_DEVICE      0x0001  /* Device disconnected */
#define USB_FLAG_TIMEOUT        0x0002  /* Transfer timeout */
#define USB_FLAG_STALL          0x0004  /* Endpoint stalled */
#define USB_FLAG_OVERFLOW       0x0008  /* Buffer overflow */
#define USB_FLAG_BABBLE         0x0010  /* Babble detected */
#define USB_FLAG_CRC_ERROR      0x0020  /* CRC/protocol error */

/* Maximum payload sizes */
#define USB_MAX_PAYLOAD_SIZE    4096    /* URB header + data */
#define USB_MAX_DATA_SIZE       4048    /* Max transfer data */
#define USB_MAX_TRANSFER_SIZE   4048    /* Max bytes per transfer (alias) */

/**
 * @brief USB URB (USB Request Block) header structure
 *
 * This structure encapsulates USB transfer information for transmission
 * over the network. It contains all necessary metadata for routing,
 * executing, and reporting USB transfers.
 *
 * The structure is designed to be network-transmissible and includes
 * fields for device identification, endpoint addressing, transfer
 * control, and status reporting.
 */
typedef struct {
    uint16_t command;           /* Command type (USB_CMD_*) */
    uint16_t flags;             /* Protocol flags */
    uint32_t seqnum;            /* Sequence number */
    uint32_t device_id;         /* Device identifier (VID:PID) */
    uint8_t  endpoint;          /* Target endpoint */
    uint8_t  transfer_type;     /* Control/Bulk/Interrupt */
    uint16_t reserved;          /* Alignment padding */
    uint32_t transfer_length;   /* Expected data length */
    uint32_t actual_length;     /* Actual transferred (response) */
    int32_t  status;            /* Transfer status (libusb codes) */
    uint8_t  setup[8];          /* Control setup packet */
    /* Followed by transfer data */
} usb_urb_header_t;

/**
 * @brief Encapsulate USB URB into XOE packet
 *
 * Packs a USB URB header and transfer data into a XOE packet structure
 * for network transmission. Calculates checksum over the entire payload.
 *
 * @param urb_header Pointer to URB header to encapsulate (read-only)
 * @param transfer_data Pointer to transfer data buffer (read-only, may be NULL)
 * @param data_len Length of transfer data in bytes
 * @param packet Pointer to output XOE packet structure (caller-allocated)
 * @return 0 on success, negative error code on failure
 *
 * MEMORY OWNERSHIP:
 * ----------------
 * Inputs (not owned by function):
 * - urb_header: Read-only, not modified or freed
 * - transfer_data: Read-only, not modified or freed
 * - packet: Caller-allocated structure, not freed by function
 *
 * Outputs (owned by caller after successful return):
 * - packet->payload: Allocated by this function (caller must free)
 * - packet->payload->data: Allocated by this function (caller must free)
 * - packet->payload->owns_data: Set to TRUE
 *
 * The caller MUST call usb_protocol_free_payload(packet) after transmission
 * to free the allocated payload and data buffer. Failure to do so will
 * result in memory leaks.
 *
 * On failure (return < 0):
 * - No memory is allocated
 * - packet structure is not modified
 *
 * Example:
 *   xoe_packet_t packet;
 *   if (usb_protocol_encapsulate(&urb, data, len, &packet) == 0) {
 *       // ... transmit packet ...
 *       usb_protocol_free_payload(&packet);  // Required!
 *   }
 */
int usb_protocol_encapsulate(
    const usb_urb_header_t* urb_header,
    const void* transfer_data,
    uint32_t data_len,
    xoe_packet_t* packet
);

/**
 * @brief Decapsulate XOE packet into USB URB
 *
 * Unpacks a XOE packet into a USB URB header and transfer data.
 * Verifies checksum and validates packet structure.
 *
 * @param packet Pointer to input XOE packet (read-only)
 * @param urb_header Pointer to output URB header structure (caller-allocated)
 * @param transfer_data Pointer to output buffer for transfer data (caller-allocated)
 * @param data_len Pointer to variable receiving actual data length (output)
 * @return 0 on success, negative error code on failure
 *
 * MEMORY OWNERSHIP:
 * ----------------
 * Inputs (not owned by function):
 * - packet: Read-only, not modified or freed
 * - packet->payload: Not freed by this function (caller manages)
 * - packet->payload->data: Not freed by this function (caller manages)
 *
 * Outputs (owned by caller):
 * - urb_header: Caller-allocated structure, filled by this function
 * - transfer_data: Caller-allocated buffer, filled by this function
 *   Must be at least USB_MAX_DATA_SIZE (4048) bytes
 * - data_len: Set to actual bytes copied to transfer_data
 *
 * This function COPIES data from the packet into caller-provided buffers.
 * It does NOT allocate memory and does NOT free the input packet.
 *
 * The caller is responsible for:
 * 1. Allocating urb_header structure (can be stack or heap)
 * 2. Allocating transfer_data buffer (at least USB_MAX_DATA_SIZE bytes)
 * 3. Freeing the input packet (if needed) after decapsulation
 *
 * Example:
 *   usb_urb_header_t urb;
 *   uint8_t data[USB_MAX_DATA_SIZE];
 *   uint32_t len;
 *   if (usb_protocol_decapsulate(&packet, &urb, data, &len) == 0) {
 *       // Use urb and data...
 *   }
 *   // Caller must still free packet if it was dynamically allocated
 */
int usb_protocol_decapsulate(
    const xoe_packet_t* packet,
    usb_urb_header_t* urb_header,
    void* transfer_data,
    uint32_t* data_len
);

/**
 * @brief Calculate checksum over URB header and data
 *
 * Computes a simple sum checksum over the URB header and transfer data.
 * This provides basic integrity checking for network transmission.
 *
 * @param urb_header Pointer to URB header
 * @param data Pointer to transfer data (may be NULL)
 * @param data_len Length of transfer data in bytes
 * @return Calculated checksum value
 *
 * Note: This uses a simple sum algorithm. Can be upgraded to CRC32
 *       if stronger integrity checking is needed.
 */
uint32_t usb_protocol_checksum(
    const usb_urb_header_t* urb_header,
    const void* data,
    uint32_t data_len
);

/**
 * @brief Free resources allocated for USB packet
 *
 * Frees the payload data buffer allocated during encapsulation.
 * This function should be called after the packet has been transmitted
 * to prevent memory leaks.
 *
 * @param packet Packet whose payload should be freed
 *
 * Note: This function does not free the packet structure itself,
 *       only the payload and its data buffer if owned by the payload.
 *
 * Thread safety: This function is thread-safe and may be called
 *                concurrently from multiple threads on different packets.
 */
void usb_protocol_free_payload(xoe_packet_t* packet);

#endif /* USB_PROTOCOL_H */
