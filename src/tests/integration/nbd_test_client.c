/*
 * nbd_test_client.c - Minimal NBD client for testing handshake
 *
 * Verifies NBD Fixed Newstyle handshake protocol:
 * 1. Server sends NBDMAGIC + IHAVEOPT + handshake flags
 * 2. Client sends client flags
 * 3. Client sends NBD_OPT_EXPORT_NAME with export name
 * 4. Server sends export size + transmission flags
 * 5. Connection established (ready for NBD_CMD_* operations)
 *
 * Usage: nbd_test_client <host> <port> <export_name>
 * Example: nbd_test_client localhost 10809 /tmp/test.img
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

/* Portable byte-swap macros for systems without be64toh/htobe64 */
#ifndef be64toh
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define be64toh(x) __builtin_bswap64(x)
        #define htobe64(x) __builtin_bswap64(x)
    #else
        #define be64toh(x) (x)
        #define htobe64(x) (x)
    #endif
#endif

/* NBD protocol constants (from NBD specification) */
#define NBD_MAGIC               0x4e42444d41474943ULL  /* "NBDMAGIC" */
#define NBD_OPTS_MAGIC          0x49484156454F5054ULL  /* "IHAVEOPT" */
#define NBD_REP_MAGIC           0x3e889045565a9ULL     /* Reply magic */
#define NBD_REQUEST_MAGIC       0x25609513UL
#define NBD_REPLY_MAGIC         0x67446698UL

/* Handshake flags */
#define NBD_FLAG_FIXED_NEWSTYLE 0x0001
#define NBD_FLAG_NO_ZEROES      0x0002
#define NBD_FLAG_C_FIXED_NEWSTYLE 0x0001
#define NBD_FLAG_C_NO_ZEROES    0x0002

/* Options */
#define NBD_OPT_EXPORT_NAME     1
#define NBD_OPT_ABORT           2
#define NBD_OPT_LIST            3

/* Transmission flags */
#define NBD_FLAG_HAS_FLAGS      0x0001
#define NBD_FLAG_READ_ONLY      0x0002
#define NBD_FLAG_SEND_FLUSH     0x0004
#define NBD_FLAG_SEND_FUA       0x0008
#define NBD_FLAG_ROTATIONAL     0x0010
#define NBD_FLAG_SEND_TRIM      0x0020

/* Commands */
#define NBD_CMD_READ            0
#define NBD_CMD_WRITE           1
#define NBD_CMD_DISC            2
#define NBD_CMD_FLUSH           3
#define NBD_CMD_TRIM            4

/* Function prototypes */
static int connect_to_server(const char *host, const char *port);
static int do_handshake(int sockfd, const char *export_name);
static int send_all(int sockfd, const void *buf, size_t len);
static int recv_all(int sockfd, void *buf, size_t len);

int main(int argc, char *argv[])
{
    int sockfd = 0;
    int result = 0;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <export_name>\n", argv[0]);
        fprintf(stderr, "Example: %s localhost 10809 /tmp/test.img\n", argv[0]);
        return 1;
    }

    printf("NBD Test Client - Handshake Verification\n");
    printf("Connecting to %s:%s...\n", argv[1], argv[2]);

    sockfd = connect_to_server(argv[1], argv[2]);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }

    printf("Connected. Starting NBD handshake...\n");

    result = do_handshake(sockfd, argv[3]);
    if (result == 0) {
        printf("\n=== HANDSHAKE SUCCESS ===\n");
        printf("NBD connection established for export: %s\n", argv[3]);
        printf("Connection is ready for NBD commands.\n");
    } else {
        printf("\n=== HANDSHAKE FAILED ===\n");
    }

    close(sockfd);
    return result;
}

static int connect_to_server(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;
    int sockfd = 0;
    int ret = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(host, port, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) {
            continue;
        }

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        fprintf(stderr, "Could not connect to %s:%s\n", host, port);
        return -1;
    }

    return sockfd;
}

