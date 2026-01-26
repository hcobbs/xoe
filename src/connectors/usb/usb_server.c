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
#include "usb_config.h"
#include "lib/common/definitions.h"
#include "lib/protocol/wire_format.h"
#include "lib/security/usb_auth.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

/* ========================================================================
 * Server Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Initialize USB server
 */
usb_server_t* usb_server_init(void)
{
    usb_server_t* server = NULL;
    int i = 0;

    /* Allocate server structure */
    server = (usb_server_t*)malloc(sizeof(usb_server_t));
    if (server == NULL) {
        return NULL;
    }

    /* Initialize client registry */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        server->clients[i].socket_fd = -1;
        server->clients[i].device_id = 0;
        server->clients[i].device_class = 0;
        server->clients[i].in_use = FALSE;
        server->clients[i].authenticated = FALSE;
        server->clients[i].auth_pending = FALSE;
        memset(server->clients[i].pending_challenge, 0, USB_AUTH_CHALLENGE_SIZE);
        memset(server->clients[i].client_ip, 0, sizeof(server->clients[i].client_ip));
        pthread_mutex_init(&server->clients[i].send_lock, NULL);
    }

    /* Initialize locks */
    pthread_mutex_init(&server->registry_lock, NULL);

    /* Initialize security configuration (auth disabled by default) */
    memset(server->auth_secret, 0, sizeof(server->auth_secret));
    memset(server->allowed_classes, 0, sizeof(server->allowed_classes));
    server->allowed_class_count = 0;
    server->require_auth = FALSE;

    /* Initialize statistics */
    server->packets_routed = 0;
    server->routing_errors = 0;
    server->active_clients = 0;
    server->auth_failures = 0;

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
    int target_index = -1;
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

    /* Find target client based on device_id (USB-008 race fix) */
    /* Acquire send_lock while holding registry_lock to prevent slot reuse */
    pthread_mutex_lock(&server->registry_lock);

    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (server->clients[i].in_use &&
            server->clients[i].device_id == urb_header->device_id &&
            server->clients[i].socket_fd != sender_fd) {

            target_fd = server->clients[i].socket_fd;
            target_index = i;
            break;
        }
    }

    /* Check if target found */
    if (target_fd < 0) {
        pthread_mutex_unlock(&server->registry_lock);
        fprintf(stderr, "USB Server: No route for device_id=0x%08x\n",
                urb_header->device_id);
        server->routing_errors++;
        usb_protocol_free_payload(&packet);
        return E_NOT_FOUND;
    }

    /* Lock send_lock while holding registry_lock (prevents race) */
    pthread_mutex_lock(&server->clients[target_index].send_lock);
    pthread_mutex_unlock(&server->registry_lock);

    /* Send packet to target client using wire format (LIB-001/NET-006 fix) */
    result = xoe_wire_send(target_fd, &packet);

    pthread_mutex_unlock(&server->clients[target_index].send_lock);

    (void)sent;  /* Suppress unused warning */

    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to route packet: error %d\n",
                result);
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
 * @brief Send authentication challenge to client
 */
static int usb_server_send_auth_challenge(usb_server_t* server,
                                           int sender_fd,
                                           int client_index,
                                           uint32_t seqnum)
{
    xoe_packet_t response;
    usb_urb_header_t response_urb;
    usb_auth_payload_t auth_payload;
    int result = 0;

    /* Generate challenge */
    result = usb_auth_generate_challenge(server->clients[client_index].pending_challenge);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to generate auth challenge\n");
        return result;
    }

    /* Mark auth as pending */
    server->clients[client_index].auth_pending = TRUE;

    /* Build auth challenge response */
    memset(&response_urb, 0, sizeof(response_urb));
    response_urb.command = USB_CMD_AUTH;
    response_urb.seqnum = seqnum;
    response_urb.device_id = server->clients[client_index].device_id;
    response_urb.status = E_USB_AUTH_REQUIRED;

    /* Build auth payload with challenge */
    memset(&auth_payload, 0, sizeof(auth_payload));
    memcpy(auth_payload.challenge, server->clients[client_index].pending_challenge,
           USB_AUTH_CHALLENGE_SIZE);
    auth_payload.device_id = server->clients[client_index].device_id;
    auth_payload.device_class = server->clients[client_index].device_class;

    /* Encapsulate with auth payload */
    result = usb_protocol_encapsulate(&response_urb, &auth_payload,
                                       sizeof(auth_payload), &response);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to encapsulate auth challenge\n");
        server->clients[client_index].auth_pending = FALSE;
        return result;
    }

    /* Send challenge */
    result = xoe_wire_send(sender_fd, &response);
    usb_protocol_free_payload(&response);

    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to send auth challenge: error %d\n", result);
        server->clients[client_index].auth_pending = FALSE;
        return E_NETWORK_ERROR;
    }

    printf("USB Server: Auth challenge sent to client socket=%d\n", sender_fd);
    return 0;
}

