/*
 * usb_client.c - USB Client Implementation
 *
 * Implements USB client coordinator for managing USB devices
 * over network connection.
 *
 * EXPERIMENTAL FEATURE - Phase 4 Implementation
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_client.h"
#include "usb_protocol.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

/* ========================================================================
 * Client Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Initialize USB client
 */
usb_client_t* usb_client_init(const char* server_ip,
                               int server_port,
                               int max_devices)
{
    usb_client_t* client;

    /* Validate parameters */
    if (server_ip == NULL || server_port <= 0 || max_devices <= 0) {
        return NULL;
    }

    /* Allocate client structure */
    client = (usb_client_t*)malloc(sizeof(usb_client_t));
    if (client == NULL) {
        return NULL;
    }

    /* Initialize fields */
    memset(client, 0, sizeof(usb_client_t));

    /* Copy server information */
    client->server_ip = (char*)malloc(strlen(server_ip) + 1);
    if (client->server_ip == NULL) {
        free(client);
        return NULL;
    }
    strcpy(client->server_ip, server_ip);
    client->server_port = server_port;

    /* Allocate device array */
    client->devices = (usb_device_t*)malloc(sizeof(usb_device_t) * max_devices);
    if (client->devices == NULL) {
        free(client->server_ip);
        free(client);
        return NULL;
    }
    memset(client->devices, 0, sizeof(usb_device_t) * max_devices);

    /* Allocate transfer thread array */
    client->transfer_threads = (pthread_t*)malloc(sizeof(pthread_t) * max_devices);
    if (client->transfer_threads == NULL) {
        free(client->devices);
        free(client->server_ip);
        free(client);
        return NULL;
    }
    memset(client->transfer_threads, 0, sizeof(pthread_t) * max_devices);

    client->device_count = 0;
    client->max_devices = max_devices;
    client->socket_fd = -1;
    client->running = FALSE;
    client->shutdown_requested = FALSE;

    /* Initialize thread synchronization */
#ifdef _WIN32
    InitializeCriticalSection(&client->lock);
    client->shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (client->shutdown_event == NULL) {
        free(client->transfer_threads);
        free(client->devices);
        free(client->server_ip);
        DeleteCriticalSection(&client->lock);
        free(client);
        return NULL;
    }
#else
    pthread_mutex_init(&client->lock, NULL);
    pthread_cond_init(&client->shutdown_cond, NULL);
#endif

    /* Initialize statistics */
    client->packets_sent = 0;
    client->packets_received = 0;
    client->transfer_errors = 0;

    return client;
}

/**
 * @brief Add USB device to client
 */
int usb_client_add_device(usb_client_t* client,
                          const usb_config_t* config)
{
    int result;
    usb_device_t* device;

    /* Validate parameters */
    if (client == NULL || config == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Check if device array is full */
    if (client->device_count >= client->max_devices) {
        return E_BUFFER_TOO_SMALL;
    }

    /* Validate device configuration */
    result = usb_config_validate(config);
    if (result != 0) {
        return result;
    }

    /* Get device slot */
    device = &client->devices[client->device_count];

    /* Open USB device (using shared libusb context = NULL for now) */
    result = usb_device_open(device, NULL, config);
    if (result != 0) {
        fprintf(stderr, "Failed to open USB device %04x:%04x: error %d\n",
                config->vendor_id, config->product_id, result);
        return result;
    }

    /* Device successfully added */
    client->device_count++;

    printf("Added USB device %04x:%04x (device %d/%d)\n",
           config->vendor_id, config->product_id,
           client->device_count, client->max_devices);

    return 0;
}

/**
 * @brief Connect to server
 */
static int usb_client_connect(usb_client_t* client)
{
    struct sockaddr_in server_addr;
    int result;

    /* Create socket */
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return E_NETWORK_ERROR;
    }

    /* Setup server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)client->server_port);

    result = inet_pton(AF_INET, client->server_ip, &server_addr.sin_addr);
    if (result <= 0) {
        fprintf(stderr, "Invalid server IP address: %s\n", client->server_ip);
        close(client->socket_fd);
        client->socket_fd = -1;
        return E_INVALID_ARGUMENT;
    }

    /* Connect to server */
    result = connect(client->socket_fd,
                     (struct sockaddr*)&server_addr,
                     sizeof(server_addr));
    if (result < 0) {
        fprintf(stderr, "Failed to connect to %s:%d: %s\n",
                client->server_ip, client->server_port, strerror(errno));
        close(client->socket_fd);
        client->socket_fd = -1;
        return E_NETWORK_ERROR;
    }

    printf("Connected to server %s:%d\n", client->server_ip, client->server_port);
    return 0;
}

