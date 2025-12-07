/**
 * @file test_net_resolve.c
 * @brief Unit tests for network address resolution module
 *
 * Tests the net_resolve module for proper hostname resolution,
 * error handling, and thread-safe operation.
 *
 * [LLM-ARCH]
 */

#include "tests/framework/test_framework.h"
#include "lib/net/net_resolve.h"
#include "lib/common/definitions.h"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ============================================================================
 * net_resolve_to_sockaddr() Tests
 * ============================================================================ */

/**
 * @brief Test numeric IPv4 address resolution
 *
 * Verifies that a dotted-decimal IP address resolves correctly.
 */
void test_sockaddr_numeric_ipv4(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;
    char ip_str[INET_ADDRSTRLEN];

    ret = net_resolve_to_sockaddr("127.0.0.1", 8080, &addr, &result);

    TEST_ASSERT_SUCCESS(ret, "Numeric IP resolution should succeed");
    TEST_ASSERT_EQUAL(0, result.error_code, "Result error_code should be 0");
    TEST_ASSERT_EQUAL(AF_INET, addr.sin_family, "Family should be AF_INET");
    TEST_ASSERT_EQUAL(htons(8080), addr.sin_port, "Port should be 8080");

    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    TEST_ASSERT_STR_EQUAL("127.0.0.1", ip_str, "Address should be 127.0.0.1");
}

/**
 * @brief Test another numeric IPv4 address
 *
 * Verifies resolution of a different IP address.
 */
void test_sockaddr_numeric_ipv4_other(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;
    char ip_str[INET_ADDRSTRLEN];

    ret = net_resolve_to_sockaddr("192.168.1.1", 12345, &addr, &result);

    TEST_ASSERT_SUCCESS(ret, "Numeric IP resolution should succeed");
    TEST_ASSERT_EQUAL(htons(12345), addr.sin_port, "Port should be 12345");

    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    TEST_ASSERT_STR_EQUAL("192.168.1.1", ip_str, "Address should be 192.168.1.1");
}

/**
 * @brief Test localhost hostname resolution
 *
 * Verifies that "localhost" resolves to a valid address.
 */
void test_sockaddr_localhost(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr("localhost", 8080, &addr, &result);

    TEST_ASSERT_SUCCESS(ret, "localhost resolution should succeed");
    TEST_ASSERT_EQUAL(0, result.error_code, "Result error_code should be 0");
    TEST_ASSERT_EQUAL(AF_INET, addr.sin_family, "Family should be AF_INET");
    TEST_ASSERT_EQUAL(htons(8080), addr.sin_port, "Port should be 8080");
    /* localhost typically resolves to 127.0.0.1, but we don't check exact value */
}

/**
 * @brief Test NULL host parameter
 *
 * Verifies that NULL host returns E_INVALID_ARGUMENT.
 */
void test_sockaddr_null_host(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr(NULL, 8080, &addr, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "NULL host should return E_INVALID_ARGUMENT");
    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, result.error_code,
                      "Result error_code should be E_INVALID_ARGUMENT");
}

/**
 * @brief Test NULL addr parameter
 *
 * Verifies that NULL addr returns E_INVALID_ARGUMENT.
 */
void test_sockaddr_null_addr(void) {
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr("localhost", 8080, NULL, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "NULL addr should return E_INVALID_ARGUMENT");
    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, result.error_code,
                      "Result error_code should be E_INVALID_ARGUMENT");
}

/**
 * @brief Test NULL result parameter (should not crash)
 *
 * Verifies that NULL result parameter is handled gracefully.
 */
void test_sockaddr_null_result(void) {
    struct sockaddr_in addr;
    int ret;

    ret = net_resolve_to_sockaddr("127.0.0.1", 8080, &addr, NULL);

    TEST_ASSERT_SUCCESS(ret, "NULL result should still work for valid input");
}

/**
 * @brief Test negative port
 *
 * Verifies that negative port returns E_INVALID_ARGUMENT.
 */
