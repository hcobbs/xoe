/*
 * usb_client.c - USB Client Implementation
 *
 * Implements USB client coordinator for managing USB devices
 * over network connection.
 *
 * EXPERIMENTAL FEATURE - Phase 5 Implementation
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_client.h"
#include "usb_protocol.h"
#include "lib/common/definitions.h"
#include "lib/net/net_resolve.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>


/* ========================================================================
 * Internal Helper Functions
 * ======================================================================== */

/**
 * @brief Get current time in milliseconds
 *
 * @return Current time in milliseconds since epoch
 */
static unsigned long get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}

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
    {
        size_t ip_len = strlen(server_ip);
        client->server_ip = (char*)malloc(ip_len + 1);
        if (client->server_ip == NULL) {
            free(client);
            return NULL;
        }
        memcpy(client->server_ip, server_ip, ip_len + 1);  /* Safe: explicit size */
    }
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
    pthread_mutex_init(&client->lock, NULL);
    pthread_cond_init(&client->shutdown_cond, NULL);

    /* Initialize statistics */
    client->packets_sent = 0;
    client->packets_received = 0;
    client->transfer_errors = 0;

    /* Phase 5: Initialize request/response tracking */
    client->next_seqnum = 1;
    client->pending_head = NULL;
    client->pending_count = 0;
    client->timeouts = 0;

    pthread_mutex_init(&client->pending_lock, NULL);

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
    net_resolve_result_t resolve_result;
    char error_buf[256];
    int result;

    /* Resolve hostname/IP and connect to server */
    result = net_resolve_connect(client->server_ip, client->server_port,
                                  &client->socket_fd, &resolve_result);
    if (result != 0) {
        net_resolve_format_error(&resolve_result, error_buf, sizeof(error_buf));
        fprintf(stderr, "Failed to connect to %s:%d: %s\n",
                client->server_ip, client->server_port, error_buf);
        client->socket_fd = -1;
        return result;
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

    /* Register all devices with server */
    {
        int i;
        printf("\nRegistering USB devices with server...\n");
        for (i = 0; i < client->device_count; i++) {
            uint32_t device_id;

            /* Construct device_id from VID:PID */
            device_id = ((uint32_t)client->devices[i].config.vendor_id << 16) |
                        client->devices[i].config.product_id;

            printf("  Registering device %d: VID:PID %04x:%04x (device_id=0x%08x)\n",
                   i + 1,
                   client->devices[i].config.vendor_id,
                   client->devices[i].config.product_id,
                   device_id);

            result = usb_client_register_device(client, device_id, 5000);
            if (result != 0) {
                fprintf(stderr, "Failed to register device %d: error %d\n",
                        i + 1, result);
                fprintf(stderr, "Closing server connection\n");
                close(client->socket_fd);
                client->socket_fd = -1;
                return result;
            }
        }
        printf("All devices registered successfully\n\n");
    }

    /* Mark as running */
    pthread_mutex_lock(&client->lock);

    client->running = TRUE;
    client->shutdown_requested = FALSE;

    pthread_mutex_unlock(&client->lock);

    /* Phase 4: Spawn worker threads for active USB transfers */
    printf("\nStarting USB client threads...\n");
    printf("  Devices: %d\n", client->device_count);
    printf("  Server: %s:%d\n", client->server_ip, client->server_port);
    printf("\n");

    /* Spawn network receive thread */
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
            result = pthread_create(&client->transfer_threads[i], NULL,
                                   usb_client_transfer_thread, ctx);
            if (result != 0) {
                fprintf(stderr, "Failed to create transfer thread for device %d: %s\n",
                        i + 1, strerror(result));
                free(ctx);
                continue;
            }

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

    /* Phase 5: Wait for shutdown with periodic timeout cleanup */
    pthread_mutex_lock(&client->lock);
    while (!client->shutdown_requested) {
        struct timespec timeout;
        struct timeval now;
        int wait_result;

        /* Wait for 1 second or until shutdown */
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 1;
        timeout.tv_nsec = now.tv_usec * 1000;

        wait_result = pthread_cond_timedwait(&client->shutdown_cond,
                                             &client->lock,
                                             &timeout);

        /* Unlock to allow cleanup to acquire pending_lock */
        pthread_mutex_unlock(&client->lock);

        /* Clean up timed-out requests */
        usb_client_cleanup_timeouts(client);

        /* Reacquire lock for next iteration */
        pthread_mutex_lock(&client->lock);

        (void)wait_result;  /* Suppress unused variable warning */
    }
    pthread_mutex_unlock(&client->lock);

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
    pthread_mutex_lock(&client->lock);

    client->shutdown_requested = TRUE;
    client->running = FALSE;

    pthread_cond_broadcast(&client->shutdown_cond);
    pthread_mutex_unlock(&client->lock);

    /* Phase 4: Wait for transfer threads to exit */
    printf("Waiting for transfer threads to exit...\n");
    for (i = 0; i < client->device_count; i++) {
        if (client->transfer_threads[i] != 0) {
            pthread_join(client->transfer_threads[i], NULL);
            client->transfer_threads[i] = 0;
        }
    }

    /* Unregister all devices from server before disconnecting */
    if (client->socket_fd >= 0) {
        printf("Unregistering devices from server...\n");
        for (i = 0; i < client->device_count; i++) {
            uint32_t device_id;
            int result;

            device_id = ((uint32_t)client->devices[i].config.vendor_id << 16) |
                        client->devices[i].config.product_id;

            result = usb_client_unregister_device(client, device_id);
            if (result != 0) {
                fprintf(stderr, "Warning: Failed to unregister device %d: error %d\n",
                        i + 1, result);
            }
        }
    }

    /* Close network connection (this will cause network thread to exit) */
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    /* Wait for network thread to exit */
    printf("Waiting for network thread to exit...\n");
    if (client->network_thread != 0) {
        pthread_join(client->network_thread, NULL);
        client->network_thread = 0;
    }

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

    /* Phase 5: Clean up any remaining pending requests */
    {
        usb_pending_request_t* req = client->pending_head;
        usb_pending_request_t* next;
        while (req != NULL) {
            next = req->next;
            pthread_mutex_destroy(&req->mutex);
            pthread_cond_destroy(&req->cond);
            free(req);
            req = next;
        }
    }

    /* Destroy synchronization primitives */
    pthread_mutex_destroy(&client->lock);
    pthread_cond_destroy(&client->shutdown_cond);
    pthread_mutex_destroy(&client->pending_lock);

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
        usb_protocol_free_payload(&packet);  /* Free allocated payload */
        return E_NETWORK_ERROR;
    }

    /* Update statistics */
    pthread_mutex_lock(&client->lock);

    client->packets_sent++;

    pthread_mutex_unlock(&client->lock);

    /* Free allocated payload after successful send */
    usb_protocol_free_payload(&packet);

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
    pthread_mutex_lock(&client->lock);

    client->packets_received++;

    pthread_mutex_unlock(&client->lock);

    return 0;
}

