/**
 * NBD I/O Test Client
 * Tests NBD read/write operations after handshake
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#else
#include <endian.h>
#endif

/* NBD Protocol Constants */
#define NBD_MAGIC               0x4e42444d41474943ULL
#define NBD_OPTS_MAGIC          0x49484156454f5054ULL
#define NBD_REQUEST_MAGIC       0x25609513UL
#define NBD_REPLY_MAGIC         0x67446698UL

#define NBD_FLAG_FIXED_NEWSTYLE 0x0001
#define NBD_FLAG_C_FIXED_NEWSTYLE 0x0001

#define NBD_OPT_EXPORT_NAME     1
#define NBD_CMD_READ            0
#define NBD_CMD_WRITE           1
#define NBD_CMD_DISC            2
#define NBD_CMD_FLUSH           3

static int connect_to_server(const char *host, const char *port);
static int do_handshake(int sockfd, const char *export_name, uint64_t *export_size);
static int nbd_read(int sockfd, uint64_t offset, uint32_t length, void *buffer);
static int nbd_write(int sockfd, uint64_t offset, uint32_t length, const void *buffer);
static int send_all(int sockfd, const void *buf, size_t len);
static int recv_all(int sockfd, void *buf, size_t len);

int main(int argc, char *argv[])
{
    int sockfd = 0;
    int result = 0;
    uint64_t export_size = 0;
    unsigned char write_buf[4096];
    unsigned char read_buf[4096];
    int i = 0;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <export_name>\n", argv[0]);
        fprintf(stderr, "Example: %s localhost 10809 /tmp/test.img\n", argv[0]);
        return 1;
    }

    printf("NBD I/O Test Client\n");
    printf("===================\n\n");

    /* Connect and handshake */
    printf("[1] Connecting to %s:%s...\n", argv[1], argv[2]);
    sockfd = connect_to_server(argv[1], argv[2]);
    if (sockfd < 0) {
        return 1;
    }
    printf("    Connected ✓\n\n");

    printf("[2] Performing NBD handshake...\n");
    result = do_handshake(sockfd, argv[3], &export_size);
    if (result != 0) {
        close(sockfd);
        return 1;
    }
    printf("    Handshake complete ✓\n");
    printf("    Export size: %llu bytes\n\n", (unsigned long long)export_size);

    /* Test 1: Write pattern */
    printf("[3] Writing test pattern (4KB at offset 0)...\n");
    for (i = 0; i < 4096; i++) {
        write_buf[i] = (unsigned char)(i & 0xFF);
    }
    result = nbd_write(sockfd, 0, 4096, write_buf);
    if (result != 0) {
        fprintf(stderr, "    Write failed ✗\n");
        close(sockfd);
        return 1;
    }
    printf("    Write successful ✓\n\n");

    /* Test 2: Read back */
    printf("[4] Reading back data (4KB at offset 0)...\n");
    memset(read_buf, 0, sizeof(read_buf));
    result = nbd_read(sockfd, 0, 4096, read_buf);
    if (result != 0) {
        fprintf(stderr, "    Read failed ✗\n");
        close(sockfd);
        return 1;
    }
    printf("    Read successful ✓\n\n");

    /* Test 3: Verify */
    printf("[5] Verifying data integrity...\n");
    if (memcmp(write_buf, read_buf, 4096) != 0) {
        fprintf(stderr, "    Data mismatch ✗\n");
        fprintf(stderr, "    First mismatch at byte: ");
        for (i = 0; i < 4096; i++) {
            if (write_buf[i] != read_buf[i]) {
                fprintf(stderr, "%d (wrote 0x%02x, read 0x%02x)\n",
                        i, write_buf[i], read_buf[i]);
                break;
            }
        }
        close(sockfd);
        return 1;
    }
    printf("    Data verified ✓\n\n");

    /* Test 4: Different offset */
    printf("[6] Testing offset writes (1KB at offset 8192)...\n");
    result = nbd_write(sockfd, 8192, 1024, write_buf);
    if (result != 0) {
        fprintf(stderr, "    Write failed ✗\n");
        close(sockfd);
        return 1;
    }
    memset(read_buf, 0, sizeof(read_buf));
    result = nbd_read(sockfd, 8192, 1024, read_buf);
    if (result != 0) {
        fprintf(stderr, "    Read failed ✗\n");
        close(sockfd);
        return 1;
    }
    if (memcmp(write_buf, read_buf, 1024) != 0) {
        fprintf(stderr, "    Data mismatch at offset ✗\n");
        close(sockfd);
        return 1;
    }
    printf("    Offset I/O verified ✓\n\n");

    printf("=== ALL TESTS PASSED ===\n");
    close(sockfd);
    return 0;
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

