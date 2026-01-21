/**
 * @file nbd_session.c
 * @brief NBD session management implementation
 *
 * Implements NBD Fixed Newstyle handshake and request handling.
 *
 * [LLM-ASSISTED]
 */

#include "connectors/nbd/nbd_session.h"
#include "connectors/nbd/nbd_protocol.h"
#include "lib/common/definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

/* NBD handshake constants */
#define NBD_OPT_EXPORT_NAME 1
#define NBD_REP_ACK         1
#define NBD_REP_SERVER      2
#define NBD_REP_ERR_UNSUP   0x80000001

/**
 * nbd_htonll - Convert host uint64_t to network byte order (big-endian)
 * @host_value: Value in host byte order
 *
 * Returns: Value in network byte order (big-endian)
 *
 * Architecture-neutral implementation that works on both little-endian
 * and big-endian systems by explicitly constructing big-endian byte order.
 */
static uint64_t nbd_htonll(uint64_t host_value) {
    uint64_t result = 0;
    unsigned char *dest = (unsigned char*)&result;

    /* Build big-endian representation byte-by-byte */
    dest[0] = (unsigned char)((host_value >> 56) & 0xFF);
    dest[1] = (unsigned char)((host_value >> 48) & 0xFF);
    dest[2] = (unsigned char)((host_value >> 40) & 0xFF);
    dest[3] = (unsigned char)((host_value >> 32) & 0xFF);
    dest[4] = (unsigned char)((host_value >> 24) & 0xFF);
    dest[5] = (unsigned char)((host_value >> 16) & 0xFF);
    dest[6] = (unsigned char)((host_value >> 8) & 0xFF);
    dest[7] = (unsigned char)(host_value & 0xFF);

    return result;
}

/**
 * nbd_ntohll - Convert network uint64_t to host byte order
 * @net_value: Value in network byte order (big-endian)
 *
 * Returns: Value in host byte order
 *
 * Architecture-neutral implementation that works on both little-endian
 * and big-endian systems by explicitly parsing big-endian byte order.
 */
static uint64_t nbd_ntohll(uint64_t net_value) {
    unsigned char *src = (unsigned char*)&net_value;

    /* Parse big-endian representation byte-by-byte */
    return ((uint64_t)src[0] << 56) |
           ((uint64_t)src[1] << 48) |
           ((uint64_t)src[2] << 40) |
           ((uint64_t)src[3] << 32) |
           ((uint64_t)src[4] << 24) |
           ((uint64_t)src[5] << 16) |
           ((uint64_t)src[6] << 8) |
           ((uint64_t)src[7]);
}

/**
 * nbd_session_init - Initialize NBD session
 */
nbd_session_t* nbd_session_init(nbd_config_t* config,
                                nbd_backend_t* backend,
                                int client_socket)
{
    nbd_session_t* session = NULL;

    if (config == NULL || backend == NULL || client_socket < 0) {
        return NULL;
    }

    session = (nbd_session_t*)malloc(sizeof(nbd_session_t));
    if (session == NULL) {
        return NULL;
    }

    memset(session, 0, sizeof(nbd_session_t));

    session->client_socket = client_socket;
    session->backend = backend;
    session->config = config;
    session->handshake_complete = FALSE;
    session->export_size = nbd_backend_get_size(backend);

    /* Copy export name */
    strncpy(session->export_name, config->export_name, NBD_NAME_MAX - 1);
    session->export_name[NBD_NAME_MAX - 1] = '\0';

    /* Build transmission flags */
    session->transmission_flags = NBD_FLAG_HAS_FLAGS;
    if (nbd_backend_is_read_only(backend)) {
        session->transmission_flags |= NBD_FLAG_READ_ONLY;
    }
    if (config->capabilities & NBD_CAP_FLUSH) {
        session->transmission_flags |= NBD_FLAG_SEND_FLUSH;
    }
    if (config->capabilities & NBD_CAP_TRIM) {
        session->transmission_flags |= NBD_FLAG_SEND_TRIM;
    }

    return session;
}

/**
 * nbd_session_handshake - Perform NBD Fixed Newstyle handshake
 */
