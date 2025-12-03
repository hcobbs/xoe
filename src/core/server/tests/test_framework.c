/**
 * @file test_framework.c
 * @brief Implementation of simple C89-compliant unit test framework
 */

#include "test_framework.h"

/* Global test statistics */
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

void run_test(const char* test_name, void (*test_func)(void)) {
    printf("Running: %s\n", test_name);
    test_func();
}

void print_test_summary(void) {
    printf("\n=== Test Summary ===\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    if (tests_run > 0) {
        printf("Success rate: %.1f%%\n", (100.0 * tests_passed) / tests_run);
    } else {
        printf("Success rate: 0.0%%\n");
    }
}
