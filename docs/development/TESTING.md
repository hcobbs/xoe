# XOE Testing Guide

This document provides comprehensive guidance on testing the XOE (X over Ethernet) project.

## Table of Contents

1. [Overview](#overview)
2. [Running Tests](#running-tests)
3. [Writing Unit Tests](#writing-unit-tests)
4. [Test Framework Reference](#test-framework-reference)
5. [Integration Tests](#integration-tests)
6. [Test Coverage Guidelines](#test-coverage-guidelines)
7. [Debugging Test Failures](#debugging-test-failures)
8. [Test Maintenance](#test-maintenance)
9. [Contributing Tests](#contributing-tests)

---

## Overview

### Test Strategy

The XOE project employs a multi-layered testing strategy:

- **Unit Tests**: Test individual functions and modules in isolation
- **Integration Tests**: Test component interactions and end-to-end workflows
- **Manual Testing**: Server/client interactions, serial port communication

### Test Philosophy

- **Comprehensive Coverage**: All critical modules should have unit tests
- **Test-Driven Quality**: Tests are mandatory for PRs (see Pre-PR Checklist in [CONTRIBUTING.md](CONTRIBUTING.md))
- **Fast Feedback**: Unit tests run in <1 second, integration tests in <10 seconds
- **Maintainable**: Tests should be clear, well-documented, and easy to update

### Current Test Coverage

**Unit Tests (68 tests total)**:
- TLS Security (22 tests): `test_tls_context`, `test_tls_error`, `test_tls_io`, `test_tls_session`
- Serial Buffer (21 tests): `test_serial_buffer`
- Serial Protocol (25 tests): `test_serial_protocol`

**Integration Tests (8 tests)**:
- TCP/TLS modes, concurrent connections, custom ports (via `scripts/test_integration.sh`)

---

## Running Tests

### Quick Reference

```bash
# Most common - run all tests
make test

# Comprehensive validation (build + test)
make check

# Compile tests without running
make test-build

# Run only unit tests
make test-unit

# Run only integration tests
make test-integration

# Verbose output with test suite headers
make test-verbose

# Clean build artifacts
make clean
```

### Makefile Test Targets

#### `make test`
Runs all unit tests followed by integration tests. This is the standard test target.

**Output**:
```
=== Running Unit Tests ===

=== TLS Context Unit Tests ===
Running: test_context_init_valid_tls13
...

=== Running Integration Tests ===
Test 1: Plain TCP mode... ✓ PASS
...
```

#### `make test-build`
Compiles all test binaries without executing them. Useful for verifying tests compile cleanly.

**Use when**: Checking for compilation errors after code changes.

#### `make test-unit`
Runs only unit tests, skipping integration tests.

**Use when**: Quick verification during development, or when integration test dependencies (certificates, etc.) are not available.

#### `make test-integration`
Runs only integration tests, skipping unit tests.

**Use when**: Testing end-to-end workflows or server behavior.

**Requirements**:
- `bin/xoe` binary must be built (`make`)
- Test certificates must exist (run `./scripts/generate_test_certs.sh` if needed)

#### `make test-verbose`
Runs all tests with enhanced formatting and section headers.

**Use when**: Reviewing test results in detail or diagnosing issues.

#### `make check`
Comprehensive validation: builds main binary, compiles all tests, and runs all tests.

**Use when**: Pre-commit validation, CI/CD pipelines, final verification before PR.

This is the **recommended target for thorough validation**.

### Running Individual Test Binaries

Test binaries are located in `bin/test_*`:

```bash
./bin/test_tls_context
./bin/test_tls_error
./bin/test_tls_io
./bin/test_tls_session
./bin/test_serial_buffer
./bin/test_serial_protocol
```

**Example Output**:
```
=== TLS Context Unit Tests ===

Running: test_context_init_valid_tls13
Running: test_context_init_valid_tls12
...

=== Test Summary ===
Tests run:    8
Tests passed: 8
Tests failed: 0
Success rate: 100.0%
```

---

## Writing Unit Tests

### Test File Template

All unit test files follow this structure:

```c
/**
 * @file test_module_name.c
 * @brief Unit tests for module_name
 *
 * Description of what this module does and what aspects are tested.
 *
 * [LLM-ARCH] or [CLASSIC]
 */

#include "tests/framework/test_framework.h"
#include "module/to/test.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Test description
 *
 * Detailed description of what this test verifies.
 */
void test_function_name(void) {
    /* Setup */
    /* ... */

    /* Execute */
    /* ... */

    /* Verify */
    TEST_ASSERT(condition, "Failure message");

    /* Cleanup */
    /* ... */
}

/**
 * @brief Main test runner
 */
int main(void) {
    printf("=== Module Name Unit Tests ===\n\n");

    run_test("test_function_name", test_function_name);
    /* ... more tests ... */

    print_test_summary();

    return (tests_failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

### Test Organization

Group related tests together:

1. **Initialization Tests**: Constructor, configuration, defaults
2. **Basic Functionality Tests**: Core operations, happy path
3. **Edge Cases**: Boundary conditions, limits
4. **Error Handling**: NULL pointers, invalid arguments, failure modes
5. **Resource Management**: Memory leaks, cleanup, destroy operations

### Test Naming Conventions

- Test function: `test_module_operation` (e.g., `test_buffer_write_read`)
- Test groups: `test_module_category_operation` (e.g., `test_buffer_init_default`)
- Descriptive names that indicate what is being tested

### Best Practices

**DO**:
- ✅ Test one thing per test function
- ✅ Use descriptive test names and clear failure messages
- ✅ Document what each test verifies (docstring)
- ✅ Clean up resources (avoid memory leaks in tests)
- ✅ Test both success and failure paths
- ✅ Use appropriate assertion macros (see Framework Reference)

**DON'T**:
- ❌ Test implementation details (test behavior, not internals)
- ❌ Create dependencies between tests (each test should be independent)
- ❌ Leave commented-out or disabled tests without explanation
- ❌ Use magic numbers (define constants for test values)

---

## Test Framework Reference

### Core Assertions

#### `TEST_ASSERT(condition, message)`
Basic assertion - fails if condition is false.

```c
TEST_ASSERT(result == expected, "Result should match expected value");
```

#### `TEST_ASSERT_EQUAL(expected, actual, message)`
Equality check (uses `==`).

```c
TEST_ASSERT_EQUAL(42, get_answer(), "Should return 42");
```

#### `TEST_ASSERT_NOT_EQUAL(expected, actual, message)`
Inequality check (uses `!=`).

```c
TEST_ASSERT_NOT_EQUAL(0, buffer_size, "Buffer size should be non-zero");
```

### Pointer Assertions

#### `TEST_ASSERT_NULL(ptr, message)`
Verifies pointer is NULL.

```c
TEST_ASSERT_NULL(invalid_ptr, "Invalid operation should return NULL");
```

#### `TEST_ASSERT_NOT_NULL(ptr, message)`
Verifies pointer is not NULL.

```c
TEST_ASSERT_NOT_NULL(buffer->data, "Data pointer should be allocated");
```

### String Assertions

#### `TEST_ASSERT_STR_EQUAL(expected, actual, message)`
String comparison using `strcmp()`. **Requires `#include <string.h>`**.

```c
TEST_ASSERT_STR_EQUAL("expected", result_str, "String should match");
```

### Numeric Comparisons

#### `TEST_ASSERT_GREATER(value, min, message)`
Verifies `value > min` (exclusive).

```c
TEST_ASSERT_GREATER(bytes_read, 0, "Should read at least one byte");
```

#### `TEST_ASSERT_RANGE(value, min, max, message)`
Verifies `min <= value <= max` (inclusive).

```c
TEST_ASSERT_RANGE(port, 1, 65535, "Port should be in valid range");
```

### Return Code Assertions

#### `TEST_ASSERT_SUCCESS(result, message)`
Verifies return code indicates success (`result >= 0`).

```c
TEST_ASSERT_SUCCESS(init_result, "Initialization should succeed");
```

#### `TEST_ASSERT_FAILURE(result, message)`
Verifies return code indicates failure (`result < 0`).

```c
TEST_ASSERT_FAILURE(result, "NULL pointer should cause failure");
```

#### `TEST_ASSERT_ERROR(result, error_code, message)`
Verifies specific error code.

```c
TEST_ASSERT_ERROR(result, E_INVALID_ARGUMENT,
                  "Should return E_INVALID_ARGUMENT");
```

### Test Runner Functions

#### `run_test(name, function)`
Executes a test function and tracks results.

```c
run_test("test_basic_operation", test_basic_operation);
```

#### `print_test_summary()`
Prints summary of all test results. Call at end of `main()`.

```c
print_test_summary();
```

### Global Test Counters

These are automatically maintained by the framework:

- `tests_run` - Total number of assertions executed
- `tests_passed` - Number of passing assertions
- `tests_failed` - Number of failing assertions

---

## Integration Tests

### Overview

Integration tests are bash scripts that test end-to-end workflows. The main integration test suite is `scripts/test_integration.sh`.

### What Integration Tests Cover

1. **Server Modes**: Plain TCP (`-e none`), TLS 1.2 (`-e tls12`), TLS 1.3 (`-e tls13`)
2. **Protocol Negotiation**: Ensuring TLS version enforcement works
3. **Concurrent Connections**: Multiple simultaneous clients
4. **Configuration Options**: Custom ports (`-p`), certificates (`-cert`, `-key`)

### Running Integration Tests

```bash
# Via Makefile
make test-integration

# Directly
./scripts/test_integration.sh
```

### Integration Test Requirements

- **Binary**: `bin/xoe` must be built
- **Certificates**: TLS tests require `./certs/server.crt` and `./certs/server.key`
  - Generate with: `./scripts/generate_test_certs.sh`
- **Network**: Tests use `localhost` on various ports (12345, 8080, etc.)
- **OpenSSL**: `openssl s_client` must be available for TLS tests

### Integration Test Output

```
=== XOE Integration Test Suite ===

Test 1: Plain TCP mode (-e none)... ✓ PASS
Test 2: TLS 1.3 mode (-e tls13)... ✓ PASS
...

=== Test Summary ===
Tests run:    8
Tests passed: 8
Tests failed: 0
Success rate: 100.0%
```

### Adding New Integration Tests

Edit `scripts/test_integration.sh` and follow the existing pattern:

```bash
# Test description
echo -n "Test N: Description... "
./bin/xoe <args> &
SERVER_PID=$!
sleep 1

# Test logic
if <test passes>; then
    echo_pass
    ((TESTS_PASSED++))
else
    echo_fail
    ((TESTS_FAILED++))
fi

kill $SERVER_PID 2>/dev/null
((TESTS_RUN++))
```

---

## Test Coverage Guidelines

### Priority Levels

**HIGH PRIORITY** (must have tests):
- Core data structures (buffers, packets, configurations)
- Network I/O operations (send, receive, protocol handling)
- Security modules (TLS context, encryption, authentication)
- Error handling and validation logic

**MEDIUM PRIORITY** (should have tests):
- Utility functions
- Configuration parsing
- State management
- Resource cleanup

**LOW PRIORITY** (optional tests):
- Simple getters/setters with no logic
- Trivial wrappers around library functions
- UI/display-only code

### Coverage Metrics

**Current Coverage**:
- **TLS Security**: 100% (all modules tested)
- **Serial Connector**: 100% (buffer and protocol tested)
- **Core Server**: Integration tests only
- **FSM States**: Partial (via integration tests)

**Goal**: 80%+ coverage for critical modules

### Untested Modules (Future Work)

These modules would benefit from additional unit tests:

- `src/core/server.c` - Multi-threaded server logic
- `src/core/fsm/state_parse_args.c` - Command-line argument parsing
- `src/core/fsm/state_validate_config.c` - Configuration validation
- `src/connectors/serial/serial_client.c` - Serial bridge client coordination

---

## Debugging Test Failures

### Common Issues

#### Issue: Tests fail to compile after refactoring
**Solution**: Check that all test files have updated `#include` paths to match new directory structure.

**Example**:
```c
// OLD (broken after refactoring):
#include "commonDefinitions.h"

// NEW (correct):
#include "lib/common/definitions.h"
```

#### Issue: Integration tests fail
**Symptoms**: "ERROR: Server failed to start" or "ERROR: Binary not found"

**Solutions**:
1. Ensure `bin/xoe` is built: `make`
2. Generate test certificates: `./scripts/generate_test_certs.sh`
3. Check for port conflicts: `lsof -i :12345`

#### Issue: TLS tests fail with certificate errors
**Solution**: Run `./scripts/generate_test_certs.sh` to create test certificates.

**Expected files**:
```
./certs/server.crt
./certs/server.key
```

#### Issue: Unit test hangs or times out
**Cause**: Likely a threading test that blocks indefinitely (e.g., reading from empty buffer without closing it).

**Solution**: Review test logic for proper buffer/resource management. Ensure buffers are closed to unblock waiting threads.

### Known Issues

#### Serial Buffer Test: test_buffer_write_partial (DISABLED)
**Status**: Test is disabled and skipped

**Issue**: The `test_buffer_write_partial` test attempts to write more data than the buffer capacity. However, `serial_buffer_write()` blocks indefinitely when the buffer is full, waiting for a reader thread to consume data (via `pthread_cond_wait`). Without a concurrent reader thread, the test hangs forever.

**Current Solution**: Test has been disabled and renamed to `test_buffer_write_partial_DISABLED`. It now prints a skip notice and passes without actually testing the blocking behavior.

**Future Fix**: Redesign the test to:
- Use a separate reader thread to consume data concurrently
- Test the blocking behavior explicitly with timeouts
- Or test non-blocking scenarios only

**Location**: `src/tests/unit/test_serial_buffer.c:175`

**Tracked**: This is a known limitation documented here and in the test file itself.

### Debugging Techniques

**1. Run single test binary**:
```bash
./bin/test_module_name
```

**2. Add debug output**:
```c
printf("DEBUG: variable value = %d\n", variable);
```

**3. Use gdb**:
```bash
gdb ./bin/test_module_name
(gdb) run
(gdb) backtrace  # if it crashes
```

**4. Check for memory leaks** (macOS):
```bash
leaks --atExit -- ./bin/test_module_name
```

**5. Valgrind** (Linux):
```bash
valgrind --leak-check=full ./bin/test_module_name
```

---

## Test Maintenance

### Keeping Tests in Sync

**When refactoring code**:
1. Update tests FIRST (or simultaneously) with code changes
2. Run `make test-build` frequently to catch compilation errors early
3. Ensure all tests pass before committing: `make check`

**When renaming/moving files**:
1. Update `#include` paths in all test files
2. Update `Makefile` if test file names changed
3. Update this documentation if test organization changed

### Handling Broken Tests

**If tests fail after a change**:
1. **Don't disable the test** - fix it or fix the code
2. If genuinely invalid, remove the test and document why
3. If temporarily broken, add `/* TODO: Fix after X */` comment

**If tests become flaky** (intermittent failures):
1. Investigate timing issues, race conditions, or resource cleanup
2. Add explicit synchronization or timeouts if needed
3. Consider if the test is testing too much (break into smaller tests)

### Performance Considerations

Unit tests should run fast (<1 second total). If tests are slow:
- Reduce test data sizes (use small buffers, not full 16KB)
- Avoid `sleep()` calls or long timeouts
- Consider mocking expensive operations

---

## Contributing Tests

### Before Submitting a PR

Pre-PR checklist (from [CONTRIBUTING.md](CONTRIBUTING.md)):

- [ ] Code builds successfully (`make`)
- [ ] All tests compile (`make test-build`)
- [ ] All tests pass (`make test`)
- [ ] New code has corresponding unit tests
- [ ] Test coverage is comprehensive
- [ ] No compiler warnings (`-Wall -Wextra`)
- [ ] Code follows C89 standards (`-std=c89 -pedantic`)
- [ ] Commits include contribution label (`[LLM-ARCH]`, `[CLASSIC]`, etc.)

### Writing Good Tests

**Characteristics of good tests**:
- **Fast**: Completes in milliseconds
- **Isolated**: No dependencies on other tests or external state
- **Repeatable**: Same result every time
- **Self-documenting**: Clear name and failure message
- **Thorough**: Tests both success and failure paths

**Example of a good test**:
```c
/**
 * @brief Test buffer initialization with custom size
 *
 * Verifies that serial_buffer_init() correctly allocates
 * a buffer with the specified custom capacity.
 */
void test_buffer_init_custom_size(void) {
    serial_buffer_t buffer;
    uint32_t custom_size = 256;

    int result = serial_buffer_init(&buffer, custom_size);

    TEST_ASSERT_SUCCESS(result, "Custom size initialization should succeed");
    TEST_ASSERT_EQUAL(custom_size, serial_buffer_free_space(&buffer),
                      "Buffer should have custom capacity available");

    serial_buffer_destroy(&buffer);
}
```

### Test Documentation

Each test file should have:
- File-level docstring explaining the module under test
- Function-level docstrings for each test explaining what it verifies
- Clear, descriptive test names
- Informative failure messages in assertions

### Getting Help

If you have questions about:
- **Writing tests**: See examples in `src/tests/unit/test_tls_context.c` or `test_serial_protocol.c`
- **Test framework**: See [Test Framework Reference](#test-framework-reference)
- **Integration tests**: See `scripts/test_integration.sh`
- **Debugging**: See [Debugging Test Failures](#debugging-test-failures)

---

## Appendix

### File Locations

```
src/tests/
├── framework/
│   ├── test_framework.h       # Test framework header (assertions, runners)
│   └── test_framework.c       # Test framework implementation
└── unit/
    ├── test_tls_context.c     # TLS context tests
    ├── test_tls_error.c       # TLS error handling tests
    ├── test_tls_io.c          # TLS I/O tests
    ├── test_tls_session.c     # TLS session tests
    ├── test_serial_buffer.c   # Serial buffer tests
    └── test_serial_protocol.c # Serial protocol tests

scripts/
└── test_integration.sh        # Integration test suite

bin/
└── test_*                     # Compiled test binaries
```

### Test Statistics

**Unit Tests**: 68 tests across 6 modules
- TLS Context: 8 tests
- TLS Error: 5 tests
- TLS I/O: 5 tests
- TLS Session: 4 tests
- Serial Buffer: 21 tests
- Serial Protocol: 25 tests

**Integration Tests**: 8 end-to-end scenarios

**Total**: 76 automated tests

### Makefile Targets Summary

| Target | Description | Use When |
|--------|-------------|----------|
| `make test` | Run all tests | Standard testing |
| `make check` | Build + test everything | Pre-commit, CI/CD |
| `make test-build` | Compile tests only | Check compilation |
| `make test-unit` | Run unit tests only | Quick dev feedback |
| `make test-integration` | Run integration tests only | E2E validation |
| `make test-verbose` | Enhanced output | Debugging, detailed review |

---

**Last Updated**: December 2025
**Maintained By**: XOE Development Team
**Related Docs**: [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_REVIEW.md](CODE_REVIEW.md), [Documentation Index](../README.md)
