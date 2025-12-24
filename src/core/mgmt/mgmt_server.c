#include "mgmt_server.h"
#include "mgmt_internal.h"
#include "mgmt_commands.h"
#include "core/config.h"
#include "lib/security/password_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if TLS_ENABLED
#include "lib/security/tls_config.h"
#include "lib/security/tls_context.h"
#include "lib/security/tls_session.h"
#include "lib/security/tls_io.h"
#endif

/**
 * Management Server Implementation
 *
 * MEMORY ISOLATION:
 * All management operations use pre-allocated, fixed-size buffers to ensure
 * complete isolation from main server memory. No dynamic allocation occurs
 * during session handling - all structures are allocated at startup.
 *
 * This design prevents:
 * - Management memory leaks affecting main server
 * - Buffer overflows in CLI corrupting server state
 * - Memory exhaustion from management sessions
 *
 * Thread architecture:
 * - Main thread: Calls mgmt_server_start/stop
 * - Listener thread: Accepts connections, spawns session threads
 * - Session threads: Handle individual management sessions (detached)
 */

/* Management server structure - FIXED SIZE, ISOLATED MEMORY */
struct mgmt_server_t {
    int listen_fd;              /* Listening socket */
    int port;                   /* Listen port */
    char password_hash[PASSWORD_HEX_LEN]; /* Hashed password (never plaintext) */
    int password_enabled;       /* Authentication required flag */
    pthread_t listener_thread;  /* Listener thread ID */
    volatile sig_atomic_t shutdown_flag; /* Shutdown signal */
    mgmt_session_t sessions[MAX_MGMT_SESSIONS]; /* Session pool (pre-allocated) */
    pthread_mutex_t session_mutex; /* Protects session pool */
    /* Rate limiting (NET-004, FSM-009 fix) */
    mgmt_rate_limit_entry_t rate_limits[MGMT_RATE_LIMIT_ENTRIES];
    pthread_mutex_t rate_limit_mutex; /* Protects rate limit table */
#if TLS_ENABLED
    /* TLS support for management interface (FSM-006 fix) */
    SSL_CTX* tls_ctx;           /* TLS context for management connections */
    int tls_enabled;            /* TLS enabled flag */
#endif
};

/* Forward declarations */
static void* listener_thread(void* arg);
static void* session_handler(void* arg);
static mgmt_session_t* acquire_session_slot(mgmt_server_t *server);
static void release_session_slot(mgmt_session_t *session);
static int authenticate_session(mgmt_session_t *session);
static int check_rate_limit(mgmt_server_t *server, in_addr_t ip);
static void record_auth_failure(mgmt_server_t *server, in_addr_t ip);
static void clear_auth_failure(mgmt_server_t *server, in_addr_t ip);

/**
 * secure_zero - Securely clear sensitive memory (NET-015 fix)
 * Uses volatile pointer to prevent compiler optimization.
 */
static void secure_zero(void* ptr, size_t len) {
    volatile unsigned char* p = (volatile unsigned char*)ptr;
    while (len--) {
        *p++ = 0;
    }
}

/**
 * mgmt_write - TLS-aware write helper (FSM-006 fix)
 * Uses TLS if enabled, falls back to plain socket write.
 */
ssize_t mgmt_write(mgmt_session_t *session, const void *buf, size_t len) {
#if TLS_ENABLED
    if (session->tls != NULL) {
        return tls_write(session->tls, buf, len);
    }
#endif
    return write(session->socket_fd, buf, len);
}

/**
 * mgmt_read - TLS-aware read helper (FSM-006 fix)
 * Uses TLS if enabled, falls back to plain socket read.
 */
ssize_t mgmt_read(mgmt_session_t *session, void *buf, size_t len) {
#if TLS_ENABLED
    if (session->tls != NULL) {
        return tls_read(session->tls, buf, len);
    }
#endif
    return read(session->socket_fd, buf, len);
}

/**
 * mgmt_server_start - Start management server
 */