/**
 * @brief Complete successful registration
 */
static int usb_server_complete_registration(usb_server_t* server,
                                             int sender_fd,
                                             int client_index,
                                             uint32_t seqnum)
{
    xoe_packet_t response;
    usb_urb_header_t response_urb;
    int result = 0;

    /* Mark as authenticated and registered */
    server->clients[client_index].authenticated = TRUE;
    server->clients[client_index].auth_pending = FALSE;
    server->clients[client_index].in_use = TRUE;
    server->active_clients++;

    /* Log success */
    usb_auth_log_event(server->clients[client_index].client_ip,
                       server->clients[client_index].device_id,
                       server->clients[client_index].device_class,
                       1, NULL);

    printf("USB Server: Registered client socket=%d device_id=0x%08x class=0x%02x\n",
           sender_fd, server->clients[client_index].device_id,
           server->clients[client_index].device_class);

    /* Build success response */
    memset(&response_urb, 0, sizeof(response_urb));
    response_urb.command = USB_RET_REGISTER;
    response_urb.seqnum = seqnum;
    response_urb.device_id = server->clients[client_index].device_id;
    response_urb.status = 0;

    /* Encapsulate response */
    result = usb_protocol_encapsulate(&response_urb, NULL, 0, &response);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to encapsulate registration response\n");
        return result;
    }

    /* Send response */
    result = xoe_wire_send(sender_fd, &response);
    usb_protocol_free_payload(&response);

    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to send registration response: error %d\n",
                result);
        return E_NETWORK_ERROR;
    }

    return 0;
}

/**
 * @brief Send registration failure response
 */
static int usb_server_send_register_failure(int sender_fd,
                                             uint32_t seqnum,
                                             uint32_t device_id,
                                             int error_code)
{
    xoe_packet_t response;
    usb_urb_header_t response_urb;
    int result = 0;

    memset(&response_urb, 0, sizeof(response_urb));
    response_urb.command = USB_RET_REGISTER;
    response_urb.seqnum = seqnum;
    response_urb.device_id = device_id;
    response_urb.status = error_code;

    result = usb_protocol_encapsulate(&response_urb, NULL, 0, &response);
    if (result != 0) {
        return result;
    }

    result = xoe_wire_send(sender_fd, &response);
    usb_protocol_free_payload(&response);

    return result;
}

/**
 * @brief Handle client registration request
 */