/**
 * @brief Register device with server
 */
int usb_client_register_device(usb_client_t* client,
                                uint32_t device_id,
                                unsigned int timeout_ms)
{
    usb_urb_header_t reg_urb, response_urb;
    uint32_t response_len;
    int result;

    /* Validate parameters */
    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Build registration URB */
    memset(&reg_urb, 0, sizeof(reg_urb));
    reg_urb.command = USB_CMD_REGISTER;
    reg_urb.seqnum = usb_client_alloc_seqnum(client);
    reg_urb.device_id = device_id;

    /* Send registration request */
    result = usb_client_send_urb(client, &reg_urb, NULL, 0);
    if (result != 0) {
        fprintf(stderr, "Failed to send registration request: error %d\n",
                result);
        return result;
    }

    /* Wait for registration response (with timeout) */
    /* For now, use simple blocking receive - timeout handling TBD */
    (void)timeout_ms;  /* Unused for now */

    response_len = 0;
    result = usb_client_receive_urb(client, &response_urb, NULL,
                                     &response_len);
    if (result != 0) {
        fprintf(stderr, "Failed to receive registration response: error %d\n",
                result);
        return result;
    }

    /* Verify response matches request */
    if (response_urb.command != USB_RET_REGISTER) {
        fprintf(stderr, "Unexpected response command: 0x%04x\n",
                response_urb.command);
        return E_PROTOCOL_ERROR;
    }

    if (response_urb.seqnum != reg_urb.seqnum) {
        fprintf(stderr, "Sequence number mismatch: expected %u, got %u\n",
                reg_urb.seqnum, response_urb.seqnum);
        return E_PROTOCOL_ERROR;
    }

    /* Check registration result */
    if (response_urb.status != 0) {
        fprintf(stderr, "Server registration failed: status %d\n",
                response_urb.status);
        return response_urb.status;
    }

    printf("Device 0x%08x registered with server successfully\n", device_id);
    return 0;
}