int nbd_session_handshake(nbd_session_t* session) {
    uint64_t magic_init = 0;
    uint64_t magic_opts = 0;
    uint16_t server_flags = 0;
    uint32_t client_flags = 0;
    uint32_t option = 0;
    uint32_t option_len = 0;
    char export_name[256] = {0};
    uint64_t export_size_net = 0;
    uint16_t transmission_flags_net = 0;
    ssize_t bytes = 0;

    if (session == NULL) {
        return E_NULL_POINTER;
    }

    /* Step 1: Send NBDMAGIC + IHAVEOPT */
    magic_init = nbd_htonll(NBD_MAGIC_INIT);
    magic_opts = nbd_htonll(NBD_MAGIC_OPTS);
    server_flags = htons(NBD_FLAG_HAS_FLAGS);

    bytes = write(session->client_socket, &magic_init, sizeof(magic_init));
    if (bytes != sizeof(magic_init)) {
        fprintf(stderr, "NBD handshake: failed to send NBDMAGIC\n");
        return E_IO_ERROR;
    }

    bytes = write(session->client_socket, &magic_opts, sizeof(magic_opts));
    if (bytes != sizeof(magic_opts)) {
        fprintf(stderr, "NBD handshake: failed to send IHAVEOPT\n");
        return E_IO_ERROR;
    }

    bytes = write(session->client_socket, &server_flags, sizeof(server_flags));
    if (bytes != sizeof(server_flags)) {
        fprintf(stderr, "NBD handshake: failed to send flags\n");
        return E_IO_ERROR;
    }

    /* Step 2: Receive client flags */
    bytes = read(session->client_socket, &client_flags, sizeof(client_flags));
    if (bytes != sizeof(client_flags)) {
        fprintf(stderr, "NBD handshake: failed to receive client flags\n");
        return E_IO_ERROR;
    }

    /* Step 3: Option negotiation (simplified: only NBD_OPT_EXPORT_NAME) */
    bytes = read(session->client_socket, &magic_opts, sizeof(magic_opts));
    if (bytes != sizeof(magic_opts)) {
        fprintf(stderr, "NBD handshake: failed to receive option magic\n");
        return E_IO_ERROR;
    }

    if (nbd_ntohll(magic_opts) != NBD_MAGIC_OPTS) {
        fprintf(stderr, "NBD handshake: invalid option magic\n");
        return E_NBD_INVALID_MAGIC;
    }

    bytes = read(session->client_socket, &option, sizeof(option));
    if (bytes != sizeof(option)) {
        fprintf(stderr, "NBD handshake: failed to receive option\n");
        return E_IO_ERROR;
    }

    option = ntohl(option);
    if (option != NBD_OPT_EXPORT_NAME) {
        fprintf(stderr, "NBD handshake: unsupported option %u\n", option);
        return E_NOT_SUPPORTED;
    }

    bytes = read(session->client_socket, &option_len, sizeof(option_len));
    if (bytes != sizeof(option_len)) {
        fprintf(stderr, "NBD handshake: failed to receive option length\n");
        return E_IO_ERROR;
    }

    option_len = ntohl(option_len);
    if (option_len > sizeof(export_name) - 1) {
        fprintf(stderr, "NBD handshake: export name too long (%u bytes)\n",
                option_len);
        return E_INVALID_ARGUMENT;
    }

    if (option_len > 0) {
        bytes = read(session->client_socket, export_name, option_len);
        if (bytes != (ssize_t)option_len) {
            fprintf(stderr, "NBD handshake: failed to receive export name\n");
            return E_IO_ERROR;
        }
        export_name[option_len] = '\0';
    } else {
        export_name[0] = '\0';
    }

    /* Step 4: Send export info */
    export_size_net = nbd_htonll(session->export_size);
    transmission_flags_net = htons(session->transmission_flags);

    bytes = write(session->client_socket, &export_size_net,
                  sizeof(export_size_net));
    if (bytes != sizeof(export_size_net)) {
        fprintf(stderr, "NBD handshake: failed to send export size\n");
        return E_IO_ERROR;
    }

    bytes = write(session->client_socket, &transmission_flags_net,
                  sizeof(transmission_flags_net));
    if (bytes != sizeof(transmission_flags_net)) {
        fprintf(stderr, "NBD handshake: failed to send transmission flags\n");
        return E_IO_ERROR;
    }

    /* Send 124 bytes of zeros (reserved) */
    {
        char zeros[124];
        memset(zeros, 0, sizeof(zeros));
        bytes = write(session->client_socket, zeros, sizeof(zeros));
        if (bytes != sizeof(zeros)) {
            fprintf(stderr, "NBD handshake: failed to send padding\n");
            return E_IO_ERROR;
        }
    }

    session->handshake_complete = TRUE;
    printf("NBD handshake complete: export '%s', size %llu bytes, flags 0x%x\n",
           export_name[0] ? export_name : session->export_name,
           (unsigned long long)session->export_size,
           session->transmission_flags);

    return SUCCESS;
}

/**
 * nbd_session_handle_request - Handle single NBD request
 */
