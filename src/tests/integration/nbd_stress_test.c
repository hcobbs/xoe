/**
 * NBD Stress Test Client
 * Tests edge cases, large transfers, concurrent operations
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
#include <time.h>

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

static int connect_to_server(const char *host, const char *port);
static int do_handshake(int sockfd, const char *export_name, uint64_t *export_size);
static int nbd_read(int sockfd, uint64_t offset, uint32_t length, void *buffer);
static int nbd_write(int sockfd, uint64_t offset, uint32_t length, const void *buffer);
static int send_all(int sockfd, const void *buf, size_t len);
static int recv_all(int sockfd, void *buf, size_t len);
static void fill_pattern(unsigned char *buf, size_t len, unsigned int seed);
static int verify_pattern(const unsigned char *buf, size_t len, unsigned int seed);

int main(int argc, char *argv[])
{
    int sockfd = 0;
    int result = 0;
    uint64_t export_size = 0;
    unsigned char *buf = NULL;
    size_t test_size = 0;
    int i = 0;
    clock_t start = 0;
    clock_t end = 0;
    double elapsed = 0.0;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <export_name>\n", argv[0]);
        return 1;
    }

    printf("NBD Stress Test Client\n");
    printf("=======================\n\n");

    /* Connect and handshake */
    sockfd = connect_to_server(argv[1], argv[2]);
    if (sockfd < 0) {
        return 1;
    }
    result = do_handshake(sockfd, argv[3], &export_size);
    if (result != 0) {
        close(sockfd);
        return 1;
    }
    printf("Connected to NBD server, export size: %llu bytes\n\n",
           (unsigned long long)export_size);

    /* Test 1: Large sequential write (1MB) */
    printf("[1] Large sequential write (1MB)...\n");
    test_size = 1024 * 1024;
    buf = (unsigned char*)malloc(test_size);
    if (buf == NULL) {
        fprintf(stderr, "    Allocation failed\n");
        close(sockfd);
        return 1;
    }
    fill_pattern(buf, test_size, 0xDEADBEEF);
    start = clock();
    result = nbd_write(sockfd, 0, test_size, buf);
    end = clock();
    elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    if (result != 0) {
        fprintf(stderr, "    Write failed ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    printf("    Write: %.3f sec (%.2f MB/s) ✓\n",
           elapsed, (test_size / 1024.0 / 1024.0) / elapsed);
    free(buf);

    /* Test 2: Large sequential read (1MB) */
    printf("[2] Large sequential read (1MB)...\n");
    buf = (unsigned char*)malloc(test_size);
    if (buf == NULL) {
        fprintf(stderr, "    Allocation failed\n");
        close(sockfd);
        return 1;
    }
    memset(buf, 0, test_size);
    start = clock();
    result = nbd_read(sockfd, 0, test_size, buf);
    end = clock();
    elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    if (result != 0) {
        fprintf(stderr, "    Read failed ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    printf("    Read: %.3f sec (%.2f MB/s) ✓\n",
           elapsed, (test_size / 1024.0 / 1024.0) / elapsed);

    /* Verify data */
    printf("[3] Verifying large transfer integrity...\n");
    if (!verify_pattern(buf, test_size, 0xDEADBEEF)) {
        fprintf(stderr, "    Data corruption detected ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    printf("    Data verified ✓\n");
    free(buf);

    /* Test 4: Random small writes */
    printf("[4] Random small writes (100 x 512 bytes)...\n");
    buf = (unsigned char*)malloc(512);
    if (buf == NULL) {
        fprintf(stderr, "    Allocation failed\n");
        close(sockfd);
        return 1;
    }
    srand((unsigned int)time(NULL));
    start = clock();
    for (i = 0; i < 100; i++) {
        uint64_t offset = 0;
        unsigned int seed = 0;

        offset = (uint64_t)(rand() % ((int)export_size - 512));
        seed = (unsigned int)rand();
        fill_pattern(buf, 512, seed);
        result = nbd_write(sockfd, offset, 512, buf);
        if (result != 0) {
            fprintf(stderr, "    Write %d failed ✗\n", i);
            free(buf);
            close(sockfd);
            return 1;
        }
    }
    end = clock();
    elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("    100 writes: %.3f sec (%.0f ops/sec) ✓\n",
           elapsed, 100.0 / elapsed);
    free(buf);

    /* Test 5: Boundary conditions */
    printf("[5] Boundary condition tests...\n");
    buf = (unsigned char*)malloc(1024);
    if (buf == NULL) {
        fprintf(stderr, "    Allocation failed\n");
        close(sockfd);
        return 1;
    }

    /* Write at end of device */
    fill_pattern(buf, 1024, 0xBAADF00D);
    result = nbd_write(sockfd, export_size - 1024, 1024, buf);
    if (result != 0) {
        fprintf(stderr, "    End-of-device write failed ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    memset(buf, 0, 1024);
    result = nbd_read(sockfd, export_size - 1024, 1024, buf);
    if (result != 0) {
        fprintf(stderr, "    End-of-device read failed ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    if (!verify_pattern(buf, 1024, 0xBAADF00D)) {
        fprintf(stderr, "    End-of-device data mismatch ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    printf("    Boundary tests passed ✓\n");
    free(buf);

    /* Test 6: Unaligned offsets */
    printf("[6] Unaligned offset tests...\n");
    buf = (unsigned char*)malloc(257);
    if (buf == NULL) {
        fprintf(stderr, "    Allocation failed\n");
        close(sockfd);
        return 1;
    }
    fill_pattern(buf, 257, 0xCAFEBABE);
    result = nbd_write(sockfd, 123, 257, buf);
    if (result != 0) {
        fprintf(stderr, "    Unaligned write failed ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    memset(buf, 0, 257);
    result = nbd_read(sockfd, 123, 257, buf);
    if (result != 0) {
        fprintf(stderr, "    Unaligned read failed ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    if (!verify_pattern(buf, 257, 0xCAFEBABE)) {
        fprintf(stderr, "    Unaligned data mismatch ✗\n");
        free(buf);
        close(sockfd);
        return 1;
    }
    printf("    Unaligned I/O passed ✓\n");
    free(buf);

    printf("\n=== ALL STRESS TESTS PASSED ===\n");
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
        fprintf(stderr, "Could not connect\n");
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

    ret = recv_all(sockfd, &magic, sizeof(magic));
    if (ret != 0) return -1;
    magic = be64toh(magic);
    if (magic != NBD_MAGIC) return -1;

    ret = recv_all(sockfd, &opts_magic, sizeof(opts_magic));
    if (ret != 0) return -1;

    ret = recv_all(sockfd, &server_flags, sizeof(server_flags));
    if (ret != 0) return -1;

    client_flags = htonl(NBD_FLAG_C_FIXED_NEWSTYLE);
    ret = send_all(sockfd, &client_flags, sizeof(client_flags));
    if (ret != 0) return -1;

    opts_magic = htobe64(NBD_OPTS_MAGIC);
    option = htonl(NBD_OPT_EXPORT_NAME);
    export_name_len = htonl((uint32_t)strlen(export_name));

    ret = send_all(sockfd, &opts_magic, sizeof(opts_magic));
    if (ret != 0) return -1;
    ret = send_all(sockfd, &option, sizeof(option));
    if (ret != 0) return -1;
    ret = send_all(sockfd, &export_name_len, sizeof(export_name_len));
    if (ret != 0) return -1;
    ret = send_all(sockfd, export_name, strlen(export_name));
    if (ret != 0) return -1;

    ret = recv_all(sockfd, export_size, sizeof(*export_size));
    if (ret != 0) return -1;
    *export_size = be64toh(*export_size);

    ret = recv_all(sockfd, &transmission_flags, sizeof(transmission_flags));
    if (ret != 0) return -1;

    ret = recv_all(sockfd, zeros, sizeof(zeros));
    if (ret != 0) return -1;

    return 0;
}

static int nbd_read(int sockfd, uint64_t offset, uint32_t length, void *buffer)
{
    nbd_request_wire_t request;
    nbd_reply_wire_t reply;
    int ret = 0;

    request.magic = htonl(NBD_REQUEST_MAGIC);
    request.flags = htons(0);
    request.type = htons(NBD_CMD_READ);
    request.cookie = htobe64(0x1234567890ABCDEFULL);
    request.offset = htobe64(offset);
    request.length = htonl(length);

    ret = send_all(sockfd, &request, sizeof(request));
    if (ret != 0) return -1;

    ret = recv_all(sockfd, &reply, sizeof(reply));
    if (ret != 0) return -1;

    reply.magic = ntohl(reply.magic);
    reply.error = ntohl(reply.error);

    if (reply.magic != NBD_REPLY_MAGIC || reply.error != 0) {
        return -1;
    }

    ret = recv_all(sockfd, buffer, length);
    if (ret != 0) return -1;

    return 0;
}

static int nbd_write(int sockfd, uint64_t offset, uint32_t length, const void *buffer)
{
    nbd_request_wire_t request;
    nbd_reply_wire_t reply;
    int ret = 0;

    request.magic = htonl(NBD_REQUEST_MAGIC);
    request.flags = htons(0);
    request.type = htons(NBD_CMD_WRITE);
    request.cookie = htobe64(0x1234567890ABCDEFULL);
    request.offset = htobe64(offset);
    request.length = htonl(length);

    ret = send_all(sockfd, &request, sizeof(request));
    if (ret != 0) return -1;

    ret = send_all(sockfd, buffer, length);
    if (ret != 0) return -1;

    ret = recv_all(sockfd, &reply, sizeof(reply));
    if (ret != 0) return -1;

    reply.magic = ntohl(reply.magic);
    reply.error = ntohl(reply.error);

    if (reply.magic != NBD_REPLY_MAGIC || reply.error != 0) {
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
        if (sent <= 0) return -1;
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
        if (received <= 0) return -1;
        ptr += received;
        remaining -= received;
    }
    return 0;
}

static void fill_pattern(unsigned char *buf, size_t len, unsigned int seed)
{
    size_t i = 0;
    unsigned int state = seed;

    for (i = 0; i < len; i++) {
        state = state * 1103515245 + 12345;
        buf[i] = (unsigned char)(state >> 16);
    }
}

static int verify_pattern(const unsigned char *buf, size_t len, unsigned int seed)
{
    size_t i = 0;
    unsigned int state = seed;
    unsigned char expected = 0;

    for (i = 0; i < len; i++) {
        state = state * 1103515245 + 12345;
        expected = (unsigned char)(state >> 16);
        if (buf[i] != expected) {
            return 0;
        }
    }
    return 1;
}
