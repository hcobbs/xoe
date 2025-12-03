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
