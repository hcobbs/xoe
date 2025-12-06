/*
 * usb_server.c - USB Server Implementation
 *
 * Implements server-side USB request routing and forwarding.
 * Routes USB Request Blocks between multiple USB clients based on device_id.
 *
 * EXPERIMENTAL FEATURE - Phase 1 Implementation
 *
 * Author: [LLM-ARCH]
 * Date: 2025-12-05
 */

#include "usb_server.h"
#include "usb_protocol.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

/* ========================================================================
 * Server Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Initialize USB server
 */
usb_server_t* usb_server_init(void)
{
    usb_server_t* server;
    int i;

    /* Allocate server structure */
    server = (usb_server_t*)malloc(sizeof(usb_server_t));
    if (server == NULL) {
        return NULL;
    }

    /* Initialize client registry */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        server->clients[i].socket_fd = -1;
        server->clients[i].device_id = 0;
        server->clients[i].in_use = FALSE;
        pthread_mutex_init(&server->clients[i].send_lock, NULL);
    }

    /* Initialize locks */
    pthread_mutex_init(&server->registry_lock, NULL);

    /* Initialize statistics */
    server->packets_routed = 0;
    server->routing_errors = 0;
    server->active_clients = 0;

    return server;
}

/**
 * @brief Cleanup USB server
 */
void usb_server_cleanup(usb_server_t* server)
{
    int i;

    if (server == NULL) {
        return;
    }

    /* Destroy all mutexes */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        pthread_mutex_destroy(&server->clients[i].send_lock);
    }
    pthread_mutex_destroy(&server->registry_lock);

    /* Free server structure */
    free(server);
}

/* ========================================================================
 * Client Management Functions
 * ======================================================================== */

/**
 * @brief Register USB client
 */
int usb_server_register_client(usb_server_t* server,
                                int socket_fd,
                                uint32_t device_id)
{
    int i;
    int result = E_OUT_OF_MEMORY;

    if (server == NULL) {
        return E_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&server->registry_lock);

    /* Find available slot */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (!server->clients[i].in_use) {
            server->clients[i].socket_fd = socket_fd;
            server->clients[i].device_id = device_id;
            server->clients[i].in_use = TRUE;
            server->active_clients++;

            printf("USB Server: Registered client socket=%d device_id=0x%08x\n",
                   socket_fd, device_id);

            result = 0;
            break;
        }
    }

    pthread_mutex_unlock(&server->registry_lock);

    if (result != 0) {
        fprintf(stderr, "USB Server: Client registry full\n");
    }

    return result;
}

/**
 * @brief Unregister USB client
 */
int usb_server_unregister_client(usb_server_t* server,
                                  int socket_fd)
{
    int i;
    int result = E_NOT_FOUND;

    if (server == NULL) {
        return E_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&server->registry_lock);

    /* Find and remove client */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (server->clients[i].in_use &&
            server->clients[i].socket_fd == socket_fd) {

            printf("USB Server: Unregistered client socket=%d device_id=0x%08x\n",
                   socket_fd, server->clients[i].device_id);

            server->clients[i].socket_fd = -1;
            server->clients[i].device_id = 0;
            server->clients[i].in_use = FALSE;
            server->active_clients--;

            result = 0;
            break;
        }
    }

    pthread_mutex_unlock(&server->registry_lock);

    return result;
}

/* ========================================================================
 * URB Routing Functions
 * ======================================================================== */

/**
 * @brief Route URB to target client
 */
int usb_server_route_urb(usb_server_t* server,
                         const usb_urb_header_t* urb_header,
                         const void* data,
                         uint32_t data_len,
                         int sender_fd)
{
    xoe_packet_t packet;
    int target_fd = -1;
    int i;
    int result;
    ssize_t sent;

    /* Validate parameters */
    if (server == NULL || urb_header == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Encapsulate URB into XOE packet */
    result = usb_protocol_encapsulate(urb_header, data, data_len, &packet);
    if (result != 0) {
        server->routing_errors++;
        return result;
    }

    /* Find target client based on device_id */
    pthread_mutex_lock(&server->registry_lock);

    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (server->clients[i].in_use &&
            server->clients[i].device_id == urb_header->device_id &&
            server->clients[i].socket_fd != sender_fd) {

            target_fd = server->clients[i].socket_fd;
            break;
        }
    }

    pthread_mutex_unlock(&server->registry_lock);

    /* Check if target found */
    if (target_fd < 0) {
        fprintf(stderr, "USB Server: No route for device_id=0x%08x\n",
                urb_header->device_id);
        server->routing_errors++;
        return E_NOT_FOUND;
    }

    /* Send packet to target client */
    pthread_mutex_lock(&server->clients[i].send_lock);

    sent = send(target_fd, &packet, sizeof(xoe_packet_t), 0);

    pthread_mutex_unlock(&server->clients[i].send_lock);

    if (sent < 0) {
        fprintf(stderr, "USB Server: Failed to route packet: %s\n",
                strerror(errno));
        server->routing_errors++;
        usb_protocol_free_payload(&packet);  /* Free allocated payload */
        return E_NETWORK_ERROR;
    }

    /* Update statistics */
    server->packets_routed++;

    /* Free allocated payload after successful send */
    usb_protocol_free_payload(&packet);

    return 0;
}

