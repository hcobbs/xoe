#!/bin/bash
################################################################################
# Integration Test Suite for XOE Server
#
# Tests all encryption modes (none, TLS 1.2, TLS 1.3) with real network
# connections to verify end-to-end functionality.
#
# Prerequisites:
#   - xoe binary built in ./bin/xoe
#   - Test certificates in ./certs/
#   - OpenSSL command-line tools installed
#   - netcat (nc) installed
################################################################################

set -e

# Configuration
BIN="./bin/xoe"
CERT_DIR="./certs"
PORT=12347  # Non-standard port to avoid conflicts
TIMEOUT=2   # Seconds to wait for server startup

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

################################################################################
# Helper Functions
################################################################################

# Print colored test result
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_RUN=$((TESTS_RUN + 1))
}

# Start server in background and wait for it to be ready
start_server() {
    local args="$@"
    $BIN $args > /dev/null 2>&1 &
    SERVER_PID=$!
    sleep $TIMEOUT

    # Check if server is still running
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}ERROR: Server failed to start${NC}"
        return 1
    fi
    return 0
}

# Stop server gracefully
stop_server() {
    if [ -n "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        SERVER_PID=""
    fi
}

# Cleanup on exit
cleanup() {
    stop_server
}

trap cleanup EXIT

################################################################################
# Pre-flight Checks
################################################################################

echo "=== XOE Integration Test Suite ==="
echo ""

# Check binary exists
if [ ! -f "$BIN" ]; then
    echo -e "${RED}ERROR: Binary not found at $BIN${NC}"
    echo "Run 'make' to build the project first."
    exit 1
fi

# Check certificates exist
if [ ! -f "$CERT_DIR/server.crt" ] || [ ! -f "$CERT_DIR/server.key" ]; then
    echo -e "${YELLOW}WARNING: Test certificates not found${NC}"
    echo "Generating test certificates..."
    ./scripts/generate_test_certs.sh
fi

# Check for required tools
if ! command -v nc &> /dev/null; then
    echo -e "${YELLOW}WARNING: netcat (nc) not found, skipping plain TCP tests${NC}"
    SKIP_NC=1
fi

if ! command -v openssl &> /dev/null; then
    echo -e "${YELLOW}WARNING: openssl not found, skipping TLS tests${NC}"
    SKIP_TLS=1
fi

################################################################################
# Test 1: Plain TCP Mode
################################################################################

if [ -z "$SKIP_NC" ]; then
    echo -n "Test 1: Plain TCP mode (-e none)... "

    if start_server -e none -p $PORT; then
        echo "test" | nc -w 1 localhost $PORT > /dev/null 2>&1
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 1: Plain TCP mode - SKIPPED (nc not available)"
fi

################################################################################
# Test 2: TLS 1.3 Mode
################################################################################

if [ -z "$SKIP_TLS" ]; then
    echo -n "Test 2: TLS 1.3 mode (-e tls13)... "

    if start_server -e tls13 -p $PORT; then
        echo "test" | openssl s_client -connect localhost:$PORT -tls1_3 \
            -CAfile $CERT_DIR/server.crt -quiet > /dev/null 2>&1
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 2: TLS 1.3 mode - SKIPPED (openssl not available)"
fi

################################################################################
# Test 3: TLS 1.2 Mode
################################################################################

if [ -z "$SKIP_TLS" ]; then
    echo -n "Test 3: TLS 1.2 mode (-e tls12)... "

    if start_server -e tls12 -p $PORT; then
        echo "test" | openssl s_client -connect localhost:$PORT -tls1_2 \
            -CAfile $CERT_DIR/server.crt -quiet > /dev/null 2>&1
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 3: TLS 1.2 mode - SKIPPED (openssl not available)"
fi

################################################################################
# Test 4: TLS Version Enforcement (TLS 1.3 server rejects TLS 1.2 client)
################################################################################

if [ -z "$SKIP_TLS" ]; then
    echo -n "Test 4: TLS 1.3 server rejects TLS 1.2 client... "

    if start_server -e tls13 -p $PORT; then
        # This should fail because server enforces TLS 1.3 only
        echo "test" | openssl s_client -connect localhost:$PORT -tls1_2 \
            -CAfile $CERT_DIR/server.crt -quiet > /dev/null 2>&1

        # Invert result - we WANT this to fail
        if [ $? -ne 0 ]; then
            print_result 0
        else
            print_result 1
        fi
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 4: TLS version enforcement - SKIPPED (openssl not available)"
fi

################################################################################
# Test 5: Custom Certificate Path
################################################################################

if [ -z "$SKIP_TLS" ]; then
    echo -n "Test 5: Custom certificate path (-cert/-key)... "

    if start_server -e tls13 -p $PORT -cert $CERT_DIR/server.crt -key $CERT_DIR/server.key; then
        echo "test" | openssl s_client -connect localhost:$PORT -tls1_3 \
            -CAfile $CERT_DIR/server.crt -quiet > /dev/null 2>&1
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 5: Custom certificate path - SKIPPED (openssl not available)"
fi

################################################################################
# Test 6: Concurrent Connections (Plain TCP)
################################################################################

if [ -z "$SKIP_NC" ]; then
    echo -n "Test 6: Concurrent connections (10 clients, plain TCP)... "

    if start_server -e none -p $PORT; then
        # Launch 10 concurrent connections
        for i in {1..10}; do
            echo "Client $i" | nc -w 1 localhost $PORT > /dev/null 2>&1 &
        done

        # Wait for all connections to complete
        wait
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 6: Concurrent connections - SKIPPED (nc not available)"
fi

################################################################################
# Test 7: Concurrent Connections (TLS 1.3)
################################################################################

if [ -z "$SKIP_TLS" ]; then
    echo -n "Test 7: Concurrent connections (10 clients, TLS 1.3)... "

    if start_server -e tls13 -p $PORT; then
        # Launch 10 concurrent TLS connections
        for i in {1..10}; do
            echo "Client $i" | openssl s_client -connect localhost:$PORT -tls1_3 \
                -CAfile $CERT_DIR/server.crt -quiet > /dev/null 2>&1 &
        done

        # Wait for all connections to complete
        wait
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 7: Concurrent TLS connections - SKIPPED (openssl not available)"
fi

################################################################################
# Test 8: Custom Port
################################################################################

if [ -z "$SKIP_NC" ]; then
    echo -n "Test 8: Custom port (-p 8080)... "

    CUSTOM_PORT=8080
    if start_server -e none -p $CUSTOM_PORT; then
        echo "test" | nc -w 1 localhost $CUSTOM_PORT > /dev/null 2>&1
        print_result $?
    else
        print_result 1
    fi

    stop_server
else
    echo "Test 8: Custom port - SKIPPED (nc not available)"
fi

################################################################################
# Test Summary
################################################################################

echo ""
echo "=== Test Summary ==="
echo "Tests run:    $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"
echo "Tests failed: $TESTS_FAILED"

if [ $TESTS_RUN -gt 0 ]; then
    SUCCESS_RATE=$(awk "BEGIN {printf \"%.1f\", 100.0 * $TESTS_PASSED / $TESTS_RUN}")
    echo "Success rate: $SUCCESS_RATE%"
fi

echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
