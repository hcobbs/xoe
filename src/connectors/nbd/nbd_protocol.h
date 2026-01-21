/**
 * @file nbd_protocol.h
 * @brief NBD protocol constants and packet structures
 *
 * Implements NBD (Network Block Device) protocol as defined in:
 * https://github.com/NetworkBlockDevice/nbd/blob/master/doc/proto.md
 *
 * Integrates with XOE's packet abstraction layer (xoe_packet_t) for
 * encapsulation and wire format serialization.
 *
 * [LLM-ASSISTED]
 */

#ifndef NBD_PROTOCOL_H
#define NBD_PROTOCOL_H

#include "lib/common/types.h"
#include "lib/protocol/protocol.h"

/* NBD Protocol Version */
#define XOE_PROTOCOL_NBD         0x0003
#define XOE_PROTOCOL_NBD_VERSION 0x0001

/* NBD Magic Values (per NBD specification) */
#define NBD_MAGIC_INIT          0x4e42444d41474943ULL  /* "NBDMAGIC" */
#define NBD_MAGIC_OPTS          0x49484156454F5054ULL  /* "IHAVEOPT" */
#define NBD_MAGIC_REPLY         0x3e889045565a9ULL
#define NBD_MAGIC_REQUEST       0x25609513
#define NBD_MAGIC_SIMPLE_REPLY  0x67446698

/* NBD Commands */
#define NBD_CMD_READ      0
#define NBD_CMD_WRITE     1
#define NBD_CMD_DISC      2  /* Disconnect */
#define NBD_CMD_FLUSH     3
#define NBD_CMD_TRIM      4

/* NBD Transmission Flags */
#define NBD_FLAG_HAS_FLAGS      (1 << 0)
#define NBD_FLAG_READ_ONLY      (1 << 1)
#define NBD_FLAG_SEND_FLUSH     (1 << 2)
#define NBD_FLAG_SEND_FUA       (1 << 3)  /* Force Unit Access */
#define NBD_FLAG_ROTATIONAL     (1 << 4)  /* Device is rotational */
#define NBD_FLAG_SEND_TRIM      (1 << 5)
#define NBD_FLAG_SEND_WRITE_ZEROES (1 << 6)
#define NBD_FLAG_CAN_MULTI_CONN (1 << 8)

/* NBD Request Flags */
#define NBD_CMD_FLAG_FUA  (1 << 0)  /* Force Unit Access */

/* NBD Error Codes */
#define NBD_OK          0
#define NBD_EPERM       1
#define NBD_EIO         5
#define NBD_ENOMEM      12
#define NBD_EINVAL      22
#define NBD_ENOSPC      28
#define NBD_EOVERFLOW   75
#define NBD_ESHUTDOWN   108

/* Protocol Limits */
#define NBD_MAX_PAYLOAD_SIZE    (4 * 1024 * 1024)  /* 4MB max transfer */
#define NBD_DEFAULT_BLOCK_SIZE  4096

/**
 * NBD XOE Payload Header
 *
 * This structure is prepended to payload data in xoe_packet_t.
 * It encapsulates NBD request/reply information for transmission
 * over XOE's wire format.
 */
typedef struct {
    uint8_t  command;     /* NBD_CMD_* */
    uint8_t  flags;       /* Command flags (NBD_CMD_FLAG_*) */
    uint16_t reserved;    /* Alignment padding */
    uint64_t cookie;      /* Request/response correlation handle */
    uint64_t offset;      /* Block offset in bytes */
    uint32_t length;      /* Data length in bytes */
    uint32_t error;       /* Error code (replies only, NBD_*) */
    /* Followed by data for READ reply or WRITE request */
} nbd_xoe_header_t;

#define NBD_XOE_HEADER_SIZE 28

/**
 * NBD Request (native NBD protocol format, 28 bytes)
 *
 * This is the on-wire format defined by the NBD specification.
 * Used during handshake and for reference; actual XOE transmission
 * uses nbd_xoe_header_t within xoe_packet_t.
 */
typedef struct {
    uint32_t magic;       /* NBD_MAGIC_REQUEST (0x25609513) */
    uint16_t flags;       /* Command flags */
    uint16_t type;        /* Command type (NBD_CMD_*) */
    uint64_t cookie;      /* Client-assigned handle */
    uint64_t offset;      /* Byte offset in export */
    uint32_t length;      /* Length of data */
} __attribute__((packed)) nbd_request_t;

/**
 * NBD Simple Reply (native NBD protocol format, 16 bytes)
 *
 * This is the on-wire format defined by the NBD specification.
 * For READ commands, reply is followed by 'length' bytes of data.
 */
typedef struct {
    uint32_t magic;       /* NBD_MAGIC_SIMPLE_REPLY (0x67446698) */
    uint32_t error;       /* 0 = success, else NBD error code */
    uint64_t cookie;      /* Echo of request cookie */
} __attribute__((packed)) nbd_simple_reply_t;

/**
 * nbd_protocol_encapsulate - Encapsulate NBD command into XOE packet
 * @command: NBD command (NBD_CMD_*)
 * @flags: Command flags (NBD_CMD_FLAG_*)
 * @cookie: Request correlation handle
 * @offset: Block offset in bytes
 * @length: Data length in bytes
 * @error: Error code (for replies, 0 for requests)
 * @data: Pointer to data buffer (for WRITE requests or READ replies)
 * @data_len: Length of data buffer
 * @packet: Output XOE packet structure
 *
 * Creates an XOE packet containing NBD protocol data. Allocates payload
 * memory (header + data) which must be freed via nbd_protocol_free_payload().
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_protocol_encapsulate(uint8_t command, uint8_t flags,
                              uint64_t cookie, uint64_t offset,
                              uint32_t length, uint32_t error,
                              const void* data, uint32_t data_len,
                              xoe_packet_t* packet);

/**
 * nbd_protocol_decapsulate - Decapsulate XOE packet into NBD components
 * @packet: Input XOE packet structure
 * @command: Output command (NBD_CMD_*)
 * @flags: Output command flags
 * @cookie: Output correlation handle
 * @offset: Output block offset
 * @length: Output data length
 * @error: Output error code
 * @data_buf: Output buffer for data (must be at least NBD_MAX_PAYLOAD_SIZE)
 * @data_buf_size: Size of data_buf
 * @actual_data_len: Actual data length extracted
 *
 * Extracts NBD protocol information from XOE packet payload.
 * Validates packet protocol ID and payload size.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_protocol_decapsulate(const xoe_packet_t* packet,
                              uint8_t* command, uint8_t* flags,
                              uint64_t* cookie, uint64_t* offset,
                              uint32_t* length, uint32_t* error,
                              void* data_buf, uint32_t data_buf_size,
                              uint32_t* actual_data_len);

/**
 * nbd_protocol_free_payload - Free payload allocated by encapsulate
 * @packet: XOE packet with allocated payload
 *
 * Frees payload memory if owns_data flag is set.
 * Safe to call multiple times; sets payload to NULL after free.
 */
void nbd_protocol_free_payload(xoe_packet_t* packet);

/**
 * nbd_protocol_checksum - Calculate checksum for NBD XOE packet
 * @packet: XOE packet to checksum
 *
 * Calculates CRC32 checksum over protocol_id, protocol_version, and payload.
 * Used for integrity verification over network transport.
 *
 * Returns: CRC32 checksum value
 */
uint32_t nbd_protocol_checksum(const xoe_packet_t* packet);

#endif /* NBD_PROTOCOL_H */
