/**
 * @file serial_client.c
 * @brief Multi-threaded serial client implementation
 *
 * Implements bidirectional serial-to-network bridging using three threads:
 * - Main thread handles initialization and shutdown coordination
 * - Serial→Network thread reads from serial port, encapsulates, sends
 * - Network→Serial thread receives from network, decapsulates, writes
 *
 * [LLM-ASSISTED]
 */

#include "connectors/serial/serial_client.h"
#include "connectors/serial/serial_port.h"
#include "connectors/serial/serial_protocol.h"
#include "connectors/serial/serial_config.h"
#include "lib/common/definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

/* C89-compliant logging - use inline fprintf to avoid variadic macro issues */
#define LOG_ERROR3(fmt, a1, a2, a3) do { \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, a1, a2, a3); \
} while (0)

#define LOG_ERROR2(fmt, a1, a2) do { \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, a1, a2); \
} while (0)

#define LOG_ERROR1(fmt, a1) do { \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, a1); \
} while (0)

#define LOG_WARN2(fmt, a1, a2) do { \
    fprintf(stderr, "[WARN] %s:%d: " fmt "\n", __FILE__, __LINE__, a1, a2); \
} while (0)

#define LOG_WARN1(fmt, a1) do { \
    fprintf(stderr, "[WARN] %s:%d: " fmt "\n", __FILE__, __LINE__, a1); \
} while (0)

#define LOG_INFO1(fmt, a1) do { \
    fprintf(stdout, "[INFO] " fmt "\n", a1); \
} while (0)

#define LOG_ERROR_SIMPLE(msg) do { \
    fprintf(stderr, "[ERROR] %s:%d: %s\n", __FILE__, __LINE__, msg); \
} while (0)

#define LOG_INFO_SIMPLE(msg) do { \
    fprintf(stdout, "[INFO] %s\n", msg); \
} while (0)

/* Internal thread entry points */
static void* serial_to_net_thread_func(void* arg);
static void* net_to_serial_thread_func(void* arg);

/**
 * @brief Initialize a serial client session
 */
serial_client_t* serial_client_init(const serial_config_t* config,
                                     int network_fd)
{
    serial_client_t* client;
    int result;

    if (config == NULL || network_fd < 0) {
        return NULL;
    }

    /* Allocate client structure */
    client = (serial_client_t*)malloc(sizeof(serial_client_t));
    if (client == NULL) {
        return NULL;
    }

    memset(client, 0, sizeof(serial_client_t));

    /* Copy configuration */
    memcpy(&client->config, config, sizeof(serial_config_t));
    client->network_fd = network_fd;

    /* Initialize mutex */
    result = pthread_mutex_init(&client->shutdown_mutex, NULL);
    if (result != 0) {
        free(client);
        return NULL;
    }

    /* Open serial port */
    result = serial_port_open(&client->config, &client->serial_fd);
    if (result != 0) {
        pthread_mutex_destroy(&client->shutdown_mutex);
        free(client);
        return NULL;
    }

    /* Initialize circular buffer for network → serial flow control */
    result = serial_buffer_init(&client->rx_buffer, 0);
    if (result != 0) {
        serial_port_close(client->serial_fd);
        pthread_mutex_destroy(&client->shutdown_mutex);
        free(client);
        return NULL;
    }

    return client;
}

/**
 * @brief Start serial client I/O threads
 */
int serial_client_start(serial_client_t* client)
{
    int result;

    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Start serial → network thread */
    result = pthread_create(&client->serial_to_net_thread, NULL,
                            serial_to_net_thread_func, client);
    if (result != 0) {
        return E_UNKNOWN_ERROR;
    }

    /* Start network → serial thread */
    result = pthread_create(&client->net_to_serial_thread, NULL,
                            net_to_serial_thread_func, client);
    if (result != 0) {
        /* Cancel the first thread */
        serial_client_request_shutdown(client);
        pthread_join(client->serial_to_net_thread, NULL);
        return E_UNKNOWN_ERROR;
    }

    client->threads_started = TRUE;
    return 0;
}

/**
 * @brief Stop serial client and wait for threads
 */
int serial_client_stop(serial_client_t* client)
{
    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Request shutdown */
    serial_client_request_shutdown(client);

    /* Close buffer to unblock net→serial thread */
    serial_buffer_close(&client->rx_buffer);

    /* Wait for threads if they were started */
    if (client->threads_started) {
        pthread_join(client->serial_to_net_thread, NULL);
        pthread_join(client->net_to_serial_thread, NULL);
        client->threads_started = FALSE;
    }

    return 0;
}

