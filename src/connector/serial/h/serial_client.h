/**
 * @file serial_client.h
 * @brief Multi-threaded serial client for XOE integration
 *
 * Provides high-level interface for bidirectional serial-to-network bridging.
 * Manages three threads:
 * - Main thread: coordination and shutdown
 * - Serial→Network thread: reads from serial, encapsulates, sends to network
 * - Network→Serial thread: receives from network, decapsulates, writes to serial
 *
 * Uses circular buffer for flow control on the network→serial path.
 *
 * [LLM-ASSISTED]
 */

#ifndef SERIAL_CLIENT_H
#define SERIAL_CLIENT_H

#include <pthread.h>
#include "serial_config.h"
#include "serial_buffer.h"

/**
 * @brief Serial client session structure
 *
 * Contains all state for a serial client session including file descriptors,
 * threads, synchronization primitives, and buffers.
 */
typedef struct {
    /* Configuration */
    serial_config_t config;
    int network_fd;
    int serial_fd;

    /* Threading */
    pthread_t serial_to_net_thread;
    pthread_t net_to_serial_thread;
    int threads_started;

    /* Flow control buffer (network → serial) */
    serial_buffer_t rx_buffer;

    /* Synchronization */
    pthread_mutex_t shutdown_mutex;
    int shutdown_flag;

    /* Sequence numbers */
    uint16_t tx_sequence;
    uint16_t rx_sequence;
} serial_client_t;

/**
 * @brief Initialize a serial client session
 *
 * Allocates and initializes all resources needed for the serial client.
 * Opens the serial port and creates the circular buffer. Does not start
 * the I/O threads; call serial_client_start() to begin operation.
 *
 * @param config Serial port configuration
 * @param network_fd Connected network socket
 * @return Pointer to initialized session, or NULL on failure
 */
serial_client_t* serial_client_init(const serial_config_t* config,
                                     int network_fd);

/**
 * @brief Start serial client I/O threads
 *
 * Spawns the serial→network and network→serial threads to begin
 * bidirectional data transfer. Returns immediately; threads run
 * concurrently until shutdown.
 *
 * @param client Pointer to client session
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT - NULL pointer
 *         E_UNKNOWN_ERROR - Thread creation failed
 */
int serial_client_start(serial_client_t* client);

/**
 * @brief Stop serial client and wait for threads
 *
 * Sets the shutdown flag, closes resources, and waits for both I/O
 * threads to terminate. Blocks until all threads have exited.
 *
 * @param client Pointer to client session
 * @return 0 on success, negative error code on failure
 */
int serial_client_stop(serial_client_t* client);

/**
 * @brief Free serial client resources
 *
 * Frees all memory and resources associated with the client session.
 * Must be called after serial_client_stop(). Sets the pointer to NULL.
 *
 * @param client Pointer to pointer to client session
 */
void serial_client_cleanup(serial_client_t** client);

/**
 * @brief Check if shutdown has been requested
 *
 * Thread-safe check of the shutdown flag. Used by I/O threads to
 * determine when to exit.
 *
 * @param client Pointer to client session
 * @return TRUE if shutdown requested, FALSE otherwise
 */
int serial_client_should_shutdown(serial_client_t* client);

/**
 * @brief Request shutdown
 *
 * Sets the shutdown flag in a thread-safe manner. Does not wait for
 * threads to exit; call serial_client_stop() for that.
 *
 * @param client Pointer to client session
 */
void serial_client_request_shutdown(serial_client_t* client);

#endif /* SERIAL_CLIENT_H */