/**
 * @brief Unregister device from server
 */
int usb_client_unregister_device(usb_client_t* client,
                                  uint32_t device_id)
{
    usb_urb_header_t unreg_urb, response_urb;
    uint32_t response_len;
    int result;

    /* Validate parameters */
    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Build unregistration URB */
    memset(&unreg_urb, 0, sizeof(unreg_urb));
    unreg_urb.command = USB_CMD_UNREGISTER;
    unreg_urb.seqnum = usb_client_alloc_seqnum(client);
    unreg_urb.device_id = device_id;

    /* Send unregistration request */
    result = usb_client_send_urb(client, &unreg_urb, NULL, 0);
    if (result != 0) {
        fprintf(stderr, "Failed to send unregistration request: error %d\n",
                result);
        return result;
    }

    /* Wait for unregistration response */
    response_len = 0;
    result = usb_client_receive_urb(client, &response_urb, NULL,
                                     &response_len);
    if (result != 0) {
        fprintf(stderr, "Failed to receive unregistration response: error %d\n",
                result);
        return result;
    }

    /* Verify response matches request */
    if (response_urb.command != USB_RET_UNREGISTER) {
        fprintf(stderr, "Unexpected response command: 0x%04x\n",
                response_urb.command);
        return E_PROTOCOL_ERROR;
    }

    if (response_urb.seqnum != unreg_urb.seqnum) {
        fprintf(stderr, "Sequence number mismatch: expected %u, got %u\n",
                unreg_urb.seqnum, response_urb.seqnum);
        return E_PROTOCOL_ERROR;
    }

    /* Check unregistration result */
    if (response_urb.status != 0) {
        fprintf(stderr, "Server unregistration failed: status %d\n",
                response_urb.status);
        return response_urb.status;
    }

    printf("Device 0x%08x unregistered from server\n", device_id);
    return 0;
}

/**
 * @brief Submit URB with bidirectional request/response
 *
 * Phase 5.5: High-level API for bidirectional USB transfers.
 * Combines send, pending request creation, wait, and cleanup.
 */
