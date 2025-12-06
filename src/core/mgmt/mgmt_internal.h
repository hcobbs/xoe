#ifndef CORE_MGMT_INTERNAL_H
#define CORE_MGMT_INTERNAL_H

#include <signal.h>
#include <pthread.h>

/**
 * Internal structures shared between management server and command handlers.
 * Not exposed to external code.
 */

#define MGMT_BUFFER_SIZE 1024    /* Per-session I/O buffer */
#define MGMT_PASSWORD_MAX 128    /* Max password length */

/* Session structure - shared between server and commands */
typedef struct mgmt_session_t {
    int socket_fd;              /* Session socket */
    int in_use;                 /* Pool slot in-use flag */
    pthread_t thread_id;        /* Session thread ID */
    char password[MGMT_PASSWORD_MAX]; /* Password buffer */
    volatile sig_atomic_t authenticated; /* Authentication status */
    char read_buffer[MGMT_BUFFER_SIZE];  /* Pre-allocated read buffer */
    char write_buffer[MGMT_BUFFER_SIZE]; /* Pre-allocated write buffer */
} mgmt_session_t;

#endif /* CORE_MGMT_INTERNAL_H */