static int do_handshake(int sockfd, const char *export_name)
{
    uint64_t magic = 0;
    uint64_t opts_magic = 0;
    uint16_t server_flags = 0;
    uint32_t client_flags = 0;
    uint64_t export_size = 0;
    uint16_t transmission_flags = 0;
    uint32_t option = 0;
    uint32_t export_name_len = 0;
    char zeros[124];
    int ret = 0;

    memset(zeros, 0, sizeof(zeros));

    /* Step 1: Receive server greeting */
    printf("\n[1] Receiving server greeting...\n");

    ret = recv_all(sockfd, &magic, sizeof(magic));
    if (ret != 0) {
        fprintf(stderr, "Failed to receive NBD magic\n");
        return -1;
    }
    magic = be64toh(magic);
    printf("    Magic: 0x%016llx ", (unsigned long long)magic);
    if (magic == NBD_MAGIC) {
        printf("(NBDMAGIC) ✓\n");
    } else {
        printf("(INVALID) ✗\n");
        return -1;
    }

    ret = recv_all(sockfd, &opts_magic, sizeof(opts_magic));
    if (ret != 0) {
        fprintf(stderr, "Failed to receive options magic\n");
        return -1;
    }
    opts_magic = be64toh(opts_magic);
    printf("    Opts Magic: 0x%016llx ", (unsigned long long)opts_magic);
    if (opts_magic == NBD_OPTS_MAGIC) {
        printf("(IHAVEOPT) ✓\n");
    } else {
        printf("(INVALID) ✗\n");
        return -1;
    }

    ret = recv_all(sockfd, &server_flags, sizeof(server_flags));
    if (ret != 0) {
        fprintf(stderr, "Failed to receive server flags\n");
        return -1;
    }
    server_flags = ntohs(server_flags);
    printf("    Server Flags: 0x%04x ", server_flags);
    if (server_flags & NBD_FLAG_FIXED_NEWSTYLE) {
        printf("(FIXED_NEWSTYLE) ✓\n");
    } else {
        printf("(NOT FIXED_NEWSTYLE) ✗\n");
        return -1;
    }

    /* Step 2: Send client flags */
    printf("\n[2] Sending client flags...\n");
    client_flags = htonl(NBD_FLAG_C_FIXED_NEWSTYLE);
    ret = send_all(sockfd, &client_flags, sizeof(client_flags));
    if (ret != 0) {
        fprintf(stderr, "Failed to send client flags\n");
        return -1;
    }
    printf("    Client Flags: 0x%08x (FIXED_NEWSTYLE) ✓\n", NBD_FLAG_C_FIXED_NEWSTYLE);

    /* Step 3: Send NBD_OPT_EXPORT_NAME option */
    printf("\n[3] Requesting export: %s\n", export_name);

    opts_magic = htobe64(NBD_OPTS_MAGIC);
    ret = send_all(sockfd, &opts_magic, sizeof(opts_magic));
    if (ret != 0) {
        fprintf(stderr, "Failed to send option magic\n");
        return -1;
    }

    option = htonl(NBD_OPT_EXPORT_NAME);
    ret = send_all(sockfd, &option, sizeof(option));
    if (ret != 0) {
        fprintf(stderr, "Failed to send option type\n");
        return -1;
    }

    export_name_len = htonl(strlen(export_name));
    ret = send_all(sockfd, &export_name_len, sizeof(export_name_len));
    if (ret != 0) {
        fprintf(stderr, "Failed to send export name length\n");
        return -1;
    }

    ret = send_all(sockfd, export_name, strlen(export_name));
    if (ret != 0) {
        fprintf(stderr, "Failed to send export name\n");
        return -1;
    }
    printf("    Sent: NBD_OPT_EXPORT_NAME (%zu bytes) ✓\n", strlen(export_name));

    /* Step 4: Receive export information */
    printf("\n[4] Receiving export information...\n");

    ret = recv_all(sockfd, &export_size, sizeof(export_size));
    if (ret != 0) {
        fprintf(stderr, "Failed to receive export size\n");
        return -1;
    }
    export_size = be64toh(export_size);
    printf("    Export Size: %llu bytes (%.2f MB) ✓\n",
           (unsigned long long)export_size,
           (double)export_size / (1024.0 * 1024.0));

    ret = recv_all(sockfd, &transmission_flags, sizeof(transmission_flags));
    if (ret != 0) {
        fprintf(stderr, "Failed to receive transmission flags\n");
        return -1;
    }
    transmission_flags = ntohs(transmission_flags);
    printf("    Transmission Flags: 0x%04x\n", transmission_flags);
    if (transmission_flags & NBD_FLAG_HAS_FLAGS) printf("        - HAS_FLAGS\n");
    if (transmission_flags & NBD_FLAG_READ_ONLY) printf("        - READ_ONLY\n");
    if (transmission_flags & NBD_FLAG_SEND_FLUSH) printf("        - SEND_FLUSH\n");
    if (transmission_flags & NBD_FLAG_SEND_FUA) printf("        - SEND_FUA\n");
    if (transmission_flags & NBD_FLAG_ROTATIONAL) printf("        - ROTATIONAL\n");
    if (transmission_flags & NBD_FLAG_SEND_TRIM) printf("        - SEND_TRIM\n");

    /* Receive 124 bytes of zeros (reserved) */
    ret = recv_all(sockfd, zeros, 124);
    if (ret != 0) {
        fprintf(stderr, "Failed to receive reserved bytes\n");
        return -1;
    }
    printf("    Reserved: 124 bytes ✓\n");

    return 0;
}

static int send_all(int sockfd, const void *buf, size_t len)
{
    size_t total = 0;
    size_t remaining = len;
    ssize_t n = 0;
    const char *ptr = (const char *)buf;

    while (total < len) {
        n = send(sockfd, ptr + total, remaining, 0);
        if (n <= 0) {
            if (n == 0) {
                fprintf(stderr, "Connection closed during send\n");
            } else {
                fprintf(stderr, "Send error: %s\n", strerror(errno));
            }
            return -1;
        }
        total += n;
        remaining -= n;
    }

    return 0;
}

static int recv_all(int sockfd, void *buf, size_t len)
{
    size_t total = 0;
    size_t remaining = len;
    ssize_t n = 0;
    char *ptr = (char *)buf;

    while (total < len) {
        n = recv(sockfd, ptr + total, remaining, 0);
        if (n <= 0) {
            if (n == 0) {
                fprintf(stderr, "Connection closed during receive (got %zu/%zu bytes)\n",
                        total, len);
            } else {
                fprintf(stderr, "Receive error: %s\n", strerror(errno));
            }
            return -1;
        }
        total += n;
        remaining -= n;
    }

    return 0;
}
