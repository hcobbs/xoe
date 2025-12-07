/**
 * net_resolve.c
 *
 * Implementation of hostname resolution utilities.
 * Uses POSIX getaddrinfo() for thread-safe resolution.
 *
 * [LLM-ARCH]
 */

#include "net_resolve.h"
#include "lib/common/definitions.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

/**
 * init_result - Initialize result structure to success state
 * @result: Result structure to initialize (may be NULL)
 */
static void init_result(net_resolve_result_t *result) {
    if (result != NULL) {
        result->error_code = 0;
        result->gai_error = 0;
        result->sys_errno = 0;
    }
}

/**
 * set_result - Set error details in result structure
 * @result:     Result structure to update (may be NULL)
 * @error_code: XOE error code
 * @gai_error:  getaddrinfo error code (0 if not applicable)
 * @sys_errno:  errno value (0 if not applicable)
 */
static void set_result(net_resolve_result_t *result, int error_code,
                       int gai_error, int sys_errno) {
    if (result != NULL) {
        result->error_code = error_code;
        result->gai_error = gai_error;
        result->sys_errno = sys_errno;
    }
}

int net_resolve_to_sockaddr(const char *host, int port,
                            struct sockaddr_in *addr,
                            net_resolve_result_t *result) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct sockaddr_in *resolved;
    int gai_ret;

    init_result(result);

    /* Validate parameters */
    if (host == NULL || addr == NULL) {
        set_result(result, E_INVALID_ARGUMENT, 0, 0);
        return E_INVALID_ARGUMENT;
    }

    if (port < 0 || port > 65535) {
        set_result(result, E_INVALID_ARGUMENT, 0, 0);
        return E_INVALID_ARGUMENT;
    }

    /* Initialize output structure */
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((unsigned short)port);

    /* Fast path: try numeric address first */
    if (inet_pton(AF_INET, host, &addr->sin_addr) == 1) {
        return 0;
    }

    /* Slow path: DNS resolution via getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_flags = 0;

    gai_ret = getaddrinfo(host, NULL, &hints, &res);
    if (gai_ret != 0) {
        set_result(result, E_DNS_ERROR, gai_ret, 0);
        return E_DNS_ERROR;
    }

    if (res == NULL) {
        set_result(result, E_DNS_ERROR, 0, 0);
        return E_DNS_ERROR;
    }

    /* Copy first result's address */
    resolved = (struct sockaddr_in *)res->ai_addr;
    addr->sin_addr = resolved->sin_addr;

    freeaddrinfo(res);
    return 0;
}

int net_resolve_connect(const char *host, int port, int *sock_out,
                        net_resolve_result_t *result) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    int gai_ret;
    int sock = -1;
    int last_errno = 0;
    struct sockaddr_in numeric_addr;

    init_result(result);

    /* Validate parameters */
    if (host == NULL || sock_out == NULL) {
        set_result(result, E_INVALID_ARGUMENT, 0, 0);
        return E_INVALID_ARGUMENT;
    }

    if (port <= 0 || port > 65535) {
        set_result(result, E_INVALID_ARGUMENT, 0, 0);
        return E_INVALID_ARGUMENT;
    }

    *sock_out = -1;

    /* Fast path: try numeric address first */
    memset(&numeric_addr, 0, sizeof(numeric_addr));
    numeric_addr.sin_family = AF_INET;
    numeric_addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, host, &numeric_addr.sin_addr) == 1) {
        /* It's a numeric IP, try direct connect */
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            set_result(result, E_NETWORK_ERROR, 0, errno);
            return E_NETWORK_ERROR;
        }

        if (connect(sock, (struct sockaddr *)&numeric_addr,
                    sizeof(numeric_addr)) == 0) {
            *sock_out = sock;
            return 0;
        }

        /* Connect failed */
        last_errno = errno;
        close(sock);
        set_result(result, E_NETWORK_ERROR, 0, last_errno);
        return E_NETWORK_ERROR;
    }

    /* Slow path: DNS resolution via getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM; /* TCP */
    hints.ai_flags = 0;

    gai_ret = getaddrinfo(host, NULL, &hints, &res);
    if (gai_ret != 0) {
        set_result(result, E_DNS_ERROR, gai_ret, 0);
        return E_DNS_ERROR;
    }

    if (res == NULL) {
        set_result(result, E_DNS_ERROR, 0, 0);
        return E_DNS_ERROR;
    }

    /* Try each address in order until one succeeds */
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in *addr_in = (struct sockaddr_in *)rp->ai_addr;

        /* Set the port (getaddrinfo doesn't set it when service is NULL) */
        addr_in->sin_port = htons((unsigned short)port);

        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            last_errno = errno;
            continue;
        }

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            /* Success */
            *sock_out = sock;
            freeaddrinfo(res);
            return 0;
        }

        /* Connect failed, try next address */
        last_errno = errno;
        close(sock);
        sock = -1;
    }

    /* All addresses failed */
    freeaddrinfo(res);
    set_result(result, E_NETWORK_ERROR, 0, last_errno);
    return E_NETWORK_ERROR;
}

void net_resolve_format_error(const net_resolve_result_t *result,
                              char *buf, size_t buflen) {
    if (result == NULL || buf == NULL || buflen == 0) {
        return;
    }

    buf[0] = '\0';

    if (result->error_code == 0) {
        snprintf(buf, buflen, "Success");
        return;
    }

    if (result->error_code == E_INVALID_ARGUMENT) {
        snprintf(buf, buflen, "Invalid argument");
        return;
    }

    if (result->error_code == E_DNS_ERROR) {
        if (result->gai_error != 0) {
            snprintf(buf, buflen, "DNS resolution failed: %s",
                     gai_strerror(result->gai_error));
        } else {
            snprintf(buf, buflen, "DNS resolution failed");
        }
        return;
    }

    if (result->error_code == E_NETWORK_ERROR) {
        if (result->sys_errno != 0) {
            snprintf(buf, buflen, "Connection failed: %s",
                     strerror(result->sys_errno));
        } else {
            snprintf(buf, buflen, "Connection failed");
        }
        return;
    }

    snprintf(buf, buflen, "Unknown error (%d)", result->error_code);
}