/**
 * @brief Free serial client resources
 */
void serial_client_cleanup(serial_client_t** client)
{
    if (client == NULL || *client == NULL) {
        return;
    }

    /* Close file descriptors */
    if ((*client)->serial_fd >= 0) {
        serial_port_close((*client)->serial_fd);
    }

    /* Destroy buffer */
    serial_buffer_destroy(&(*client)->rx_buffer);

    /* Destroy mutex */
    pthread_mutex_destroy(&(*client)->shutdown_mutex);

    /* Free structure */
    free(*client);
    *client = NULL;
}

/**
 * @brief Check if shutdown has been requested
 */
int serial_client_should_shutdown(serial_client_t* client)
{
    int should_shutdown;

    if (client == NULL) {
        return TRUE;
    }

    pthread_mutex_lock(&client->shutdown_mutex);
    should_shutdown = client->shutdown_flag;
    pthread_mutex_unlock(&client->shutdown_mutex);

    return should_shutdown;
}

/**
 * @brief Request shutdown
 */
void serial_client_request_shutdown(serial_client_t* client)
{
    if (client == NULL) {
        return;
    }

    pthread_mutex_lock(&client->shutdown_mutex);
    client->shutdown_flag = TRUE;
    pthread_mutex_unlock(&client->shutdown_mutex);
}

/**
 * @brief Serial → Network thread function
 *
 * Continuously reads from the serial port, encapsulates data into
 * XOE packets, and sends to the network socket. Exits on error or
 * shutdown request.
 */
static void* serial_to_net_thread_func(void* arg)
{
    serial_client_t* client;
    unsigned char buffer[SERIAL_READ_CHUNK_SIZE];
    int bytes_read;
    int bytes_sent;
    xoe_packet_t packet;
    int result;

    client = (serial_client_t*)arg;
    LOG_INFO_SIMPLE("Serial→Network thread started");

    while (!serial_client_should_shutdown(client)) {
        /* Read from serial port with timeout */
        bytes_read = serial_port_read(client->serial_fd, buffer,
                                       SERIAL_READ_CHUNK_SIZE,
                                       client->config.read_timeout_ms);

        if (bytes_read < 0) {
            /* Error reading from serial port */
            LOG_ERROR3("Serial read failed: error code %d (errno=%d: %s)",
                       bytes_read, errno, strerror(errno));
            LOG_ERROR2("Device: %s, Baud: %d",
                       client->config.device_path, client->config.baud_rate);
            serial_client_request_shutdown(client);
            break;
        }

        if (bytes_read == 0) {
            /* Timeout, try again */
            continue;
        }

        /* Encapsulate into XOE packet */
        result = serial_protocol_encapsulate(buffer, bytes_read,
                                              client->tx_sequence, 0,
                                              &packet);
        if (result != 0) {
            LOG_ERROR3("Packet encapsulation failed: error code %d, bytes=%d, seq=%u",
                       result, bytes_read, client->tx_sequence);
            serial_client_request_shutdown(client);
            break;
        }

        client->tx_sequence++;

        /* Send to network socket - store length before freeing payload */
        {
            int payload_len = (int)packet.payload->len;
            bytes_sent = write(client->network_fd, packet.payload->data,
                               payload_len);

            /* Free packet payload */
            serial_protocol_free_payload(&packet);

            if (bytes_sent < 0) {
                /* Network error */
                LOG_ERROR2("Network write failed: errno=%d: %s", errno, strerror(errno));
                serial_client_request_shutdown(client);
                break;
            }

            if (bytes_sent != payload_len) {
                LOG_WARN2("Partial network write: sent %d of %d bytes", bytes_sent, payload_len);
            }
        }
    }

    LOG_INFO_SIMPLE("Serial→Network thread exiting");
    return NULL;
}

/**
 * @brief Network → Serial thread function
 *
 * Continuously receives data from the network socket, decapsulates
 * XOE packets, and writes to the serial port via the circular buffer.
 * The buffer handles speed mismatch between network and serial.
 */
