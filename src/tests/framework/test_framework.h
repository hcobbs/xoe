/**
 * @file test_framework.h
 * @brief Simple C89-compliant unit test framework
 *
 * Provides basic test macros and utilities for unit testing.
 * Compatible with ANSI C (C89) standard.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>

/* Test statistics - global counters */
extern int tests_run;
extern int tests_passed;
extern int tests_failed;

/**
 * @brief Assert that a condition is true
 *
 * Increments test counters and prints failure message if condition is false.
 *
 * @param condition Expression to evaluate
 * @param message Description of what is being tested
 */
#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (!(condition)) { \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, message); \
            tests_failed++; \
        } else { \
            tests_passed++; \
        } \
    } while (0)

/**
 * @brief Assert that two values are equal
 *
 * @param expected Expected value
 * @param actual Actual value
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

/**
 * @brief Assert that a pointer is not NULL
 *
 * @param ptr Pointer to check
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

/**
 * @brief Assert that a pointer is NULL
 *
 * @param ptr Pointer to check
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

/**
 * @brief Assert that two strings are equal
 *
 * Requires string.h to be included. Uses strcmp for comparison.
 *
 * @param expected Expected string value
 * @param actual Actual string value
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_STR_EQUAL(expected, actual, message) \
    TEST_ASSERT(strcmp((expected), (actual)) == 0, message)

/**
 * @brief Assert that two values are not equal
 *
 * @param expected Value that should not match
 * @param actual Actual value
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_NOT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) != (actual), message)

/**
 * @brief Assert that a value is greater than a minimum
 *
 * @param value Value to check
 * @param min Minimum threshold (exclusive)
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_GREATER(value, min, message) \
    TEST_ASSERT((value) > (min), message)

/**
 * @brief Assert that a value is within a range
 *
 * @param value Value to check
 * @param min Minimum threshold (inclusive)
 * @param max Maximum threshold (inclusive)
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_RANGE(value, min, max, message) \
    TEST_ASSERT((value) >= (min) && (value) <= (max), message)

/**
 * @brief Assert that a return code indicates success
 *
 * @param result Return code to check (success >= 0)
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_SUCCESS(result, message) \
    TEST_ASSERT((result) >= 0, message)

/**
 * @brief Assert that a return code indicates failure
 *
 * @param result Return code to check (failure < 0)
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_FAILURE(result, message) \
    TEST_ASSERT((result) < 0, message)

/**
 * @brief Assert that a return code matches a specific error code
 *
 * @param result Actual return code
 * @param error_code Expected error code
 * @param message Description of what is being tested
 */
#define TEST_ASSERT_ERROR(result, error_code, message) \
    TEST_ASSERT((result) == (error_code), message)

/**
 * @brief Run a test function
 *
 * Prints test name and executes the test function.
 *
 * @param test_name Name of the test for display
 * @param test_func Function pointer to test function
 */
void run_test(const char* test_name, void (*test_func)(void));

/**
 * @brief Print test execution summary
 *
 * Displays total tests run, passed, failed, and success rate.
 */
void print_test_summary(void);

#endif /* TEST_FRAMEWORK_H */
