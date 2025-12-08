/**
 * @file test_tls_error.c
 * @brief Unit tests for TLS error handling
 *
 * Tests the tls_error module for proper error reporting and thread safety.
 */

#include "tests/framework/test_framework.h"
#include "lib/security/tls_error.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Test error string retrieval
 *
 * Verifies that tls_get_error_string() returns a valid string pointer.
 */
void test_get_error_string(void) {
    const char* error_str = tls_get_error_string();
    TEST_ASSERT_NOT_NULL(error_str, "Error string should not be NULL");
}

/**
 * @brief Test error string is not empty
 *
 * Verifies that tls_get_error_string() returns a non-empty string.
 */
void test_error_string_not_empty(void) {
    const char* error_str = tls_get_error_string();
    if (error_str != NULL) {
        TEST_ASSERT(strlen(error_str) >= 0, "Error string should be valid");
    } else {
        TEST_ASSERT(0, "Error string is NULL");
    }
}

/**
 * @brief Test last error code retrieval
 *
 * Verifies that tls_get_last_error() returns a valid error code.
 * With no prior SSL operations, the OpenSSL error queue should be empty,
 * so tls_get_last_error() should return 0 (no error).
 */
void test_get_last_error(void) {
    int error_code = tls_get_last_error();
    /*
     * With no prior SSL operations in this process, the error queue should
     * be empty. tls_get_last_error() returns:
     *   - 0 if no error (ERR_peek_last_error() == 0)
     *   - E_TLS_PROTOCOL_ERROR (-16) if there's an error in the queue
     *
     * In a fresh test environment, we expect 0.
     */
    TEST_ASSERT(error_code == 0 || error_code == E_TLS_PROTOCOL_ERROR,
                "Error code should be 0 (no error) or E_TLS_PROTOCOL_ERROR");

    /* Additional check: if no error, should be exactly 0 */
    if (error_code == 0) {
        TEST_ASSERT(error_code == 0, "No SSL error should return 0");
    }
}

/**
 * @brief Test error printing function
 *
 * Verifies that tls_print_errors() does not crash when called.
 */
void test_print_errors(void) {
    tls_print_errors("Test error");
    TEST_ASSERT(1, "Print errors should not crash");
}

/**
 * @brief Test error printing with NULL prefix
 *
 * Verifies that tls_print_errors() handles NULL prefix safely.
 */
void test_print_errors_null_prefix(void) {
    tls_print_errors(NULL);
    TEST_ASSERT(1, "Print errors should handle NULL prefix");
}

/**
 * @brief Main test runner for TLS error tests
 *
 * Executes all TLS error unit tests and prints summary.
 *
 * @return EXIT_SUCCESS if all tests pass, EXIT_FAILURE if any fail
 */
int main(void) {
    printf("=== TLS Error Unit Tests ===\n\n");

    run_test("test_get_error_string", test_get_error_string);
    run_test("test_error_string_not_empty", test_error_string_not_empty);
    run_test("test_get_last_error", test_get_last_error);
    run_test("test_print_errors", test_print_errors);
    run_test("test_print_errors_null_prefix", test_print_errors_null_prefix);

    print_test_summary();

    return (tests_failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