void test_sockaddr_negative_port(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr("localhost", -1, &addr, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "Negative port should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test port too large
 *
 * Verifies that port > 65535 returns E_INVALID_ARGUMENT.
 */
void test_sockaddr_port_too_large(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr("localhost", 70000, &addr, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "Port > 65535 should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test port zero (valid for bind)
 *
 * Verifies that port 0 is allowed (used for bind with OS-assigned port).
 */
void test_sockaddr_port_zero(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr("localhost", 0, &addr, &result);

    TEST_ASSERT_SUCCESS(ret, "Port 0 should succeed (valid for bind)");
    TEST_ASSERT_EQUAL(0, addr.sin_port, "Port should be 0 in network byte order");
}

/**
 * @brief Test invalid hostname
 *
 * Verifies that an invalid hostname returns E_DNS_ERROR.
 */
void test_sockaddr_invalid_hostname(void) {
    struct sockaddr_in addr;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_to_sockaddr("this.domain.definitely.does.not.exist.invalid",
                                   8080, &addr, &result);

    TEST_ASSERT_EQUAL(E_DNS_ERROR, ret, "Invalid hostname should return E_DNS_ERROR");
    TEST_ASSERT_EQUAL(E_DNS_ERROR, result.error_code,
                      "Result error_code should be E_DNS_ERROR");
    TEST_ASSERT_NOT_EQUAL(0, result.gai_error, "gai_error should be non-zero");
}

/* ============================================================================
 * net_resolve_connect() Tests
 * ============================================================================ */

/**
 * @brief Test connect with NULL host
 *
 * Verifies that NULL host returns E_INVALID_ARGUMENT.
 */
void test_connect_null_host(void) {
    int sock = -1;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_connect(NULL, 8080, &sock, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "NULL host should return E_INVALID_ARGUMENT");
    TEST_ASSERT_EQUAL(-1, sock, "Socket should remain -1 on error");
}

/**
 * @brief Test connect with NULL sock_out
 *
 * Verifies that NULL sock_out returns E_INVALID_ARGUMENT.
 */
void test_connect_null_sock(void) {
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_connect("localhost", 8080, NULL, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "NULL sock_out should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test connect with invalid port (zero)
 *
 * Verifies that port 0 returns E_INVALID_ARGUMENT for connect.
 */
void test_connect_port_zero(void) {
    int sock = -1;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_connect("localhost", 0, &sock, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "Port 0 should return E_INVALID_ARGUMENT for connect");
}

/**
 * @brief Test connect with port too large
 *
 * Verifies that port > 65535 returns E_INVALID_ARGUMENT.
 */
void test_connect_port_too_large(void) {
    int sock = -1;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_connect("localhost", 70000, &sock, &result);

    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, ret, "Port > 65535 should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test connect with invalid hostname
 *
 * Verifies that an invalid hostname returns E_DNS_ERROR.
 */
void test_connect_invalid_hostname(void) {
    int sock = -1;
    net_resolve_result_t result;
    int ret;

    ret = net_resolve_connect("this.domain.definitely.does.not.exist.invalid",
                               8080, &sock, &result);

    TEST_ASSERT_EQUAL(E_DNS_ERROR, ret, "Invalid hostname should return E_DNS_ERROR");
    TEST_ASSERT_EQUAL(-1, sock, "Socket should be -1 on DNS error");
}

/**
 * @brief Test connect to non-listening port (connection refused)
 *
 * Verifies that connecting to a non-listening port returns E_NETWORK_ERROR.
 */
void test_connect_refused(void) {
    int sock = -1;
    net_resolve_result_t result;
    int ret;

    /* Port 1 is almost never in use and should refuse connections */
    ret = net_resolve_connect("127.0.0.1", 1, &sock, &result);

    TEST_ASSERT_EQUAL(E_NETWORK_ERROR, ret, "Connection to closed port should return E_NETWORK_ERROR");
    TEST_ASSERT_EQUAL(-1, sock, "Socket should be -1 on connection error");
    TEST_ASSERT_NOT_EQUAL(0, result.sys_errno, "sys_errno should be set");
}

/* ============================================================================
 * net_resolve_format_error() Tests
 * ============================================================================ */

/**
 * @brief Test format error with success result
 *
 * Verifies that success result formats correctly.
 */
void test_format_error_success(void) {
    net_resolve_result_t result = {0, 0, 0};
    char buf[256];

    net_resolve_format_error(&result, buf, sizeof(buf));

    TEST_ASSERT_STR_EQUAL("Success", buf, "Success should format as 'Success'");
}

/**
 * @brief Test format error with invalid argument
 *
 * Verifies that invalid argument error formats correctly.
 */
void test_format_error_invalid_arg(void) {
    net_resolve_result_t result = {E_INVALID_ARGUMENT, 0, 0};
    char buf[256];

    net_resolve_format_error(&result, buf, sizeof(buf));

    TEST_ASSERT_STR_EQUAL("Invalid argument", buf,
                          "Invalid argument should format correctly");
}

/**
 * @brief Test format error with DNS error
 *
 * Verifies that DNS error formats with gai_strerror message.
 */
void test_format_error_dns(void) {
    net_resolve_result_t result = {E_DNS_ERROR, EAI_NONAME, 0};
    char buf[256];

    net_resolve_format_error(&result, buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "DNS"), "DNS error should contain 'DNS'");
}

/**
 * @brief Test format error with NULL result
 *
 * Verifies that NULL result is handled gracefully.
 */
void test_format_error_null_result(void) {
    char buf[256] = "unchanged";

    net_resolve_format_error(NULL, buf, sizeof(buf));

    TEST_ASSERT_STR_EQUAL("unchanged", buf, "NULL result should not modify buffer");
}

/**
 * @brief Test format error with NULL buffer
 *
 * Verifies that NULL buffer is handled gracefully.
 */
void test_format_error_null_buffer(void) {
    net_resolve_result_t result = {0, 0, 0};

    /* Should not crash */
    net_resolve_format_error(&result, NULL, 256);

    TEST_ASSERT(1, "NULL buffer should not crash");
}

/**
 * @brief Test format error with zero buffer size
 *
 * Verifies that zero buffer size is handled gracefully.
 */
void test_format_error_zero_buflen(void) {
    net_resolve_result_t result = {0, 0, 0};
    char buf[256] = "unchanged";

    net_resolve_format_error(&result, buf, 0);

    TEST_ASSERT_STR_EQUAL("unchanged", buf, "Zero buflen should not modify buffer");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("=== Network Resolution Unit Tests ===\n\n");

    /* net_resolve_to_sockaddr() tests */
    run_test("test_sockaddr_numeric_ipv4", test_sockaddr_numeric_ipv4);
    run_test("test_sockaddr_numeric_ipv4_other", test_sockaddr_numeric_ipv4_other);
    run_test("test_sockaddr_localhost", test_sockaddr_localhost);
    run_test("test_sockaddr_null_host", test_sockaddr_null_host);
    run_test("test_sockaddr_null_addr", test_sockaddr_null_addr);
    run_test("test_sockaddr_null_result", test_sockaddr_null_result);
    run_test("test_sockaddr_negative_port", test_sockaddr_negative_port);
    run_test("test_sockaddr_port_too_large", test_sockaddr_port_too_large);
    run_test("test_sockaddr_port_zero", test_sockaddr_port_zero);
    run_test("test_sockaddr_invalid_hostname", test_sockaddr_invalid_hostname);

    /* net_resolve_connect() tests */
    run_test("test_connect_null_host", test_connect_null_host);
    run_test("test_connect_null_sock", test_connect_null_sock);
    run_test("test_connect_port_zero", test_connect_port_zero);
    run_test("test_connect_port_too_large", test_connect_port_too_large);
    run_test("test_connect_invalid_hostname", test_connect_invalid_hostname);
    run_test("test_connect_refused", test_connect_refused);

    /* net_resolve_format_error() tests */
    run_test("test_format_error_success", test_format_error_success);
    run_test("test_format_error_invalid_arg", test_format_error_invalid_arg);
    run_test("test_format_error_dns", test_format_error_dns);
    run_test("test_format_error_null_result", test_format_error_null_result);
    run_test("test_format_error_null_buffer", test_format_error_null_buffer);
    run_test("test_format_error_zero_buflen", test_format_error_zero_buflen);

    print_test_summary();

    return (tests_failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
