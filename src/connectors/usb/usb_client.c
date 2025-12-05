/*
 * usb_client.c - USB Client Implementation
 *
 * Implements USB client coordinator for managing USB devices
 * over network connection.
 *
 * EXPERIMENTAL FEATURE - Phase 3 Implementation
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

    /* Phase 3 Stub: Full threading will be added in later phases
     * For now, we just maintain the connection for testing */
    printf("\nUSB Client started (Phase 3 stub - no active transfers)\n");
    printf("  Devices: %d\n", client->device_count);
    printf("  Server: %s:%d\n", client->server_ip, client->server_port);
    printf("  Socket FD: %d\n", client->socket_fd);
    printf("\nPress Ctrl+C to exit...\n\n");

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

    /* Close network connection */
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }

    printf("USB client stopped\n");
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