mgmt_server_t* mgmt_server_start(xoe_config_t *config) {
    mgmt_server_t *server;
    struct sockaddr_in server_addr;
    int opt = 1;
    int i;

    if (config == NULL) {
        return NULL;
    }

    /* Allocate server structure */
    server = (mgmt_server_t*)malloc(sizeof(mgmt_server_t));
    if (server == NULL) {
        fprintf(stderr, "Failed to allocate management server\n");
        return NULL;
    }

    /* Initialize fields */
    server->listen_fd = -1;
    server->port = config->mgmt_port;
    server->password_hash[0] = '\0';
    server->password_enabled = 0;
    server->shutdown_flag = 0;

    /* Hash password if provided (never store plaintext) */
    if (config->mgmt_password != NULL && config->mgmt_password[0] != '\0') {
        if (password_hash(config->mgmt_password, server->password_hash) != 0) {
            fprintf(stderr, "Failed to hash management password\n");
            free(server);
            return NULL;
        }
        server->password_enabled = 1;
    }

    /* Initialize session pool (pre-allocated, zero dynamic allocation) */
    /* USB-009 fix: check pthread_mutex_init return value */
    if (pthread_mutex_init(&server->session_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize session mutex\n");
        free(server);
        return NULL;
    }
    for (i = 0; i < MAX_MGMT_SESSIONS; i++) {
        server->sessions[i].in_use = 0;
        server->sessions[i].socket_fd = -1;
        server->sessions[i].authenticated = 0;
        server->sessions[i].client_ip = 0;
        server->sessions[i].server = server;  /* Back-pointer for rate limiting */
        /* Copy hashed password to each session's isolated buffer */
        strncpy(server->sessions[i].password, server->password_hash, MGMT_PASSWORD_MAX - 1);
        server->sessions[i].password[MGMT_PASSWORD_MAX - 1] = '\0';
        /* Buffers are already allocated as part of the structure */
    }

    /* Initialize rate limiting (NET-004, FSM-009 fix) */
    /* USB-009 fix: check pthread_mutex_init return value */
    if (pthread_mutex_init(&server->rate_limit_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize rate limit mutex\n");
        pthread_mutex_destroy(&server->session_mutex);
        free(server);
        return NULL;
    }
    for (i = 0; i < MGMT_RATE_LIMIT_ENTRIES; i++) {
        server->rate_limits[i].ip_addr = 0;
        server->rate_limits[i].failure_count = 0;
        server->rate_limits[i].lockout_until = 0;
    }

#if TLS_ENABLED
    /* Initialize TLS context for management connections (FSM-006 fix) */
    server->tls_enabled = 0;
    server->tls_ctx = NULL;

    /* Only enable TLS if encryption is configured and cert/key exist */
    if (config->encryption_mode == ENCRYPT_TLS12 ||
        config->encryption_mode == ENCRYPT_TLS13) {
        if (config->cert_path[0] != '\0' && config->key_path[0] != '\0') {
            server->tls_ctx = tls_context_init(config->cert_path,
                                               config->key_path,
                                               config->encryption_mode);
            if (server->tls_ctx == NULL) {
                fprintf(stderr, "Warning: Failed to init TLS for management\n");
                fprintf(stderr, "Management will use unencrypted connections\n");
            } else {
                server->tls_enabled = 1;
            }
        }
    }
#endif

    /* Create listening socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        fprintf(stderr, "Failed to create management socket: %s\n", strerror(errno));
        pthread_mutex_destroy(&server->rate_limit_mutex);
        pthread_mutex_destroy(&server->session_mutex);
        free(server);
        return NULL;
    }

    /* Set socket options */
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "Warning: Failed to set SO_REUSEADDR: %s\n", strerror(errno));
    }

    /* Bind to port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 only */
    server_addr.sin_port = htons(server->port);

    if (bind(server->listen_fd, (struct sockaddr*)&server_addr,
             sizeof(server_addr)) < 0) {
        fprintf(stderr, "Failed to bind management port %d: %s\n",
                server->port, strerror(errno));
        close(server->listen_fd);
        pthread_mutex_destroy(&server->rate_limit_mutex);
        pthread_mutex_destroy(&server->session_mutex);
        free(server);
        return NULL;
    }

    /* Start listening */
    if (listen(server->listen_fd, MAX_PENDING_CONNECTIONS) < 0) {
        fprintf(stderr, "Failed to listen on management port: %s\n", strerror(errno));
        close(server->listen_fd);
        pthread_mutex_destroy(&server->rate_limit_mutex);
        pthread_mutex_destroy(&server->session_mutex);
        free(server);
        return NULL;
    }

    /* Spawn listener thread */
    if (pthread_create(&server->listener_thread, NULL, listener_thread, server) != 0) {
        fprintf(stderr, "Failed to create management listener thread: %s\n",
                strerror(errno));
        close(server->listen_fd);
        pthread_mutex_destroy(&server->rate_limit_mutex);
        pthread_mutex_destroy(&server->session_mutex);
        free(server);
        return NULL;
    }

    printf("Management interface started on 127.0.0.1:%d\n", server->port);
#if TLS_ENABLED
    printf("Encryption: %s\n", server->tls_enabled ? "TLS enabled" : "disabled");
#else
    printf("Encryption: disabled (TLS not compiled)\n");
#endif
    printf("Authentication: enabled\n");

    return server;
}