int usb_client_submit_urb_sync(usb_client_t* client,
                                usb_urb_header_t* urb_header,
                                const void* send_data,
                                uint32_t send_len,
                                void* recv_data,
                                uint32_t recv_size,
                                uint32_t* actual_len,
                                unsigned int timeout_ms)
{
    usb_pending_request_t* request = NULL;
    uint32_t seqnum;
    int result;

    /* Validate parameters */
    if (client == NULL || urb_header == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Allocate sequence number */
    seqnum = usb_client_alloc_seqnum(client);
    if (seqnum == 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Update URB header with sequence number */
    urb_header->seqnum = seqnum;

    /* Create pending request for response matching */
    request = usb_client_create_pending_request(
        client,
        seqnum,
        urb_header->device_id,
        urb_header->endpoint,
        recv_data,
        recv_size,
        timeout_ms
    );

    if (request == NULL) {
        return E_OUT_OF_MEMORY;
    }

    /* Send URB to server */
    result = usb_client_send_urb(client, urb_header, send_data, send_len);
    if (result != 0) {
        usb_client_free_pending_request(client, request);
        return result;
    }

    /* Wait for response */
    result = usb_client_wait_pending_request(client, request);

    /* Copy actual length from completed request */
    if (actual_len != NULL) {
        *actual_len = request->response_received;
    }

    /* Check result status */
    if (result == 0 && request->status != 0) {
        result = request->status;
    }

    /* Cleanup pending request */
    usb_client_free_pending_request(client, request);

    return result;
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
void* usb_client_network_thread(void* arg)
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
        pthread_mutex_lock(&client->lock);
        running = client->running;
        pthread_mutex_unlock(&client->lock);

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

        /* Phase 5: Route response to pending request */
        result = usb_client_complete_pending_request(
            client,
            urb_header.seqnum,
            data_buffer,
            urb_header.actual_length,
            urb_header.status
        );

        if (result == E_NOT_FOUND) {
            /* No pending request found - may be unsolicited data or timeout */
            printf("Warning: Received URB with no matching request "
                   "(seqnum=%u, device_id=0x%08x, endpoint=0x%02x)\n",
                   urb_header.seqnum, urb_header.device_id, urb_header.endpoint);
        } else if (result != 0) {
            fprintf(stderr, "Error completing pending request: %d\n", result);
        }

        /* Update statistics */
        pthread_mutex_lock(&client->lock);
        client->packets_received++;
        pthread_mutex_unlock(&client->lock);
    }

    printf("Network receive thread exiting\n");

    return NULL;
}

/**
 * @brief Per-device USB transfer thread
 *
 * Handles USB transfers for a single device, reading from USB
 * and sending to network.
 */
void* usb_client_transfer_thread(void* arg)
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
        return NULL;
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
        return NULL;
    }

    while (1) {
        /* Check if shutdown requested */
        pthread_mutex_lock(&client->lock);
        running = client->running;
        pthread_mutex_unlock(&client->lock);

        if (!running) {
            break;
        }

        /* Phase 5.5: Bidirectional USB transfers
         *
         * IN endpoint (USB→Network): Read from device, send to server
         * OUT endpoint (Network→USB): Send request to server, wait for data,
         *                              write to device
         */

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
                pthread_mutex_lock(&client->lock);
                client->transfer_errors++;
                pthread_mutex_unlock(&client->lock);

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
        }

        /* Phase 5.5: Handle OUT endpoint (Network→USB) */
        if (device->config.bulk_out_endpoint != USB_NO_ENDPOINT) {
            unsigned char out_buffer[USB_MAX_TRANSFER_SIZE];
            uint32_t actual_len = 0;

            /* Prepare OUT request URB */
            memset(&urb_header, 0, sizeof(usb_urb_header_t));
            urb_header.command = USB_CMD_SUBMIT;
            urb_header.device_id = ((uint32_t)device->config.vendor_id << 16) |
                                   device->config.product_id;
            urb_header.endpoint = device->config.bulk_out_endpoint;
            urb_header.transfer_type = USB_TRANSFER_BULK;
            urb_header.transfer_length = USB_MAX_TRANSFER_SIZE;

            /* Submit bidirectional request to server */
            result = usb_client_submit_urb_sync(
                client,
                &urb_header,
                NULL,                       /* No data to send */
                0,
                out_buffer,                 /* Buffer for response */
                USB_MAX_TRANSFER_SIZE,
                &actual_len,
                device->config.transfer_timeout_ms
            );

            /* Handle timeout (no OUT data available from server) */
            if (result == E_TIMEOUT) {
                continue;  /* Expected, retry */
            }

            /* Handle other errors */
            if (result != 0) {
                fprintf(stderr, "USB OUT request error on device %d: %d\n",
                        ctx->device_index + 1, result);
                continue;  /* Non-fatal, retry */
            }

            /* Write received data to USB device */
            if (actual_len > 0) {
                int transferred = 0;

                result = usb_transfer_bulk_write(
                    device,
                    device->config.bulk_out_endpoint,
                    out_buffer,
                    (int)actual_len,
                    &transferred,
                    device->config.transfer_timeout_ms
                );

                if (result != 0) {
                    fprintf(stderr, "USB OUT write error on device %d: %d\n",
                            ctx->device_index + 1, result);

                    pthread_mutex_lock(&client->lock);
                    client->transfer_errors++;
                    pthread_mutex_unlock(&client->lock);

                    continue;
                }

                printf("Device %d: Wrote %d bytes to USB OUT endpoint\n",
                       ctx->device_index + 1, transferred);
            }
        }

        /* If no endpoints configured, sleep briefly */
        if (device->config.bulk_in_endpoint == USB_NO_ENDPOINT &&
            device->config.bulk_out_endpoint == USB_NO_ENDPOINT) {
            usleep(100000);  /* 100ms */
        }
    }

    /* Cleanup */
    usb_transfer_free(transfer_ctx);
    free(ctx);

    printf("Transfer thread exiting for device %d\n", ctx->device_index + 1);

    return NULL;
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
    printf("Pending requests: %lu\n", client->pending_count);
    printf("Timeouts:         %lu\n", client->timeouts);
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

    pthread_mutex_lock((pthread_mutex_t*)&client->lock);
    running = client->running;
    pthread_mutex_unlock((pthread_mutex_t*)&client->lock);

    return running;
}