int nbd_session_handle_request(nbd_session_t* session) {
    nbd_request_t request;
    nbd_simple_reply_t reply;
    unsigned char* data_buf = NULL;
    ssize_t bytes;
    int result;

    if (session == NULL) {
        return E_NULL_POINTER;
    }

    if (!session->handshake_complete) {
        fprintf(stderr, "NBD session: request before handshake complete\n");
        return E_INVALID_STATE;
    }

    /* Read request header */
    bytes = read(session->client_socket, &request, sizeof(request));
    if (bytes == 0) {
        return E_IO_ERROR;  /* Connection closed */
    }
    if (bytes != sizeof(request)) {
        fprintf(stderr, "NBD session: failed to read request header\n");
        return E_IO_ERROR;
    }

    /* Convert from network byte order */
    request.magic = ntohl(request.magic);
    request.flags = ntohs(request.flags);
    request.type = ntohs(request.type);
    request.cookie = nbd_ntohll(request.cookie);
    request.offset = nbd_ntohll(request.offset);
    request.length = ntohl(request.length);

    /* Validate request magic */
    if (request.magic != NBD_MAGIC_REQUEST) {
        fprintf(stderr, "NBD session: invalid request magic 0x%x\n",
                request.magic);
        return E_NBD_INVALID_MAGIC;
    }

    /* Initialize reply header */
    reply.magic = htonl(NBD_MAGIC_SIMPLE_REPLY);
    reply.error = htonl(NBD_OK);
    reply.cookie = nbd_htonll(request.cookie);

    /* Handle command */
    switch (request.type) {
        case NBD_CMD_READ:
            /* Allocate data buffer */
            data_buf = (unsigned char*)malloc(request.length);
            if (data_buf == NULL) {
                reply.error = htonl(NBD_ENOMEM);
                write(session->client_socket, &reply, sizeof(reply));
                return E_OUT_OF_MEMORY;
            }

            /* Read from backend */
            result = nbd_backend_read(session->backend, data_buf,
                                     request.offset, request.length);
            if (result != SUCCESS) {
                reply.error = htonl(NBD_EIO);
                write(session->client_socket, &reply, sizeof(reply));
                free(data_buf);
                return result;
            }

            /* Send reply + data */
            bytes = write(session->client_socket, &reply, sizeof(reply));
            if (bytes != sizeof(reply)) {
                fprintf(stderr, "NBD session: failed to send reply\n");
                free(data_buf);
                return E_IO_ERROR;
            }
            /* Send data payload in loop for large transfers */
            {
                size_t total_written = 0;
                while (total_written < request.length) {
                    bytes = write(session->client_socket,
                                 data_buf + total_written,
                                 request.length - total_written);
                    if (bytes <= 0) {
                        fprintf(stderr, "NBD session: failed to send data\n");
                        free(data_buf);
                        return E_IO_ERROR;
                    }
                    total_written += bytes;
                }
            }
            free(data_buf);
            break;

        case NBD_CMD_WRITE:
            /* Allocate data buffer */
            data_buf = (unsigned char*)malloc(request.length);
            if (data_buf == NULL) {
                reply.error = htonl(NBD_ENOMEM);
                write(session->client_socket, &reply, sizeof(reply));
                return E_OUT_OF_MEMORY;
            }

            /* Read data from client */
            {
                size_t total_read = 0;
                while (total_read < request.length) {
                    bytes = read(session->client_socket,
                                data_buf + total_read,
                                request.length - total_read);
                    if (bytes <= 0) {
                        fprintf(stderr, "NBD session: failed to read write data\n");
                        free(data_buf);
                        return E_IO_ERROR;
                    }
                    total_read += bytes;
                }
            }

            /* Write to backend */
            result = nbd_backend_write(session->backend, data_buf,
                                      request.offset, request.length);
            if (result != SUCCESS) {
                reply.error = htonl(NBD_EIO);
                write(session->client_socket, &reply, sizeof(reply));
                free(data_buf);
                return result;
            }

            /* Send reply */
            bytes = write(session->client_socket, &reply, sizeof(reply));
            if (bytes != sizeof(reply)) {
                fprintf(stderr, "NBD session: failed to send write reply\n");
                free(data_buf);
                return E_IO_ERROR;
            }
            free(data_buf);
            break;

        case NBD_CMD_FLUSH:
            /* Flush backend */
            result = nbd_backend_flush(session->backend);
            if (result != SUCCESS) {
                reply.error = htonl(NBD_EIO);
            }
            bytes = write(session->client_socket, &reply, sizeof(reply));
            if (bytes != sizeof(reply)) {
                fprintf(stderr, "NBD session: failed to send flush reply\n");
                return E_IO_ERROR;
            }
            break;

        case NBD_CMD_DISC:
            /* Disconnect requested */
            return E_IO_ERROR;

        case NBD_CMD_TRIM:
            /* TRIM backend */
            result = nbd_backend_trim(session->backend, request.offset,
                                     request.length);
            if (result != SUCCESS) {
                reply.error = htonl(NBD_EIO);
            }
            bytes = write(session->client_socket, &reply, sizeof(reply));
            if (bytes != sizeof(reply)) {
                fprintf(stderr, "NBD session: failed to send trim reply\n");
                return E_IO_ERROR;
            }
            break;

        default:
            fprintf(stderr, "NBD session: unsupported command %u\n",
                    request.type);
            reply.error = htonl(NBD_EINVAL);
            bytes = write(session->client_socket, &reply, sizeof(reply));
            if (bytes != sizeof(reply)) {
                fprintf(stderr, "NBD session: failed to send error reply\n");
            }
            return E_NOT_SUPPORTED;
    }

    return SUCCESS;
}

/**
 * nbd_session_cleanup - Clean up NBD session
 */
void nbd_session_cleanup(nbd_session_t** session) {
    if (session == NULL || *session == NULL) {
        return;
    }

    free(*session);
    *session = NULL;
}
