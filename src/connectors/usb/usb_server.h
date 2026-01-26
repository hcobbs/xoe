/*
 * usb_server.h - USB Server Interface
 *
 * Server-side USB request routing and forwarding for USB over Ethernet.
 * Coordinates USB requests between multiple USB clients.
 *
 * EXPERIMENTAL FEATURE - Subject to change
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_SERVER_H
#define USB_SERVER_H

#include "usb_protocol.h"
#include "usb_config.h"
#include <pthread.h>

/* Maximum number of USB clients that can connect */
#define USB_MAX_CLIENTS 16

/* Authentication challenge size (must match usb_auth.h) */
#define USB_AUTH_CHALLENGE_SIZE 32

/**
 * @brief USB client registration entry
 *
 * Tracks connected USB clients and their advertised USB devices.
 * Used for routing URBs to the appropriate client.
 */
typedef struct {
    int socket_fd;                      /* Client socket */
    uint32_t device_id;                 /* USB device ID (VID:PID) */
    uint8_t device_class;               /* USB device class */
    int in_use;                         /* Slot in use flag */
    int authenticated;                  /* Authentication status */
    int auth_pending;                   /* Auth challenge sent, awaiting response */
    uint8_t pending_challenge[USB_AUTH_CHALLENGE_SIZE]; /* Challenge for auth */
    char client_ip[46];                 /* Client IP (IPv6-ready) */
    pthread_mutex_t send_lock;          /* Mutex for send operations */
} usb_client_entry_t;

/**
 * @brief USB server context structure
 *
 * Maintains state for USB server operation including client registry
 * and routing logic.
 */
typedef struct usb_server_t {
    /* Client registry */
    usb_client_entry_t clients[USB_MAX_CLIENTS];
    pthread_mutex_t registry_lock;      /* Registry modification lock */

    /* Security configuration */
    char auth_secret[USB_AUTH_SECRET_MAX];  /* Shared secret for auth */
    uint8_t allowed_classes[16];            /* Device class whitelist */
    int allowed_class_count;                /* Whitelist size */
    int require_auth;                       /* Authentication required flag */

    /* Statistics */
    unsigned long packets_routed;       /* Total packets routed */
    unsigned long routing_errors;       /* Routing error count */
    unsigned long active_clients;       /* Number of active clients */
    unsigned long auth_failures;        /* Authentication failures */
} usb_server_t;

/* ========================================================================
 * Server Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Initialize USB server
 *
 * Creates and initializes a USB server context with client registry.
 *
 * @return Initialized server context, or NULL on failure
 */
usb_server_t* usb_server_init(void);

/**
 * @brief Cleanup USB server
 *
 * Releases all resources including client registry.
 *
 * @param server Server context to cleanup (may be NULL)
 */
void usb_server_cleanup(usb_server_t* server);

/* ========================================================================
 * Client Management Functions
 * ======================================================================== */

/**
 * @brief Register USB client
 *
 * Registers a USB client connection and its advertised device.
 *
 * @param server Server context
 * @param socket_fd Client socket file descriptor
 * @param device_id USB device identifier (VID:PID)
 * @return 0 on success, negative error code on failure
 */
int usb_server_register_client(usb_server_t* server,
                                int socket_fd,
                                uint32_t device_id);

/**
 * @brief Unregister USB client
 *
 * Removes a USB client from the registry.
 *
 * @param server Server context
 * @param socket_fd Client socket file descriptor
 * @return 0 on success, negative error code on failure
 */
int usb_server_unregister_client(usb_server_t* server,
                                  int socket_fd);

/* ========================================================================
 * URB Routing Functions
 * ======================================================================== */

/**
 * @brief Route URB to target client
 *
 * Routes a USB Request Block to the appropriate client based on device_id.
 * This is the core routing function of the USB server.
 *
 * @param server Server context
 * @param urb_header URB header
 * @param data Transfer data (may be NULL)
 * @param data_len Length of transfer data
 * @param sender_fd Socket FD of sending client (to avoid loopback)
 * @return 0 on success, negative error code on failure
 */
int usb_server_route_urb(usb_server_t* server,
                         const usb_urb_header_t* urb_header,
                         const void* data,
                         uint32_t data_len,
                         int sender_fd);

/**
 * @brief Handle incoming URB from client
 *
 * Processes a URB received from a client. Decapsulates the packet,
 * performs routing logic, and forwards to the appropriate destination.
 *
 * @param server Server context
 * @param packet Received XOE packet
 * @param sender_fd Socket FD of sending client
 * @return 0 on success, negative error code on failure
 */
int usb_server_handle_urb(usb_server_t* server,
                          const xoe_packet_t* packet,
                          int sender_fd);

/* ========================================================================
 * Statistics and Status Functions
 * ======================================================================== */

/**
 * @brief Print server statistics
 *
 * Displays routing statistics and registered clients.
 *
 * @param server Server context
 */
void usb_server_print_stats(const usb_server_t* server);

/* ========================================================================
 * Authentication Configuration Functions
 * ======================================================================== */

/**
 * @brief Configure authentication secret
 *
 * Sets the shared secret used for challenge-response authentication.
 * If secret is NULL or empty, authentication is disabled.
 *
 * @param server Server context
 * @param secret Shared secret string (max USB_AUTH_SECRET_MAX chars)
 * @return 0 on success, negative error code on failure
 */
int usb_server_set_auth_secret(usb_server_t* server, const char* secret);

/**
 * @brief Configure device class whitelist
 *
 * Sets the allowed device classes. Only devices in the whitelist
 * will be accepted for registration. HID class is blocked by
 * default unless explicitly whitelisted.
 *
 * @param server Server context
 * @param classes Array of allowed USB class codes
 * @param count Number of entries in classes array
 * @return 0 on success, negative error code on failure
 */
int usb_server_set_class_whitelist(usb_server_t* server,
                                   const uint8_t* classes,
                                   int count);

/**
 * @brief Enable or disable authentication requirement
 *
 * @param server Server context
 * @param require 1 to require authentication, 0 to disable
 */
void usb_server_set_require_auth(usb_server_t* server, int require);

/**
 * @brief Get client IP address from socket
 *
 * Extracts the client IP address from a connected socket for logging.
 *
 * @param socket_fd Client socket file descriptor
 * @param ip_out Output buffer (at least 46 bytes for IPv6)
 * @param ip_out_len Size of output buffer
 */
void usb_server_get_client_ip(int socket_fd, char* ip_out, size_t ip_out_len);

#endif /* USB_SERVER_H */