/* ========================================================================
 * Phase 5: Request/Response Tracking Functions
 * ======================================================================== */

/**
 * @brief Allocate sequence number for new request
 */
uint32_t usb_client_alloc_seqnum(usb_client_t* client)
{
    uint32_t seqnum;

    if (client == NULL) {
        return 0;
    }

    pthread_mutex_lock(&client->pending_lock);
    seqnum = client->next_seqnum++;
    pthread_mutex_unlock(&client->pending_lock);

    return seqnum;
}

/**
 * @brief Create and enqueue pending request
 */
usb_pending_request_t* usb_client_create_pending_request(
    usb_client_t* client,
    uint32_t seqnum,
    uint32_t device_id,
    uint8_t endpoint,
    void* response_buffer,
    uint32_t response_size,
    unsigned int timeout_ms
)
{
    usb_pending_request_t* request;

    /* Validate parameters */
    if (client == NULL) {
        return NULL;
    }

    /* Allocate request structure */
    request = (usb_pending_request_t*)malloc(sizeof(usb_pending_request_t));
    if (request == NULL) {
        return NULL;
    }

    /* Initialize request fields */
    memset(request, 0, sizeof(usb_pending_request_t));
    request->seqnum = seqnum;
    request->device_id = device_id;
    request->endpoint = endpoint;
    request->response_data = response_buffer;
    request->response_size = response_size;
    request->response_received = 0;
    request->status = 0;
    request->completed = FALSE;
    request->timestamp_ms = get_time_ms();
    request->timeout_ms = timeout_ms;
    request->next = NULL;

    /* Initialize synchronization */
    pthread_mutex_init(&request->mutex, NULL);
    pthread_cond_init(&request->cond, NULL);

    /* Add to pending queue */
    pthread_mutex_lock(&client->pending_lock);

    request->next = client->pending_head;
    client->pending_head = request;
    client->pending_count++;

    pthread_mutex_unlock(&client->pending_lock);

    return request;
}

/**
 * @brief Wait for pending request completion
 */