/**
 * mgmt_server_stop - Stop management server
 */
void mgmt_server_stop(mgmt_server_t *server) {
    int i;

    if (server == NULL) {
        return;
    }

    printf("Shutting down management interface...\n");

    /* Signal shutdown */
    server->shutdown_flag = 1;

    /* Close listening socket (unblocks accept) */
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    /* Wait for listener thread */
    pthread_join(server->listener_thread, NULL);

    /* Close all active sessions */
    pthread_mutex_lock(&server->session_mutex);
    for (i = 0; i < MAX_MGMT_SESSIONS; i++) {
        if (server->sessions[i].in_use && server->sessions[i].socket_fd >= 0) {
#if TLS_ENABLED
            if (server->sessions[i].tls != NULL) {
                tls_session_shutdown(server->sessions[i].tls);
                tls_session_destroy(server->sessions[i].tls);
                server->sessions[i].tls = NULL;
            }
#endif
            close(server->sessions[i].socket_fd);
            server->sessions[i].socket_fd = -1;
        }
    }
    pthread_mutex_unlock(&server->session_mutex);

    /* Wait briefly for session threads to exit (they're detached, can't join) */
    sleep(1);

#if TLS_ENABLED
    /* Cleanup TLS context (FSM-006 fix) */
    if (server->tls_ctx != NULL) {
        tls_context_cleanup(server->tls_ctx);
        server->tls_ctx = NULL;
    }
#endif

    /* Cleanup (no dynamic memory to free - all pre-allocated) */
    pthread_mutex_destroy(&server->rate_limit_mutex);
    pthread_mutex_destroy(&server->session_mutex);
    free(server); /* Only the server structure itself was malloc'd */

    printf("Management interface stopped\n");
}

/**
 * mgmt_server_get_active_sessions - Get count of active sessions
 */
int mgmt_server_get_active_sessions(mgmt_server_t *server) {
    int count = 0;
    int i;

    if (server == NULL) {
        return 0;
    }

    pthread_mutex_lock(&server->session_mutex);
    for (i = 0; i < MAX_MGMT_SESSIONS; i++) {
        if (server->sessions[i].in_use) {
            count++;
        }
    }
    pthread_mutex_unlock(&server->session_mutex);

    return count;
}

/**
 * listener_thread - Main listener thread
 */
