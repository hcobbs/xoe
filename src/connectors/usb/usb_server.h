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
#include <pthread.h>

/* Maximum number of USB clients that can connect */
#define USB_MAX_CLIENTS 16

/**
 * @brief USB client registration entry
 *
 * Tracks connected USB clients and their advertised USB devices.
 * Used for routing URBs to the appropriate client.
 */
typedef struct {
    int socket_fd;                      /* Client socket */
    uint32_t device_id;                 /* USB device ID (VID:PID) */
    int in_use;                         /* Slot in use flag */
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

    /* Statistics */
    unsigned long packets_routed;       /* Total packets routed */
    unsigned long routing_errors;       /* Routing error count */
    unsigned long active_clients;       /* Number of active clients */
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

#endif /* USB_SERVER_H */