/**
 * @brief Handle client registration request
 */
static int usb_server_handle_register(usb_server_t* server,
                                       const usb_urb_header_t* urb_header,
                                       int sender_fd)
{
    xoe_packet_t response;
    usb_urb_header_t response_urb;
    int result;
    ssize_t sent;

    /* Register client with device_id from URB header */
    result = usb_server_register_client(server, sender_fd,
                                         urb_header->device_id);

    /* Build registration response */
    memset(&response_urb, 0, sizeof(response_urb));
    response_urb.command = USB_RET_REGISTER;
    response_urb.seqnum = urb_header->seqnum;
    response_urb.device_id = urb_header->device_id;
    response_urb.status = result;

    /* Encapsulate response */
    result = usb_protocol_encapsulate(&response_urb, NULL, 0, &response);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to encapsulate registration response\n");
        return result;
    }

    /* Send response back to client */
    sent = send(sender_fd, &response, sizeof(xoe_packet_t), 0);
    if (sent < 0) {
        fprintf(stderr, "USB Server: Failed to send registration response: %s\n",
                strerror(errno));
        usb_protocol_free_payload(&response);  /* Free allocated payload */
        return E_NETWORK_ERROR;
    }

    /* Free allocated payload after successful send */
    usb_protocol_free_payload(&response);

    return 0;
}

/**
 * @brief Handle client unregistration request
 */
static int usb_server_handle_unregister(usb_server_t* server,
                                         const usb_urb_header_t* urb_header,
                                         int sender_fd)
{
    xoe_packet_t response;
    usb_urb_header_t response_urb;
    int result;
    ssize_t sent;

    /* Unregister client */
    result = usb_server_unregister_client(server, sender_fd);

    /* Build unregistration response */
    memset(&response_urb, 0, sizeof(response_urb));
    response_urb.command = USB_RET_UNREGISTER;
    response_urb.seqnum = urb_header->seqnum;
    response_urb.device_id = urb_header->device_id;
    response_urb.status = result;

    /* Encapsulate response */
    result = usb_protocol_encapsulate(&response_urb, NULL, 0, &response);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to encapsulate unregistration response\n");
        return result;
    }

    /* Send response back to client */
    sent = send(sender_fd, &response, sizeof(xoe_packet_t), 0);
    if (sent < 0) {
        fprintf(stderr, "USB Server: Failed to send unregistration response: %s\n",
                strerror(errno));
        usb_protocol_free_payload(&response);  /* Free allocated payload */
        return E_NETWORK_ERROR;
    }

    /* Free allocated payload after successful send */
    usb_protocol_free_payload(&response);

    return 0;
}

/**
 * @brief Handle incoming URB from client
 */
int usb_server_handle_urb(usb_server_t* server,
                          const xoe_packet_t* packet,
                          int sender_fd)
{
    usb_urb_header_t urb_header;
    unsigned char data_buffer[USB_MAX_TRANSFER_SIZE];
    uint32_t data_len;
    int result;

    /* Validate parameters */
    if (server == NULL || packet == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Decapsulate XOE packet */
    data_len = sizeof(data_buffer);
    result = usb_protocol_decapsulate(packet, &urb_header,
                                      data_buffer, &data_len);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to decapsulate URB: error %d\n",
                result);
        server->routing_errors++;
        return result;
    }

    /* Handle command based on type */
    switch (urb_header.command) {
        case USB_CMD_REGISTER:
            return usb_server_handle_register(server, &urb_header, sender_fd);

        case USB_CMD_UNREGISTER:
            return usb_server_handle_unregister(server, &urb_header, sender_fd);

        case USB_CMD_SUBMIT:
        case USB_RET_SUBMIT:
            /* Route URB to target client */
            result = usb_server_route_urb(server, &urb_header,
                                          data_buffer, urb_header.actual_length,
                                          sender_fd);
            return result;

        default:
            fprintf(stderr, "USB Server: Unknown command type: 0x%04x\n",
                    urb_header.command);
            server->routing_errors++;
            return E_INVALID_ARGUMENT;
    }
}

/* ========================================================================
 * Statistics and Status Functions
 * ======================================================================== */

/**
 * @brief Print server statistics
 */
void usb_server_print_stats(const usb_server_t* server)
{
    int i;

    if (server == NULL) {
        return;
    }

    printf("\n========================================\n");
    printf(" USB Server Statistics\n");
    printf("========================================\n");
    printf("Active clients:   %lu\n", server->active_clients);
    printf("Packets routed:   %lu\n", server->packets_routed);
    printf("Routing errors:   %lu\n", server->routing_errors);
    printf("\n");
    printf("Registered Devices:\n");

    pthread_mutex_lock((pthread_mutex_t*)&server->registry_lock);

    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (server->clients[i].in_use) {
            printf("  [%d] socket=%d device_id=0x%08x\n",
                   i, server->clients[i].socket_fd,
                   server->clients[i].device_id);
        }
    }

    pthread_mutex_unlock((pthread_mutex_t*)&server->registry_lock);

    printf("========================================\n\n");
}
