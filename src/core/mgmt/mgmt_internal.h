#ifndef CORE_MGMT_INTERNAL_H
#define CORE_MGMT_INTERNAL_H

#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>

#if TLS_ENABLED
#include <openssl/ssl.h>
#endif

/**
 * Internal structures shared between management server and command handlers.
 * Not exposed to external code.
 */

#define MGMT_BUFFER_SIZE 1024    /* Per-session I/O buffer */
#define MGMT_PASSWORD_MAX 128    /* Max password length */

/* Rate limiting constants (NET-004, FSM-009 fix) */
#define MGMT_RATE_LIMIT_ENTRIES 16   /* Max tracked IPs */
#define MGMT_RATE_LIMIT_LOCKOUT 30   /* Seconds to lock out after failures */
#define MGMT_RATE_LIMIT_FAILURES 5   /* Failures before lockout */

/* Rate limit entry for tracking auth failures per IP */
typedef struct {
    in_addr_t ip_addr;      /* IPv4 address (0 = unused) */
    int failure_count;      /* Number of failed auth attempts */
    time_t lockout_until;   /* Lockout expiry time (0 = not locked) */
} mgmt_rate_limit_entry_t;

/* Forward declaration */
struct mgmt_server_t;

/* Session structure - shared between server and commands */
typedef struct mgmt_session_t {
    int socket_fd;              /* Session socket */
    int in_use;                 /* Pool slot in-use flag */
    pthread_t thread_id;        /* Session thread ID */
    char password[MGMT_PASSWORD_MAX]; /* Password buffer */
    volatile sig_atomic_t authenticated; /* Authentication status */
    char read_buffer[MGMT_BUFFER_SIZE];  /* Pre-allocated read buffer */
    char write_buffer[MGMT_BUFFER_SIZE]; /* Pre-allocated write buffer */
    in_addr_t client_ip;        /* Client IP for rate limiting */
    struct mgmt_server_t *server; /* Back-pointer to server for rate limiting */
#if TLS_ENABLED
    SSL* tls;                   /* TLS session (FSM-006 fix) */
#endif
} mgmt_session_t;

/**
 * TLS-aware I/O helpers (FSM-006 fix)
 * Uses TLS if enabled, falls back to plain socket I/O.
 */
ssize_t mgmt_write(mgmt_session_t *session, const void *buf, size_t len);
ssize_t mgmt_read(mgmt_session_t *session, void *buf, size_t len);

#endif /* CORE_MGMT_INTERNAL_H */
