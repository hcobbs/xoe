/**
 * @file test_tls_context.c
 * @brief Unit tests for TLS context management
 *
 * Tests the tls_context module for proper initialization, configuration,
 * and cleanup of SSL_CTX objects.
 */

#include "tests/framework/test_framework.h"
#include "lib/security/tls_context.h"
#include "lib/security/tls_config.h"
#include "lib/common/definitions.h"
#include <stdlib.h>

/**
 * @brief Test valid TLS 1.3 context initialization
 *
 * Verifies that tls_context_init() successfully creates a context
 * when given valid certificate, key, and TLS 1.3 version.
 */
void test_context_init_valid_tls13(void) {
    SSL_CTX* ctx = tls_context_init("./certs/server.crt", "./certs/server.key", ENCRYPT_TLS13);
    TEST_ASSERT_NOT_NULL(ctx, "TLS 1.3 context should initialize successfully");
    if (ctx) {
        tls_context_cleanup(ctx);
    }
}

/**
 * @brief Test valid TLS 1.2 context initialization
 *
 * Verifies that tls_context_init() successfully creates a context
 * when given valid certificate, key, and TLS 1.2 version.
 */
void test_context_init_valid_tls12(void) {
    SSL_CTX* ctx = tls_context_init("./certs/server.crt", "./certs/server.key", ENCRYPT_TLS12);
    TEST_ASSERT_NOT_NULL(ctx, "TLS 1.2 context should initialize successfully");
    if (ctx) {
        tls_context_cleanup(ctx);
    }
}

/**
 * @brief Test context initialization with NULL certificate path
 *
 * Verifies that tls_context_init() returns NULL when certificate path is NULL.
 */
void test_context_init_null_cert(void) {
    SSL_CTX* ctx = tls_context_init(NULL, "./certs/server.key", ENCRYPT_TLS13);
    TEST_ASSERT_NULL(ctx, "Context init should fail with NULL certificate");
}

/**
 * @brief Test context initialization with NULL key path
 *
 * Verifies that tls_context_init() returns NULL when key path is NULL.
 */
void test_context_init_null_key(void) {
    SSL_CTX* ctx = tls_context_init("./certs/server.crt", NULL, ENCRYPT_TLS13);
    TEST_ASSERT_NULL(ctx, "Context init should fail with NULL key");
}

/**
 * @brief Test context initialization with invalid TLS version
 *
 * Verifies that tls_context_init() returns NULL when given an invalid
 * TLS version parameter (not ENCRYPT_TLS12 or ENCRYPT_TLS13).
 */
void test_context_init_invalid_version(void) {
    SSL_CTX* ctx = tls_context_init("./certs/server.crt", "./certs/server.key", 99);
    TEST_ASSERT_NULL(ctx, "Context init should fail with invalid version");
}

/**
 * @brief Test context initialization with missing certificate file
 *
 * Verifies that tls_context_init() returns NULL when certificate file
 * does not exist.
 */
void test_context_init_missing_cert(void) {
    SSL_CTX* ctx = tls_context_init("/nonexistent/cert.crt", "./certs/server.key", ENCRYPT_TLS13);
    TEST_ASSERT_NULL(ctx, "Context init should fail with missing certificate");
}

/**
 * @brief Test context initialization with missing key file
 *
 * Verifies that tls_context_init() returns NULL when key file does not exist.
 */
void test_context_init_missing_key(void) {
    SSL_CTX* ctx = tls_context_init("./certs/server.crt", "/nonexistent/key.key", ENCRYPT_TLS13);
    TEST_ASSERT_NULL(ctx, "Context init should fail with missing key");
}

/**
 * @brief Test context cleanup with NULL pointer
 *
 * Verifies that tls_context_cleanup() safely handles NULL pointer
 * without crashing (no-op behavior).
 */
void test_context_cleanup_null(void) {
    tls_context_cleanup(NULL);
    TEST_ASSERT(1, "Context cleanup should handle NULL pointer safely");
}

/**
 * @brief Main test runner for TLS context tests
 *
 * Executes all TLS context unit tests and prints summary.
 *
 * @return EXIT_SUCCESS if all tests pass, EXIT_FAILURE if any fail
 */
int main(void) {
    printf("=== TLS Context Unit Tests ===\n\n");

    run_test("test_context_init_valid_tls13", test_context_init_valid_tls13);
    run_test("test_context_init_valid_tls12", test_context_init_valid_tls12);
    run_test("test_context_init_null_cert", test_context_init_null_cert);
    run_test("test_context_init_null_key", test_context_init_null_key);
    run_test("test_context_init_invalid_version", test_context_init_invalid_version);
    run_test("test_context_init_missing_cert", test_context_init_missing_cert);
    run_test("test_context_init_missing_key", test_context_init_missing_key);
    run_test("test_context_cleanup_null", test_context_cleanup_null);

    print_test_summary();

    return (tests_failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