static int usb_server_handle_register(usb_server_t* server,
                                       const usb_urb_header_t* urb_header,
                                       int sender_fd)
{
    int i = 0;
    int slot = -1;
    uint8_t device_class = 0;
    char client_ip[46];

    /* Extract device class from endpoint field (protocol convention) */
    device_class = urb_header->endpoint;

    /* Get client IP for logging */
    usb_server_get_client_ip(sender_fd, client_ip, sizeof(client_ip));

    pthread_mutex_lock(&server->registry_lock);

    /* Check device class whitelist first */
    if (!usb_auth_check_class_whitelist(device_class,
                                         server->allowed_classes,
                                         server->allowed_class_count)) {
        pthread_mutex_unlock(&server->registry_lock);

        /* Log rejection */
        usb_auth_log_event(client_ip, urb_header->device_id, device_class,
                           0, "device class blocked");
        server->auth_failures++;

        fprintf(stderr, "USB Server: Device class 0x%02x blocked for socket=%d\n",
                device_class, sender_fd);

        return usb_server_send_register_failure(sender_fd, urb_header->seqnum,
                                                 urb_header->device_id,
                                                 E_USB_CLASS_BLOCKED);
    }

    /* Find available slot */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (!server->clients[i].in_use && !server->clients[i].auth_pending) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pthread_mutex_unlock(&server->registry_lock);
        fprintf(stderr, "USB Server: Client registry full\n");
        return usb_server_send_register_failure(sender_fd, urb_header->seqnum,
                                                 urb_header->device_id,
                                                 E_OUT_OF_MEMORY);
    }

    /* Initialize slot */
    server->clients[slot].socket_fd = sender_fd;
    server->clients[slot].device_id = urb_header->device_id;
    server->clients[slot].device_class = device_class;
    server->clients[slot].authenticated = FALSE;
    server->clients[slot].auth_pending = FALSE;
    strncpy(server->clients[slot].client_ip, client_ip,
            sizeof(server->clients[slot].client_ip) - 1);

    /* Check if authentication is required */
    if (server->require_auth && server->auth_secret[0] != '\0') {
        /* Send auth challenge */
        int result = usb_server_send_auth_challenge(server, sender_fd, slot,
                                                     urb_header->seqnum);
        pthread_mutex_unlock(&server->registry_lock);
        return result;
    }

    /* No auth required, complete registration */
    pthread_mutex_unlock(&server->registry_lock);
    return usb_server_complete_registration(server, sender_fd, slot,
                                             urb_header->seqnum);
}

/**
 * @brief Handle authentication response from client
 */