static void* listener_thread(void* arg) {
    mgmt_server_t *server = (mgmt_server_t*)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int client_fd;
    mgmt_session_t *session;
    pthread_t session_thread;
    in_addr_t client_ip;

    while (!server->shutdown_flag) {
        client_len = sizeof(client_addr);
        client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr,
                          &client_len);

        if (client_fd < 0) {
            if (server->shutdown_flag) {
                break; /* Normal shutdown */
            }
            fprintf(stderr, "Management accept failed: %s\n", strerror(errno));
            continue;
        }

        /* Extract client IP for rate limiting */
        client_ip = client_addr.sin_addr.s_addr;

        /* Check rate limit before accepting (NET-004, FSM-009 fix) */
        if (!check_rate_limit(server, client_ip)) {
            const char *msg = "Too many failed attempts. Try again later.\n";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        /* Acquire session slot */
        session = acquire_session_slot(server);
        if (session == NULL) {
            const char *msg = "Management server full (max sessions reached)\n";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        session->socket_fd = client_fd;
        session->client_ip = client_ip;
#if TLS_ENABLED
        session->tls = NULL;

        /* Perform TLS handshake if TLS is enabled (FSM-006 fix) */
        if (server->tls_enabled && server->tls_ctx != NULL) {
            session->tls = tls_session_create(server->tls_ctx, client_fd);
            if (session->tls == NULL) {
                const char *msg = "TLS handshake failed\n";
                write(client_fd, msg, strlen(msg));
                close(client_fd);
                release_session_slot(session);
                continue;
            }
        }
#endif

        /* Spawn session handler (detached) */
        if (pthread_create(&session_thread, NULL, session_handler, session) != 0) {
            fprintf(stderr, "Failed to create session thread: %s\n", strerror(errno));
            release_session_slot(session);
            close(client_fd);
            continue;
        }

        session->thread_id = session_thread;
        pthread_detach(session_thread);
    }

    pthread_exit(NULL);
}

/**
 * session_handler - Per-session thread handler
 *
 * Uses pre-allocated session buffers - NO dynamic allocation in this function.
 * All I/O uses fixed-size buffers from the session structure.
 */
static void* session_handler(void* arg) {
    mgmt_session_t *session = (mgmt_session_t*)arg;
    const char *welcome = "XOE Management Console v1.0\n";

    /* Send welcome (using TLS if enabled - FSM-006 fix) */
    mgmt_write(session, welcome, strlen(welcome));

    /* Authenticate (always required for security) */
    if (!authenticate_session(session)) {
        const char *msg = "Authentication failed\n";
        mgmt_write(session, msg, strlen(msg));
        /* Record auth failure for rate limiting (NET-004, FSM-009 fix) */
        record_auth_failure(session->server, session->client_ip);
#if TLS_ENABLED
        if (session->tls != NULL) {
            tls_session_shutdown(session->tls);
            tls_session_destroy(session->tls);
            session->tls = NULL;
        }
#endif
        close(session->socket_fd);
        release_session_slot(session);
        pthread_exit(NULL);
    }

    /* Clear any previous failures on successful auth */
    clear_auth_failure(session->server, session->client_ip);

    /* Main command loop (Phase 5) */
    mgmt_command_loop(session);

    /* Cleanup */
#if TLS_ENABLED
    if (session->tls != NULL) {
        tls_session_shutdown(session->tls);
        tls_session_destroy(session->tls);
        session->tls = NULL;
    }
#endif
    close(session->socket_fd);
    release_session_slot(session);
    pthread_exit(NULL);
}

/**
 * authenticate_session - Perform password authentication
 *
 * Uses session's pre-allocated read_buffer - no stack/heap allocation.
 */
static int authenticate_session(mgmt_session_t *session) {
    const char *prompt = "Password: ";
    ssize_t bytes_read;
    char *newline;
    int attempts = 0;

    while (attempts < 3) {
        /* Send prompt (using TLS if enabled - FSM-006 fix) */
        mgmt_write(session, prompt, strlen(prompt));

        /* Read password into pre-allocated buffer (blocking, with bounds check) */
        bytes_read = mgmt_read(session, session->read_buffer,
                              MGMT_BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            return 0; /* Connection closed */
        }

        session->read_buffer[bytes_read] = '\0';

        /* Strip newline */
        newline = strchr(session->read_buffer, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        newline = strchr(session->read_buffer, '\r');
        if (newline != NULL) {
            *newline = '\0';
        }

        /* Verify password against stored hash (constant-time comparison) */
        if (password_verify(session->read_buffer, session->password) == 1) {
            /* Clear password from buffer immediately (NET-015 fix) */
            secure_zero(session->read_buffer, MGMT_BUFFER_SIZE);
            mgmt_write(session, "Authentication successful\n\n", 27);
            session->authenticated = 1;
            return 1;
        }

        /* Clear password from buffer after failed attempt (NET-015 fix) */
        secure_zero(session->read_buffer, MGMT_BUFFER_SIZE);

        attempts++;
        if (attempts < 3) {
            const char *retry = "Incorrect password, try again\n";
            mgmt_write(session, retry, strlen(retry));
        }
    }

    return 0; /* Authentication failed after 3 attempts */
}

/**
 * acquire_session_slot - Acquire session from pool
 */
static mgmt_session_t* acquire_session_slot(mgmt_server_t *server) {
    mgmt_session_t *slot = NULL;
    int i;

    pthread_mutex_lock(&server->session_mutex);
    for (i = 0; i < MAX_MGMT_SESSIONS; i++) {
        if (!server->sessions[i].in_use) {
            server->sessions[i].in_use = 1;
            server->sessions[i].authenticated = 0;
            slot = &server->sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&server->session_mutex);

    return slot;
}

/**
 * release_session_slot - Release session back to pool
 */
static void release_session_slot(mgmt_session_t *session) {
    if (session != NULL) {
        session->in_use = 0;
        session->socket_fd = -1;
        session->authenticated = 0;
    }
}

/**
 * check_rate_limit - Check if IP is allowed to attempt authentication
 *
 * Returns 1 if allowed, 0 if rate limited (locked out).
 * Automatically clears expired lockouts.
 */
static int check_rate_limit(mgmt_server_t *server, in_addr_t ip) {
    int i;
    time_t now;
    int allowed = 1;

    if (server == NULL) {
        return 1;
    }

    now = time(NULL);

    pthread_mutex_lock(&server->rate_limit_mutex);

    for (i = 0; i < MGMT_RATE_LIMIT_ENTRIES; i++) {
        if (server->rate_limits[i].ip_addr == ip) {
            /* Check if lockout has expired */
            if (server->rate_limits[i].lockout_until > 0) {
                if (now < server->rate_limits[i].lockout_until) {
                    /* Still locked out */
                    allowed = 0;
                } else {
                    /* Lockout expired, clear entry */
                    server->rate_limits[i].ip_addr = 0;
                    server->rate_limits[i].failure_count = 0;
                    server->rate_limits[i].lockout_until = 0;
                }
            }
            break;
        }
    }

    pthread_mutex_unlock(&server->rate_limit_mutex);

    return allowed;
}

/**
 * record_auth_failure - Record failed authentication attempt
 *
 * Increments failure count for IP. If threshold reached, sets lockout.
 * Uses LRU-style replacement if table is full.
 */
static void record_auth_failure(mgmt_server_t *server, in_addr_t ip) {
    int i;
    int found = -1;
    int empty = -1;
    time_t now;

    if (server == NULL) {
        return;
    }

    now = time(NULL);

    pthread_mutex_lock(&server->rate_limit_mutex);

    /* Find existing entry or empty slot */
    for (i = 0; i < MGMT_RATE_LIMIT_ENTRIES; i++) {
        if (server->rate_limits[i].ip_addr == ip) {
            found = i;
            break;
        }
        if (empty < 0 && server->rate_limits[i].ip_addr == 0) {
            empty = i;
        }
    }

    if (found >= 0) {
        /* Increment existing entry */
        server->rate_limits[found].failure_count++;
        if (server->rate_limits[found].failure_count >= MGMT_RATE_LIMIT_FAILURES) {
            server->rate_limits[found].lockout_until = now + MGMT_RATE_LIMIT_LOCKOUT;
            fprintf(stderr, "Rate limit: IP locked out for %d seconds\n",
                    MGMT_RATE_LIMIT_LOCKOUT);
        }
    } else if (empty >= 0) {
        /* Create new entry in empty slot */
        server->rate_limits[empty].ip_addr = ip;
        server->rate_limits[empty].failure_count = 1;
        server->rate_limits[empty].lockout_until = 0;
    } else {
        /* Table full, overwrite first entry (simple replacement policy) */
        server->rate_limits[0].ip_addr = ip;
        server->rate_limits[0].failure_count = 1;
        server->rate_limits[0].lockout_until = 0;
    }

    pthread_mutex_unlock(&server->rate_limit_mutex);
}

/**
 * clear_auth_failure - Clear auth failure record on successful login
 *
 * Removes the IP from rate limit tracking after successful authentication.
 */
static void clear_auth_failure(mgmt_server_t *server, in_addr_t ip) {
    int i;

    if (server == NULL) {
        return;
    }

    pthread_mutex_lock(&server->rate_limit_mutex);

    for (i = 0; i < MGMT_RATE_LIMIT_ENTRIES; i++) {
        if (server->rate_limits[i].ip_addr == ip) {
            server->rate_limits[i].ip_addr = 0;
            server->rate_limits[i].failure_count = 0;
            server->rate_limits[i].lockout_until = 0;
            break;
        }
    }

    pthread_mutex_unlock(&server->rate_limit_mutex);
}
