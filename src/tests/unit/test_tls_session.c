/**
 * @file test_tls_session.c
 * @brief Unit tests for TLS session management
 *
 * Tests the tls_session module for proper session creation, handshake,
 * shutdown, and cleanup.
 */

#include "tests/framework/test_framework.h"
#include "lib/security/tls_session.h"
#include "lib/security/tls_context.h"
#include "lib/security/tls_config.h"
#include "lib/common/definitions.h"
#include <stdlib.h>

/**
 * @brief Test session creation with NULL context
 *
 * Verifies that tls_session_create() returns NULL when given a NULL
 * SSL_CTX pointer.
 */
void test_session_create_null_context(void) {
    SSL* session = NULL;
    session = tls_session_create(NULL, 0);
    TEST_ASSERT_NULL(session, "Session create should fail with NULL context");
}

/**
 * @brief Test session creation with invalid socket
 *
 * Verifies that tls_session_create() handles invalid socket descriptor.
 * Note: This test creates a session object but handshake will fail.
 */
void test_session_create_invalid_socket(void) {
    SSL_CTX* ctx = NULL;
    SSL* session = NULL;

    ctx = tls_context_init("./certs/server.crt", "./certs/server.key", ENCRYPT_TLS13);
    if (ctx == NULL) {
        TEST_ASSERT(0, "Failed to initialize context for session test");
        return;
    }

    session = tls_session_create(ctx, -1);
    TEST_ASSERT_NULL(session, "Session create should fail with invalid socket");

    tls_context_cleanup(ctx);
}

/**
 * @brief Test session shutdown with NULL session
 *
 * Verifies that tls_session_shutdown() safely handles NULL pointer.
 */
void test_session_shutdown_null(void) {
    int result = 0;
    result = tls_session_shutdown(NULL);
    TEST_ASSERT(result < 0, "Session shutdown should return error for NULL session");
}

/**
 * @brief Test session destroy with NULL session
 *
 * Verifies that tls_session_destroy() safely handles NULL pointer
 * without crashing (no-op behavior).
 */
void test_session_destroy_null(void) {
    tls_session_destroy(NULL);
    TEST_ASSERT(1, "Session destroy should handle NULL pointer safely");
}

/**
 * @brief Main test runner for TLS session tests
 *
 * Executes all TLS session unit tests and prints summary.
 *
 * @return EXIT_SUCCESS if all tests pass, EXIT_FAILURE if any fail
 */
int main(void) {
    printf("=== TLS Session Unit Tests ===\n\n");

    run_test("test_session_create_null_context", test_session_create_null_context);
    run_test("test_session_create_invalid_socket", test_session_create_invalid_socket);
    run_test("test_session_shutdown_null", test_session_shutdown_null);
    run_test("test_session_destroy_null", test_session_destroy_null);

    print_test_summary();

    return (tests_failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
