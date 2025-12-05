/*
 * usb_client.h - USB Client Interface
 *
 * Coordinates USB device communication over network connection.
 * Manages multi-threaded USB transfer handling and network I/O.
 *
 * EXPERIMENTAL FEATURE - Subject to change
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#ifndef USB_CLIENT_H
#define USB_CLIENT_H

#include "usb_config.h"
#include "usb_device.h"
#include "usb_transfer.h"
#include "lib/protocol/protocol.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/**
 * @brief USB client context structure
 *
 * Maintains state for USB client operation including network connection,
 * device management, and thread coordination.
 */
typedef struct {
    /* Network connection */
    int socket_fd;                      /* Network socket to server */
    char* server_ip;                    /* Server IP address */
    int server_port;                    /* Server port */

    /* USB devices */
    usb_device_t* devices;              /* Array of USB devices */
    int device_count;                   /* Number of active devices */
    int max_devices;                    /* Maximum devices supported */

    /* Thread management */
#ifdef _WIN32
    HANDLE network_thread;              /* Network receive thread */
    HANDLE* transfer_threads;           /* Per-device transfer threads */
    CRITICAL_SECTION lock;              /* Thread synchronization */
    HANDLE shutdown_event;              /* Shutdown signal */
#else
    pthread_t network_thread;           /* Network receive thread */
    pthread_t* transfer_threads;        /* Per-device transfer threads */
    pthread_mutex_t lock;               /* Thread synchronization */
    pthread_cond_t shutdown_cond;       /* Shutdown condition */
#endif

    /* State flags */
    int running;                        /* Client is running */
    int shutdown_requested;             /* Shutdown requested */

    /* Statistics */
    unsigned long packets_sent;         /* Packets sent to server */
    unsigned long packets_received;     /* Packets received from server */
    unsigned long transfer_errors;      /* Transfer error count */
} usb_client_t;

/**
 * @brief Thread context for per-device USB transfers
 *
 * Contains device-specific context passed to transfer threads.
 */
typedef struct {
    usb_client_t* client;               /* Parent client context */
    usb_device_t* device;               /* USB device for this thread */
    int device_index;                   /* Device index in array */
} usb_transfer_thread_ctx_t;

/* ========================================================================
 * Client Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Initialize USB client
 *
 * Creates and initializes a USB client context with network connection
 * and device management structures.
 *
 * @param server_ip Server IP address
 * @param server_port Server port number
 * @param max_devices Maximum number of USB devices to support
 * @return Initialized client context, or NULL on failure
 *
 * Note: The client must be started with usb_client_start() and cleaned
 *       up with usb_client_cleanup().
 */
usb_client_t* usb_client_init(const char* server_ip,
                               int server_port,
                               int max_devices);

/**
 * @brief Add USB device to client
 *
 * Opens and initializes a USB device, adding it to the client's
 * device list for management.
 *
 * @param client Client context
 * @param config Device configuration
 * @return 0 on success, negative error code on failure
 *
 * Note: Devices must be added before calling usb_client_start().
 */
int usb_client_add_device(usb_client_t* client,
                          const usb_config_t* config);

/**
 * @brief Start USB client operation
 *
 * Connects to server and spawns worker threads for USB transfer
 * handling and network communication.
 *
 * @param client Client context
 * @return 0 on success, negative error code on failure
 *
 * Thread Architecture:
 * - Network receive thread: Handles incoming packets from server
 * - Per-device transfer threads: Handle USB I/O for each device
 *
 * Note: This function returns immediately after starting threads.
 *       Use usb_client_wait() to block until shutdown.
 */
int usb_client_start(usb_client_t* client);

/**
 * @brief Wait for client shutdown
 *
 * Blocks until the client is shut down via usb_client_stop() or
 * an error occurs.
 *
 * @param client Client context
 * @return 0 on normal shutdown, negative error code on failure
 */
int usb_client_wait(usb_client_t* client);

/**
 * @brief Stop USB client operation
 *
 * Signals shutdown and waits for all worker threads to terminate.
 * Closes network connection but does not free client resources.
 *
 * @param client Client context
 * @return 0 on success, negative error code on failure
 *
 * Note: Call usb_client_cleanup() to free all resources after stopping.
 */
int usb_client_stop(usb_client_t* client);

/**
 * @brief Cleanup USB client
 *
 * Releases all resources including network connection, USB devices,
 * and thread synchronization primitives.
 *
 * @param client Client context to cleanup (may be NULL)
 *
 * Note: This function safely handles partially initialized clients
 *       and NULL pointers.
 */
void usb_client_cleanup(usb_client_t* client);

/* ========================================================================
 * Network Communication Functions
 * ======================================================================== */

/**
 * @brief Send URB to server
 *
 * Encapsulates URB header and data into XOE packet and sends to server.
 *
 * @param client Client context
 * @param urb_header URB header structure
 * @param data Transfer data (may be NULL)
 * @param data_len Length of transfer data
 * @return 0 on success, negative error code on failure
 */
int usb_client_send_urb(usb_client_t* client,
                        const usb_urb_header_t* urb_header,
                        const void* data,
                        uint32_t data_len);

/**
 * @brief Receive URB from server
 *
 * Receives and decapsulates XOE packet into URB header and data.
 *
 * @param client Client context
 * @param urb_header Pointer to receive URB header
 * @param data Buffer to receive transfer data
 * @param data_len Pointer to data buffer size (input)/actual length (output)
 * @return 0 on success, negative error code on failure
 */
int usb_client_receive_urb(usb_client_t* client,
                           usb_urb_header_t* urb_header,
                           void* data,
                           uint32_t* data_len);

/* ========================================================================
 * Thread Entry Points (Internal Use)
 * ======================================================================== */

/**
 * @brief Network receive thread entry point
 *
 * Continuously receives packets from server and dispatches them
 * to appropriate device handlers.
 *
 * @param arg Client context (usb_client_t*)
 * @return Thread exit code
 */
#ifdef _WIN32
DWORD WINAPI usb_client_network_thread(LPVOID arg);
#else
void* usb_client_network_thread(void* arg);
#endif

/**
 * @brief Per-device transfer thread entry point
 *
 * Handles USB transfers for a single device, reading from USB
 * and sending to network.
 *
 * @param arg Transfer thread context (usb_transfer_thread_ctx_t*)
 * @return Thread exit code
 */
#ifdef _WIN32
DWORD WINAPI usb_client_transfer_thread(LPVOID arg);
#else
void* usb_client_transfer_thread(void* arg);
#endif

/* ========================================================================
 * Statistics and Status Functions
 * ======================================================================== */

/**
 * @brief Print client statistics
 *
 * Displays packet counts, error counts, and device status.
 *
 * @param client Client context
 */
void usb_client_print_stats(const usb_client_t* client);

/**
 * @brief Check if client is running
 *
 * @param client Client context
 * @return TRUE if running, FALSE otherwise
 */
int usb_client_is_running(const usb_client_t* client);

#endif /* USB_CLIENT_H */
