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
#include <pthread.h>

/* Forward declaration for pending request structure */
typedef struct usb_pending_request usb_pending_request_t;

/**
 * @brief Pending request structure for request/response matching
 *
 * Tracks outgoing USB requests waiting for responses from the server.
 * Used for bidirectional transfer coordination.
 *
 * Phase 5 Addition
 */
struct usb_pending_request {
    uint32_t seqnum;                    /* Sequence number */
    uint32_t device_id;                 /* Device identifier */
    uint8_t endpoint;                   /* Target endpoint */

    /* Response data */
    void* response_data;                /* Buffer for response data */
    uint32_t response_size;             /* Size of response buffer */
    uint32_t response_received;         /* Bytes received */
    int32_t status;                     /* Transfer status */

    /* Synchronization */
    pthread_mutex_t mutex;              /* Response mutex */
    pthread_cond_t cond;                /* Response condition variable */
    int completed;                      /* Response received flag */

    /* Timeout tracking */
    unsigned long timestamp_ms;         /* Request timestamp */
    unsigned int timeout_ms;            /* Timeout value */

    /* List linkage */
    usb_pending_request_t* next;        /* Next in queue */
};

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
    pthread_t network_thread;           /* Network receive thread */
    pthread_t* transfer_threads;        /* Per-device transfer threads */
    pthread_mutex_t lock;               /* Thread synchronization */
    pthread_cond_t shutdown_cond;       /* Shutdown condition */

    /* State flags */
    int running;                        /* Client is running */
    int shutdown_requested;             /* Shutdown requested */

    /* Phase 5: Request/response tracking */
    uint32_t next_seqnum;               /* Next sequence number */
    usb_pending_request_t* pending_head; /* Pending requests queue head */
    pthread_mutex_t pending_lock;       /* Pending queue lock */

    /* Statistics */
    unsigned long packets_sent;         /* Packets sent to server */
    unsigned long packets_received;     /* Packets received from server */
    unsigned long transfer_errors;      /* Transfer error count */
    unsigned long pending_count;        /* Pending requests count */
    unsigned long timeouts;             /* Request timeout count */
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
void* usb_client_network_thread(void* arg);

/**
 * @brief Per-device transfer thread entry point
 *
 * Handles USB transfers for a single device, reading from USB
 * and sending to network.
 *
 * @param arg Transfer thread context (usb_transfer_thread_ctx_t*)
 * @return Thread exit code
 */
void* usb_client_transfer_thread(void* arg);

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

/* ========================================================================
 * Phase 5: Request/Response Tracking Functions
 * ======================================================================== */

/**
 * @brief Allocate sequence number for new request
 *
 * Thread-safe allocation of unique sequence numbers for request tracking.
 *
 * @param client Client context
 * @return Next sequence number
 */
uint32_t usb_client_alloc_seqnum(usb_client_t* client);

/**
 * @brief Create and enqueue pending request
 *
 * Allocates a pending request structure and adds it to the pending queue
 * for response matching.
 *
 * @param client Client context
 * @param seqnum Sequence number for this request
 * @param device_id Device identifier
 * @param endpoint Target endpoint
 * @param response_buffer Buffer to receive response data
 * @param response_size Size of response buffer
 * @param timeout_ms Timeout in milliseconds
 * @return Pointer to pending request, or NULL on failure
 */
usb_pending_request_t* usb_client_create_pending_request(
    usb_client_t* client,
    uint32_t seqnum,
    uint32_t device_id,
    uint8_t endpoint,
    void* response_buffer,
    uint32_t response_size,
    unsigned int timeout_ms
);

/**
 * @brief Wait for pending request completion
 *
 * Blocks until the request completes (response received) or times out.
 *
 * @param client Client context
 * @param request Pending request to wait for
 * @return 0 on success, E_TIMEOUT on timeout, negative error code on failure
 */
int usb_client_wait_pending_request(
    usb_client_t* client,
    usb_pending_request_t* request
);

/**
 * @brief Complete pending request with response data
 *
 * Called by network receive thread when a response arrives.
 * Signals waiting threads and stores response data.
 *
 * @param client Client context
 * @param seqnum Sequence number of completed request
 * @param data Response data
 * @param data_len Length of response data
 * @param status Transfer status
 * @return 0 on success, E_NOT_FOUND if request not found
 */
int usb_client_complete_pending_request(
    usb_client_t* client,
    uint32_t seqnum,
    const void* data,
    uint32_t data_len,
    int32_t status
);

/**
 * @brief Free pending request
 *
 * Removes request from pending queue and frees all resources.
 *
 * @param client Client context
 * @param request Pending request to free
 */
void usb_client_free_pending_request(
    usb_client_t* client,
    usb_pending_request_t* request
);

/**
 * @brief Clean up timed-out requests
 *
 * Scans pending queue and removes/signals any requests that have exceeded
 * their timeout value.
 *
 * @param client Client context
 * @return Number of requests timed out
 */
int usb_client_cleanup_timeouts(usb_client_t* client);

#endif /* USB_CLIENT_H */