/**
 * @brief Start USB client operation
 */
int usb_client_start(usb_client_t* client)
{
    int result;

    /* Validate parameters */
    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Check if devices are configured */
    if (client->device_count == 0) {
        fprintf(stderr, "No USB devices configured\n");
        return E_INVALID_STATE;
    }

    /* Connect to server */
    result = usb_client_connect(client);
    if (result != 0) {
        return result;
    }

    /* Mark as running */
#ifdef _WIN32
    EnterCriticalSection(&client->lock);
#else
    pthread_mutex_lock(&client->lock);
#endif

    client->running = TRUE;
    client->shutdown_requested = FALSE;

#ifdef _WIN32
    LeaveCriticalSection(&client->lock);
#else
    pthread_mutex_unlock(&client->lock);
#endif

    /* Phase 4: Spawn worker threads for active USB transfers */
    printf("\nStarting USB client threads...\n");
    printf("  Devices: %d\n", client->device_count);
    printf("  Server: %s:%d\n", client->server_ip, client->server_port);
    printf("\n");

    /* Spawn network receive thread */
#ifdef _WIN32
    client->network_thread = CreateThread(
        NULL,
        0,
        usb_client_network_thread,
        client,
        0,
        NULL
    );
    if (client->network_thread == NULL) {
        fprintf(stderr, "Failed to create network thread\n");
        client->running = FALSE;
        close(client->socket_fd);
        client->socket_fd = -1;
        return E_IO_ERROR;
    }
#else
    result = pthread_create(&client->network_thread, NULL,
                           usb_client_network_thread, client);
    if (result != 0) {
        fprintf(stderr, "Failed to create network thread: %s\n",
                strerror(result));
        client->running = FALSE;
        close(client->socket_fd);
        client->socket_fd = -1;
        return E_IO_ERROR;
    }
#endif

    printf("Network receive thread spawned\n");

    /* Spawn per-device transfer threads */
    {
        int i;
        for (i = 0; i < client->device_count; i++) {
            usb_transfer_thread_ctx_t* ctx;

            /* Allocate thread context */
            ctx = (usb_transfer_thread_ctx_t*)malloc(
                sizeof(usb_transfer_thread_ctx_t));
            if (ctx == NULL) {
                fprintf(stderr, "Failed to allocate thread context for device %d\n",
                        i + 1);
                /* Continue with other devices */
                continue;
            }

            ctx->client = client;
            ctx->device = &client->devices[i];
            ctx->device_index = i;

            /* Spawn thread */
#ifdef _WIN32
            client->transfer_threads[i] = CreateThread(
                NULL,
                0,
                usb_client_transfer_thread,
                ctx,
                0,
                NULL
            );
            if (client->transfer_threads[i] == NULL) {
                fprintf(stderr, "Failed to create transfer thread for device %d\n",
                        i + 1);
                free(ctx);
                continue;
            }
#else
            result = pthread_create(&client->transfer_threads[i], NULL,
                                   usb_client_transfer_thread, ctx);
            if (result != 0) {
                fprintf(stderr, "Failed to create transfer thread for device %d: %s\n",
                        i + 1, strerror(result));
                free(ctx);
                continue;
            }
#endif

            printf("Transfer thread spawned for device %d (VID:PID %04x:%04x)\n",
                   i + 1, client->devices[i].config.vendor_id,
                   client->devices[i].config.product_id);
        }
    }

    printf("\nAll threads started. Press Ctrl+C to exit...\n\n");

    return 0;
}

/**
 * @brief Wait for client shutdown
 */
int usb_client_wait(usb_client_t* client)
{
    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Phase 3 Stub: Just wait for shutdown signal */
#ifdef _WIN32
    WaitForSingleObject(client->shutdown_event, INFINITE);
#else
    pthread_mutex_lock(&client->lock);
    while (!client->shutdown_requested) {
        pthread_cond_wait(&client->shutdown_cond, &client->lock);
    }
    pthread_mutex_unlock(&client->lock);
#endif

    return 0;
}

