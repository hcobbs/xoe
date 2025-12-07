/**
 * net_resolve.h
 *
 * Network address resolution utilities for XOE.
 * Provides thread-safe hostname-to-address resolution with failover support.
 *
 * [LLM-ARCH]
 */

#ifndef NET_RESOLVE_H
#define NET_RESOLVE_H

#include <netinet/in.h>
#include <stddef.h>

/**
 * Thread-safe result structure for resolution operations.
 * Contains both success data and error details.
 * All fields are populated by the resolution functions.
 */
typedef struct {
    int error_code;     /* 0 on success, negative XOE error code on failure */
    int gai_error;      /* getaddrinfo() error code (use gai_strerror) */
    int sys_errno;      /* errno value if system call failed */
} net_resolve_result_t;

/**
 * net_resolve_connect - Resolve hostname/IP and connect with failover
 * @host:     Hostname or dotted-decimal IP address
 * @port:     Port number (host byte order, 1-65535)
 * @sock_out: Output: connected socket fd on success, -1 on failure
 * @result:   Output: detailed error information (may be NULL)
 *
 * Resolves the given hostname or IP address and attempts to connect.
 * When DNS returns multiple A records, tries each address in order
 * until one succeeds or all fail.
 *
 * Thread-safe: all state is returned via parameters, no globals used.
 *
 * Returns: 0 on success, negative error code on failure
 *          E_INVALID_ARGUMENT - NULL host or sock_out, invalid port
 *          E_DNS_ERROR - hostname resolution failed
 *          E_NETWORK_ERROR - all connection attempts failed
 */
int net_resolve_connect(const char *host, int port, int *sock_out,
                        net_resolve_result_t *result);

/**
 * net_resolve_to_sockaddr - Resolve hostname/IP to sockaddr_in
 * @host:   Hostname or dotted-decimal IP address
 * @port:   Port number (host byte order, 0-65535; 0 allowed for bind)
 * @addr:   Output: populated sockaddr_in structure
 * @result: Output: detailed error information (may be NULL)
 *
 * Resolves the given hostname or IP address to a sockaddr_in structure
 * suitable for bind(). Uses the first result when DNS returns multiple
 * A records.
 *
 * Thread-safe: all state is returned via parameters, no globals used.
 *
 * Returns: 0 on success, negative error code on failure
 *          E_INVALID_ARGUMENT - NULL host or addr
 *          E_DNS_ERROR - hostname resolution failed
 */
int net_resolve_to_sockaddr(const char *host, int port,
                            struct sockaddr_in *addr,
                            net_resolve_result_t *result);

/**
 * net_resolve_format_error - Format error message from result structure
 * @result: Result structure from a resolve operation
 * @buf:    Output buffer for error message
 * @buflen: Size of output buffer
 *
 * Formats a human-readable error message based on the error details
 * in the result structure. Thread-safe: writes to caller-provided buffer.
 *
 * If result is NULL or buflen is 0, no action is taken.
 */
void net_resolve_format_error(const net_resolve_result_t *result,
                              char *buf, size_t buflen);

#endif /* NET_RESOLVE_H */