static void* net_to_serial_thread_func(void* arg)
{
    serial_client_t* client;
    unsigned char net_buffer[SERIAL_MAX_PAYLOAD_SIZE + SERIAL_HEADER_SIZE];
    unsigned char serial_buffer[SERIAL_MAX_PAYLOAD_SIZE];
    int bytes_received;
    int bytes_written;
    xoe_packet_t packet;
    uint32_t actual_len;
    uint16_t sequence;
    uint16_t flags;
    int result;

    client = (serial_client_t*)arg;
    memset(&packet, 0, sizeof(packet));
    LOG_INFO_SIMPLE("Network→Serial thread started");

    while (!serial_client_should_shutdown(client)) {
        /* Receive from network */
        bytes_received = read(client->network_fd, net_buffer,
                              sizeof(net_buffer));

        if (bytes_received < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted, try again */
            }
            /* Network error */
            LOG_ERROR2("Network read failed: errno=%d: %s", errno, strerror(errno));
            serial_client_request_shutdown(client);
            break;
        }

        if (bytes_received == 0) {
            /* Connection closed */
            LOG_INFO_SIMPLE("Network connection closed by peer");
            serial_client_request_shutdown(client);
            break;
        }

        /* Setup packet structure for decapsulation */
        packet.protocol_id = XOE_PROTOCOL_SERIAL;
        packet.protocol_version = XOE_PROTOCOL_SERIAL_VERSION;

        /* Allocate payload structure */
        packet.payload = (xoe_payload_t*)malloc(sizeof(xoe_payload_t));
        if (packet.payload == NULL) {
            LOG_ERROR_SIMPLE("Memory allocation failed for payload structure");
            serial_client_request_shutdown(client);
            break;
        }

        packet.payload->data = net_buffer;
        packet.payload->len = bytes_received;
        packet.payload->owns_data = FALSE;  /* Data is stack-allocated */

        /* Decapsulate */
        result = serial_protocol_decapsulate(&packet, serial_buffer,
                                              sizeof(serial_buffer),
                                              &actual_len, &sequence,
                                              &flags);

        /* Free only the payload structure, not the data (it's stack-allocated) */
        free(packet.payload);
        packet.payload = NULL;

        if (result != 0) {
            /* Decapsulation error, skip packet */
            LOG_WARN2("Packet decapsulation failed: error code %d, bytes=%d", result, bytes_received);
            continue;
        }

        /* Check for serial errors in flags */
        if (flags & SERIAL_FLAG_PARITY_ERROR) {
            LOG_WARN1("Parity error detected in packet seq=%u", sequence);
        }
        if (flags & SERIAL_FLAG_FRAMING_ERROR) {
            LOG_WARN1("Framing error detected in packet seq=%u", sequence);
        }
        if (flags & SERIAL_FLAG_OVERRUN_ERROR) {
            LOG_WARN1("Overrun error detected in packet seq=%u", sequence);
        }

        client->rx_sequence = sequence;

        /* Write to serial port via circular buffer */
        bytes_written = serial_buffer_write(&client->rx_buffer,
                                             serial_buffer, actual_len);

        if (bytes_written <= 0) {
            /* Buffer closed or error */
            LOG_ERROR1("Buffer write failed: returned %d", bytes_written);
            serial_client_request_shutdown(client);
            break;
        }

        if ((uint32_t)bytes_written != actual_len) {
            LOG_WARN2("Partial buffer write: wrote %d of %u bytes", bytes_written, actual_len);
        }

        /* Read from buffer and write to serial port */
        while (serial_buffer_available(&client->rx_buffer) > 0 &&
               !serial_client_should_shutdown(client)) {
            bytes_received = serial_buffer_read(&client->rx_buffer,
                                                 serial_buffer,
                                                 sizeof(serial_buffer));

            if (bytes_received <= 0) {
                break; /* Buffer empty or closed */
            }

            bytes_written = serial_port_write(client->serial_fd,
                                               serial_buffer,
                                               bytes_received);

            if (bytes_written < 0) {
                /* Serial write error */
                LOG_ERROR3("Serial write failed: error code %d (errno=%d: %s)", bytes_written, errno, strerror(errno));
                LOG_ERROR1("Device: %s", client->config.device_path);
                serial_client_request_shutdown(client);
                break;
            }

            if (bytes_written != bytes_received) {
                LOG_WARN2("Partial serial write: wrote %d of %d bytes", bytes_written, bytes_received);
            }
        }
    }

    LOG_INFO_SIMPLE("Network→Serial thread exiting");
    return NULL;
}