/**
 * @brief Stop USB client operation
 */
int usb_client_stop(usb_client_t* client)
{
    int i;

    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    printf("\nStopping USB client...\n");

    /* Signal shutdown */
#ifdef _WIN32
    EnterCriticalSection(&client->lock);
#else
    pthread_mutex_lock(&client->lock);
#endif

    client->shutdown_requested = TRUE;
    client->running = FALSE;

#ifdef _WIN32
    SetEvent(client->shutdown_event);
    LeaveCriticalSection(&client->lock);
#else
    pthread_cond_broadcast(&client->shutdown_cond);
    pthread_mutex_unlock(&client->lock);
#endif

    /* Phase 4: Wait for transfer threads to exit */
    printf("Waiting for transfer threads to exit...\n");
    for (i = 0; i < client->device_count; i++) {
#ifdef _WIN32
        if (client->transfer_threads[i] != NULL) {
            WaitForSingleObject(client->transfer_threads[i], INFINITE);
            CloseHandle(client->transfer_threads[i]);
            client->transfer_threads[i] = NULL;
        }
#else
        if (client->transfer_threads[i] != 0) {
            pthread_join(client->transfer_threads[i], NULL);
            client->transfer_threads[i] = 0;
        }
#endif
    }

    /* Close network connection (this will cause network thread to exit) */
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    /* Wait for network thread to exit */
    printf("Waiting for network thread to exit...\n");
#ifdef _WIN32
    if (client->network_thread != NULL) {
        WaitForSingleObject(client->network_thread, INFINITE);
        CloseHandle(client->network_thread);
        client->network_thread = NULL;
    }
#else
    if (client->network_thread != 0) {
        pthread_join(client->network_thread, NULL);
        client->network_thread = 0;
    }
#endif

    printf("All threads stopped\n");
    return 0;
}

/**
 * @brief Cleanup USB client
 */
void usb_client_cleanup(usb_client_t* client)
{
    int i;

    if (client == NULL) {
        return;
    }

    printf("Cleaning up USB client...\n");

    /* Close all USB devices */
    if (client->devices != NULL) {
        for (i = 0; i < client->device_count; i++) {
            usb_device_close(&client->devices[i]);
        }
        free(client->devices);
    }

    /* Free thread array */
    if (client->transfer_threads != NULL) {
        free(client->transfer_threads);
    }

    /* Free server IP */
    if (client->server_ip != NULL) {
        free(client->server_ip);
    }

    /* Destroy synchronization primitives */
#ifdef _WIN32
    if (client->shutdown_event != NULL) {
        CloseHandle(client->shutdown_event);
    }
    DeleteCriticalSection(&client->lock);
#else
    pthread_mutex_destroy(&client->lock);
    pthread_cond_destroy(&client->shutdown_cond);
#endif

    /* Free client structure */
    free(client);

    printf("USB client cleanup complete\n");
}

/* ========================================================================
 * Network Communication Functions
 * ======================================================================== */

/**
 * @brief Send URB to server
 */