static int usb_server_handle_auth_response(usb_server_t* server,
                                            const usb_urb_header_t* urb_header,
                                            const void* data,
                                            uint32_t data_len,
                                            int sender_fd)
{
    int i = 0;
    int slot = -1;
    usb_auth_payload_t auth_payload;
    int verify_result = 0;

    /* Validate payload */
    if (data == NULL || data_len < sizeof(usb_auth_payload_t)) {
        fprintf(stderr, "USB Server: Invalid auth response payload\n");
        return E_PROTOCOL_ERROR;
    }

    memcpy(&auth_payload, data, sizeof(auth_payload));

    pthread_mutex_lock(&server->registry_lock);

    /* Find client slot with pending auth */
    for (i = 0; i < USB_MAX_CLIENTS; i++) {
        if (server->clients[i].socket_fd == sender_fd &&
            server->clients[i].auth_pending) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pthread_mutex_unlock(&server->registry_lock);
        fprintf(stderr, "USB Server: Auth response from unregistered client\n");
        return E_INVALID_STATE;
    }

    /* Verify the response */
    verify_result = usb_auth_verify_response(
        server->auth_secret,
        server->clients[slot].pending_challenge,
        server->clients[slot].device_id,
        server->clients[slot].device_class,
        auth_payload.response
    );

    if (verify_result != 1) {
        /* Auth failed */
        server->clients[slot].auth_pending = FALSE;
        server->clients[slot].socket_fd = -1;
        server->auth_failures++;

        usb_auth_log_event(server->clients[slot].client_ip,
                           server->clients[slot].device_id,
                           server->clients[slot].device_class,
                           0, "invalid auth response");

        pthread_mutex_unlock(&server->registry_lock);

        fprintf(stderr, "USB Server: Auth verification failed for socket=%d\n",
                sender_fd);

        return usb_server_send_register_failure(sender_fd, urb_header->seqnum,
                                                 urb_header->device_id,
                                                 E_USB_AUTH_FAILED);
    }

    /* Clear challenge from memory */
    memset(server->clients[slot].pending_challenge, 0, USB_AUTH_CHALLENGE_SIZE);

    /* Complete registration */
    pthread_mutex_unlock(&server->registry_lock);
    return usb_server_complete_registration(server, sender_fd, slot,
                                             urb_header->seqnum);
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

    /* Send response back to client using wire format (LIB-001/NET-006 fix) */
    result = xoe_wire_send(sender_fd, &response);
    if (result != 0) {
        fprintf(stderr, "USB Server: Failed to send unregistration response: error %d\n",
                result);
        usb_protocol_free_payload(&response);  /* Free allocated payload */
        return E_NETWORK_ERROR;
    }
    (void)sent;  /* Suppress unused warning */

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

        case USB_RET_AUTH:
            return usb_server_handle_auth_response(server, &urb_header,
                                                    data_buffer, data_len,
                                                    sender_fd);

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
    printf("Auth failures:    %lu\n", server->auth_failures);
    printf("Auth required:    %s\n", server->require_auth ? "yes" : "no");
    printf("Class whitelist:  %d entries\n", server->allowed_class_count);
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

/* ========================================================================
 * Authentication Configuration Functions
 * ======================================================================== */

/**
 * @brief Configure authentication secret
 */
int usb_server_set_auth_secret(usb_server_t* server, const char* secret)
{
    if (server == NULL) {
        return E_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&server->registry_lock);

    if (secret == NULL || secret[0] == '\0') {
        /* Disable authentication */
        memset(server->auth_secret, 0, sizeof(server->auth_secret));
        server->require_auth = FALSE;
    } else {
        /* Set secret (truncate if too long) */
        strncpy(server->auth_secret, secret, USB_AUTH_SECRET_MAX - 1);
        server->auth_secret[USB_AUTH_SECRET_MAX - 1] = '\0';
        server->require_auth = TRUE;
    }

    pthread_mutex_unlock(&server->registry_lock);

    printf("USB Server: Authentication %s\n",
           server->require_auth ? "enabled" : "disabled");

    return 0;
}

/**
 * @brief Configure device class whitelist
 */
int usb_server_set_class_whitelist(usb_server_t* server,
                                   const uint8_t* classes,
                                   int count)
{
    int i = 0;

    if (server == NULL) {
        return E_INVALID_ARGUMENT;
    }

    if (count < 0 || count > 16) {
        return E_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&server->registry_lock);

    if (classes == NULL || count == 0) {
        /* Clear whitelist (default policy applies: block HID only) */
        memset(server->allowed_classes, 0, sizeof(server->allowed_classes));
        server->allowed_class_count = 0;
    } else {
        /* Set whitelist */
        for (i = 0; i < count && i < 16; i++) {
            server->allowed_classes[i] = classes[i];
        }
        server->allowed_class_count = count;
    }

    pthread_mutex_unlock(&server->registry_lock);

    printf("USB Server: Device class whitelist set (%d entries)\n", count);

    return 0;
}

/**
 * @brief Enable or disable authentication requirement
 */
void usb_server_set_require_auth(usb_server_t* server, int require)
{
    if (server == NULL) {
        return;
    }

    pthread_mutex_lock(&server->registry_lock);
    server->require_auth = require ? TRUE : FALSE;
    pthread_mutex_unlock(&server->registry_lock);

    printf("USB Server: Authentication requirement %s\n",
           require ? "enabled" : "disabled");
}

/**
 * @brief Get client IP address from socket
 */
void usb_server_get_client_ip(int socket_fd, char* ip_out, size_t ip_out_len)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (ip_out == NULL || ip_out_len == 0) {
        return;
    }

    ip_out[0] = '\0';

    if (getpeername(socket_fd, (struct sockaddr*)&addr, &addr_len) != 0) {
        strncpy(ip_out, "(unknown)", ip_out_len - 1);
        ip_out[ip_out_len - 1] = '\0';
        return;
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&addr;
        inet_ntop(AF_INET, &s->sin_addr, ip_out, ip_out_len);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
        inet_ntop(AF_INET6, &s->sin6_addr, ip_out, ip_out_len);
    } else {
        strncpy(ip_out, "(unknown)", ip_out_len - 1);
        ip_out[ip_out_len - 1] = '\0';
    }
}