static int do_handshake(int sockfd, const char *export_name, uint64_t *export_size)
{
    uint64_t magic = 0;
    uint64_t opts_magic = 0;
    uint16_t server_flags = 0;
    uint32_t client_flags = 0;
    uint32_t option = 0;
    uint32_t export_name_len = 0;
    uint16_t transmission_flags = 0;
    char zeros[124];
    int ret = 0;

    memset(zeros, 0, sizeof(zeros));

    /* Receive server greeting */
    ret = recv_all(sockfd, &magic, sizeof(magic));
    if (ret != 0) {
        return -1;
    }
    magic = be64toh(magic);
    if (magic != NBD_MAGIC) {
        fprintf(stderr, "Invalid NBD magic\n");
        return -1;
    }

    ret = recv_all(sockfd, &opts_magic, sizeof(opts_magic));
    if (ret != 0) {
        return -1;
    }

    ret = recv_all(sockfd, &server_flags, sizeof(server_flags));
    if (ret != 0) {
        return -1;
    }

    /* Send client flags */
    client_flags = htonl(NBD_FLAG_C_FIXED_NEWSTYLE);
    ret = send_all(sockfd, &client_flags, sizeof(client_flags));
    if (ret != 0) {
        return -1;
    }

    /* Send export name option */
    opts_magic = htobe64(NBD_OPTS_MAGIC);
    option = htonl(NBD_OPT_EXPORT_NAME);
    export_name_len = htonl((uint32_t)strlen(export_name));

    ret = send_all(sockfd, &opts_magic, sizeof(opts_magic));
    if (ret != 0) {
        return -1;
    }
    ret = send_all(sockfd, &option, sizeof(option));
    if (ret != 0) {
        return -1;
    }
    ret = send_all(sockfd, &export_name_len, sizeof(export_name_len));
    if (ret != 0) {
        return -1;
    }
    ret = send_all(sockfd, export_name, strlen(export_name));
    if (ret != 0) {
        return -1;
    }

    /* Receive export info */
    ret = recv_all(sockfd, export_size, sizeof(*export_size));
    if (ret != 0) {
        return -1;
    }
    *export_size = be64toh(*export_size);

    ret = recv_all(sockfd, &transmission_flags, sizeof(transmission_flags));
    if (ret != 0) {
        return -1;
    }

    ret = recv_all(sockfd, zeros, sizeof(zeros));
    if (ret != 0) {
        return -1;
    }

    return 0;
}

typedef struct {
    uint32_t magic;
    uint16_t flags;
    uint16_t type;
    uint64_t cookie;
    uint64_t offset;
    uint32_t length;
} __attribute__((packed)) nbd_request_wire_t;

typedef struct {
    uint32_t magic;
    uint32_t error;
    uint64_t cookie;
} __attribute__((packed)) nbd_reply_wire_t;

static int nbd_read(int sockfd, uint64_t offset, uint32_t length, void *buffer)
{
    nbd_request_wire_t request;
    nbd_reply_wire_t reply;
    int ret = 0;

    /* Build request */
    request.magic = htonl(NBD_REQUEST_MAGIC);
    request.flags = htons(0);
    request.type = htons(NBD_CMD_READ);
    request.cookie = htobe64(0x1234567890ABCDEFULL);
    request.offset = htobe64(offset);
    request.length = htonl(length);

    /* Send request as single write */
    ret = send_all(sockfd, &request, sizeof(request));
    if (ret != 0) return -1;

    /* Receive reply */
    ret = recv_all(sockfd, &reply, sizeof(reply));
    if (ret != 0) return -1;

    reply.magic = ntohl(reply.magic);
    reply.error = ntohl(reply.error);

    if (reply.magic != NBD_REPLY_MAGIC) {
        fprintf(stderr, "Invalid reply magic: 0x%08x\n", reply.magic);
        return -1;
    }
    if (reply.error != 0) {
        fprintf(stderr, "NBD read error: %u\n", reply.error);
        return -1;
    }

    /* Receive data */
    ret = recv_all(sockfd, buffer, length);
    if (ret != 0) return -1;

    return 0;
}

static int nbd_write(int sockfd, uint64_t offset, uint32_t length, const void *buffer)
{
    nbd_request_wire_t request;
    nbd_reply_wire_t reply;
    int ret = 0;

    /* Build request */
    request.magic = htonl(NBD_REQUEST_MAGIC);
    request.flags = htons(0);
    request.type = htons(NBD_CMD_WRITE);
    request.cookie = htobe64(0x1234567890ABCDEFULL);
    request.offset = htobe64(offset);
    request.length = htonl(length);

    /* Send request */
    ret = send_all(sockfd, &request, sizeof(request));
    if (ret != 0) return -1;

    /* Send data */
    ret = send_all(sockfd, buffer, length);
    if (ret != 0) return -1;

    /* Receive reply */
    ret = recv_all(sockfd, &reply, sizeof(reply));
    if (ret != 0) return -1;

    reply.magic = ntohl(reply.magic);
    reply.error = ntohl(reply.error);

    if (reply.magic != NBD_REPLY_MAGIC) {
        fprintf(stderr, "Invalid reply magic: 0x%08x\n", reply.magic);
        return -1;
    }
    if (reply.error != 0) {
        fprintf(stderr, "NBD write error: %u\n", reply.error);
        return -1;
    }

    return 0;
}

static int send_all(int sockfd, const void *buf, size_t len)
{
    const char *ptr = (const char*)buf;
    size_t remaining = len;
    ssize_t sent = 0;

    while (remaining > 0) {
        sent = send(sockfd, ptr, remaining, 0);
        if (sent <= 0) {
            if (sent < 0) {
                perror("send");
            }
            return -1;
        }
        ptr += sent;
        remaining -= sent;
    }
    return 0;
}

static int recv_all(int sockfd, void *buf, size_t len)
{
    char *ptr = (char*)buf;
    size_t remaining = len;
    ssize_t received = 0;

    while (remaining > 0) {
        received = recv(sockfd, ptr, remaining, 0);
        if (received <= 0) {
            if (received < 0) {
                perror("recv");
            } else {
                fprintf(stderr, "Connection closed prematurely\n");
            }
            return -1;
        }
        ptr += received;
        remaining -= received;
    }
    return 0;
}