int usb_client_send_urb(usb_client_t* client,
                        const usb_urb_header_t* urb_header,
                        const void* data,
                        uint32_t data_len)
{
    xoe_packet_t packet;
    int result;
    ssize_t sent;

    /* Validate parameters */
    if (client == NULL || urb_header == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (data_len > 0 && data == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Encapsulate URB into XOE packet */
    result = usb_protocol_encapsulate(urb_header, data, data_len, &packet);
    if (result != 0) {
        return result;
    }

    /* Send packet to server */
    sent = send(client->socket_fd, &packet, sizeof(xoe_packet_t), 0);
    if (sent < 0) {
        fprintf(stderr, "Failed to send URB to server: %s\n", strerror(errno));
        return E_NETWORK_ERROR;
    }

    /* Update statistics */
#ifdef _WIN32
    EnterCriticalSection(&client->lock);
#else
    pthread_mutex_lock(&client->lock);
#endif

    client->packets_sent++;

#ifdef _WIN32
    LeaveCriticalSection(&client->lock);
#else
    pthread_mutex_unlock(&client->lock);
#endif

    return 0;
}

/**
 * @brief Receive URB from server
 */
int usb_client_receive_urb(usb_client_t* client,
                           usb_urb_header_t* urb_header,
                           void* data,
                           uint32_t* data_len)
{
    xoe_packet_t packet;
    ssize_t received;
    int result;

    /* Validate parameters */
    if (client == NULL || urb_header == NULL || data_len == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Receive packet from server */
    received = recv(client->socket_fd, &packet, sizeof(xoe_packet_t), 0);
    if (received < 0) {
        if (errno == EINTR) {
            return E_INTERRUPTED;
        }
        fprintf(stderr, "Failed to receive URB from server: %s\n",
                strerror(errno));
        return E_NETWORK_ERROR;
    }

    if (received == 0) {
        /* Connection closed */
        return E_NETWORK_ERROR;
    }

    /* Decapsulate XOE packet into URB */
    result = usb_protocol_decapsulate(&packet, urb_header, data, data_len);
    if (result != 0) {
        fprintf(stderr, "Failed to decapsulate URB packet: error %d\n",
                result);
        return result;
    }

    /* Update statistics */
#ifdef _WIN32
    EnterCriticalSection(&client->lock);
#else
    pthread_mutex_lock(&client->lock);
#endif

    client->packets_received++;

#ifdef _WIN32
    LeaveCriticalSection(&client->lock);
#else
    pthread_mutex_unlock(&client->lock);
#endif

    return 0;
}

/* ========================================================================
 * Thread Entry Points
 * ======================================================================== */

/**
 * @brief Network receive thread
 *
 * Continuously receives URBs from server and dispatches them to
 * appropriate device handlers.
 */
#ifdef _WIN32
DWORD WINAPI usb_client_network_thread(LPVOID arg)
#else
void* usb_client_network_thread(void* arg)
#endif
{
    usb_client_t* client = (usb_client_t*)arg;
    usb_urb_header_t urb_header;
    unsigned char data_buffer[USB_MAX_TRANSFER_SIZE];
    uint32_t data_len;
    int result;
    int running;

    printf("Network receive thread started\n");

    while (1) {
        /* Check if shutdown requested */
#ifdef _WIN32
        EnterCriticalSection(&client->lock);
        running = client->running;
        LeaveCriticalSection(&client->lock);
#else
        pthread_mutex_lock(&client->lock);
        running = client->running;
        pthread_mutex_unlock(&client->lock);
#endif

        if (!running) {
            break;
        }

        /* Receive URB from server */
        data_len = sizeof(data_buffer);
        result = usb_client_receive_urb(client, &urb_header,
                                        data_buffer, &data_len);

        if (result == E_INTERRUPTED) {
            continue;  /* Signal interrupted, retry */
        }

        if (result != 0) {
            fprintf(stderr, "Network receive error: %d\n", result);
            break;  /* Fatal error, exit thread */
        }

        /* Phase 4 Stub: Dispatch URB to appropriate device handler
         * In future phases, this will route responses back to waiting
         * transfer contexts based on device_id and sequence number */
        printf("Received URB: device_id=0x%08x, endpoint=0x%02x, "
               "status=%d, actual_length=%u\n",
               urb_header.device_id, urb_header.endpoint,
               urb_header.status, urb_header.actual_length);
    }

    printf("Network receive thread exiting\n");

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/**
 * @brief Per-device USB transfer thread
 *
 * Handles USB transfers for a single device, reading from USB
 * and sending to network.
 */
#ifdef _WIN32
DWORD WINAPI usb_client_transfer_thread(LPVOID arg)
#else
void* usb_client_transfer_thread(void* arg)
#endif
{
    usb_transfer_thread_ctx_t* ctx = (usb_transfer_thread_ctx_t*)arg;
    usb_client_t* client;
    usb_device_t* device;
    usb_transfer_ctx_t* transfer_ctx = NULL;
    unsigned char buffer[USB_MAX_TRANSFER_SIZE];
    int result;
    int running;
    usb_urb_header_t urb_header;

    if (ctx == NULL) {
#ifdef _WIN32
        return 1;
#else
        return NULL;
#endif
    }

    client = ctx->client;
    device = ctx->device;

    printf("Transfer thread started for device %d (VID:PID %04x:%04x)\n",
           ctx->device_index + 1,
           device->config.vendor_id, device->config.product_id);

    /* Allocate transfer context for async operations */
    transfer_ctx = usb_transfer_alloc();
    if (transfer_ctx == NULL) {
        fprintf(stderr, "Failed to allocate transfer context for device %d\n",
                ctx->device_index + 1);
        free(ctx);
#ifdef _WIN32
        return 1;
#else
        return NULL;
#endif
    }

    while (1) {
        /* Check if shutdown requested */
#ifdef _WIN32
        EnterCriticalSection(&client->lock);
        running = client->running;
        LeaveCriticalSection(&client->lock);
#else
        pthread_mutex_lock(&client->lock);
        running = client->running;
        pthread_mutex_unlock(&client->lock);
#endif

        if (!running) {
            break;
        }

        /* Phase 4: Read from USB device
         *
         * For bulk IN transfers, continuously read from device
         * and send to network. This is a simplified implementation
         * that will be enhanced in future phases with proper
         * request/response matching. */

        if (device->config.bulk_in_endpoint != USB_NO_ENDPOINT) {
            int transferred = 0;

            /* Perform synchronous bulk read */
            result = usb_transfer_bulk_read(
                device,
                device->config.bulk_in_endpoint,
                buffer,
                USB_MAX_TRANSFER_SIZE,
                &transferred,
                device->config.transfer_timeout_ms
            );

            /* Handle timeout (expected for IN polling) */
            if (result == E_TIMEOUT) {
                continue;  /* No data available, retry */
            }

            /* Handle other errors */
            if (result != 0) {
                fprintf(stderr, "USB read error on device %d: %d\n",
                        ctx->device_index + 1, result);

                /* Update error statistics */
#ifdef _WIN32
                EnterCriticalSection(&client->lock);
#else
                pthread_mutex_lock(&client->lock);
#endif
                client->transfer_errors++;
#ifdef _WIN32
                LeaveCriticalSection(&client->lock);
#else
                pthread_mutex_unlock(&client->lock);
#endif

                continue;  /* Non-fatal error, retry */
            }

            /* Data received, prepare URB header */
            memset(&urb_header, 0, sizeof(usb_urb_header_t));
            urb_header.command = USB_CMD_SUBMIT;
            urb_header.device_id = ((uint32_t)device->config.vendor_id << 16) |
                                   device->config.product_id;
            urb_header.endpoint = device->config.bulk_in_endpoint;
            urb_header.transfer_type = USB_TRANSFER_BULK;
            urb_header.transfer_length = (uint32_t)transferred;
            urb_header.actual_length = (uint32_t)transferred;
            urb_header.status = 0;  /* Success */

            /* Send URB to server */
            result = usb_client_send_urb(client, &urb_header,
                                        buffer, (uint32_t)transferred);
            if (result != 0) {
                fprintf(stderr, "Failed to send URB for device %d: %d\n",
                        ctx->device_index + 1, result);
                break;  /* Network error, exit thread */
            }

            printf("Device %d: Sent %d bytes to server\n",
                   ctx->device_index + 1, transferred);
        } else {
            /* No IN endpoint configured, sleep briefly */
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);  /* 100ms */
#endif
        }
    }

    /* Cleanup */
    usb_transfer_free(transfer_ctx);
    free(ctx);

    printf("Transfer thread exiting for device %d\n", ctx->device_index + 1);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ========================================================================
 * Statistics and Status Functions
 * ======================================================================== */

/**
 * @brief Print client statistics
 */
void usb_client_print_stats(const usb_client_t* client)
{
    if (client == NULL) {
        return;
    }

    printf("\n========================================\n");
    printf(" USB Client Statistics\n");
    printf("========================================\n");
    printf("Devices:          %d / %d\n", client->device_count, client->max_devices);
    printf("Packets sent:     %lu\n", client->packets_sent);
    printf("Packets received: %lu\n", client->packets_received);
    printf("Transfer errors:  %lu\n", client->transfer_errors);
    printf("Running:          %s\n", client->running ? "Yes" : "No");
    printf("========================================\n\n");
}

/**
 * @brief Check if client is running
 */
int usb_client_is_running(const usb_client_t* client)
{
    int running;

    if (client == NULL) {
        return FALSE;
    }

#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION*)&client->lock);
    running = client->running;
    LeaveCriticalSection((CRITICAL_SECTION*)&client->lock);
#else
    pthread_mutex_lock((pthread_mutex_t*)&client->lock);
    running = client->running;
    pthread_mutex_unlock((pthread_mutex_t*)&client->lock);
#endif

    return running;
}