int usb_client_wait_pending_request(
    usb_client_t* client,
    usb_pending_request_t* request
)
{
    int result = 0;

    /* Validate parameters */
    if (client == NULL || request == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Wait with timeout using condition variable */
    {
        struct timespec abs_timeout;
        struct timeval now;
        unsigned long timeout_ms = request->timeout_ms;

        gettimeofday(&now, NULL);
        abs_timeout.tv_sec = now.tv_sec + (timeout_ms / 1000);
        abs_timeout.tv_nsec = (now.tv_usec * 1000) +
                             ((timeout_ms % 1000) * 1000000);

        /* Handle nanosecond overflow */
        if (abs_timeout.tv_nsec >= 1000000000) {
            abs_timeout.tv_sec++;
            abs_timeout.tv_nsec -= 1000000000;
        }

        pthread_mutex_lock(&request->mutex);
        while (!request->completed && result == 0) {
            int wait_result = pthread_cond_timedwait(&request->cond,
                                                     &request->mutex,
                                                     &abs_timeout);
            if (wait_result == ETIMEDOUT) {
                result = E_TIMEOUT;
            } else if (wait_result != 0) {
                result = E_IO_ERROR;
            }
        }
        pthread_mutex_unlock(&request->mutex);
    }

    return result;
}

/**
 * @brief Complete pending request with response data
 */
int usb_client_complete_pending_request(
    usb_client_t* client,
    uint32_t seqnum,
    const void* data,
    uint32_t data_len,
    int32_t status
)
{
    usb_pending_request_t* request;
    int found = FALSE;

    /* Validate parameters */
    if (client == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Find request in pending queue */
    pthread_mutex_lock(&client->pending_lock);

    request = client->pending_head;
    while (request != NULL) {
        if (request->seqnum == seqnum) {
            found = TRUE;
            break;
        }
        request = request->next;
    }

    pthread_mutex_unlock(&client->pending_lock);

    if (!found) {
        return E_NOT_FOUND;
    }

    /* Copy response data */
    pthread_mutex_lock(&request->mutex);

    if (data != NULL && data_len > 0 && request->response_data != NULL) {
        uint32_t copy_len = (data_len < request->response_size) ?
                           data_len : request->response_size;
        memcpy(request->response_data, data, copy_len);
        request->response_received = copy_len;
    }
    request->status = status;
    request->completed = TRUE;

    /* Signal condition variable */
    pthread_cond_signal(&request->cond);
    pthread_mutex_unlock(&request->mutex);

    return 0;
}

/**
 * @brief Free pending request
 */
void usb_client_free_pending_request(
    usb_client_t* client,
    usb_pending_request_t* request
)
{
    usb_pending_request_t* prev;
    usb_pending_request_t* curr;

    if (client == NULL || request == NULL) {
        return;
    }

    /* Remove from pending queue */
    pthread_mutex_lock(&client->pending_lock);

    prev = NULL;
    curr = client->pending_head;
    while (curr != NULL) {
        if (curr == request) {
            if (prev == NULL) {
                client->pending_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            client->pending_count--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&client->pending_lock);

    /* Destroy synchronization primitives */
    pthread_mutex_destroy(&request->mutex);
    pthread_cond_destroy(&request->cond);

    /* Free request structure */
    free(request);
}

/**
 * @brief Clean up timed-out requests
 */
int usb_client_cleanup_timeouts(usb_client_t* client)
{
    usb_pending_request_t* request;
    usb_pending_request_t* next;
    usb_pending_request_t* prev;
    unsigned long current_time;
    int timeout_count = 0;

    if (client == NULL) {
        return 0;
    }

    current_time = get_time_ms();

    /* Scan pending queue for timeouts */
    pthread_mutex_lock(&client->pending_lock);

    prev = NULL;
    request = client->pending_head;
    while (request != NULL) {
        next = request->next;

        /* Check if request has timed out */
        if (!request->completed &&
            (current_time - request->timestamp_ms) >= request->timeout_ms) {

            /* Mark as timed out and signal waiting thread */
            pthread_mutex_lock(&request->mutex);
            request->completed = TRUE;
            request->status = E_TIMEOUT;
            pthread_cond_signal(&request->cond);
            pthread_mutex_unlock(&request->mutex);

            timeout_count++;
            client->timeouts++;
        }

        prev = request;
        request = next;
    }

    pthread_mutex_unlock(&client->pending_lock);

    return timeout_count;
}
