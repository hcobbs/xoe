#!/bin/bash
#
# Memory Leak Test Suite
#
# Runs XOE server/client under Address Sanitizer to detect memory leaks
# and memory errors during runtime.
#
# Usage: ./scripts/test_memory_leaks.sh
#
# Prerequisites:
# - XOE must be built with ASAN: make asan
# - Test certificates must exist: ./scripts/generate_test_certs.sh
#

set -e  # Exit on error

BIN=./bin/xoe
PORT=12350
TIMEOUT=3

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Check if binary exists
if [ ! -f "$BIN" ]; then
    echo -e "${RED}Error: $BIN not found${NC}"
    echo "Run 'make asan' first to build with Address Sanitizer"
    exit 1
fi

# Check if built with ASAN
if ! nm "$BIN" | grep -q "__asan"; then
    echo -e "${YELLOW}Warning: Binary may not be built with ASAN${NC}"
    echo "Run 'make asan' to rebuild with Address Sanitizer"
fi

# Cleanup function
cleanup() {
    pkill -9 -f "bin/xoe.*$PORT" 2>/dev/null || true
    rm -f /tmp/test_leak_*.log
}

trap cleanup EXIT

echo "========================================"
echo " XOE Memory Leak Test Suite"
echo "========================================"
echo ""

# Test 1: Server startup and shutdown
echo -n "Test 1: Server startup/shutdown (no leaks)... "
TESTS_RUN=$((TESTS_RUN + 1))

$BIN -p $PORT > /tmp/test_leak_1.log 2>&1 &
SERVER_PID=$!
sleep 1

kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

if grep -q "LeakSanitizer" /tmp/test_leak_1.log; then
    echo -e "${RED}✗ FAIL${NC}"
    echo "Memory leaks detected:"
    grep -A 5 "LeakSanitizer" /tmp/test_leak_1.log
    TESTS_FAILED=$((TESTS_FAILED + 1))
else
    echo -e "${GREEN}✓ PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Test 2: Client connect and disconnect
echo -n "Test 2: Client connect/disconnect (no leaks)... "
TESTS_RUN=$((TESTS_RUN + 1))

$BIN -p $PORT > /tmp/test_leak_2_server.log 2>&1 &
SERVER_PID=$!
sleep 1

echo "test" | timeout $TIMEOUT $BIN -c 127.0.0.1:$PORT > /tmp/test_leak_2_client.log 2>&1 || true

kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

if grep -q "LeakSanitizer" /tmp/test_leak_2_server.log || grep -q "LeakSanitizer" /tmp/test_leak_2_client.log; then
    echo -e "${RED}✗ FAIL${NC}"
    echo "Memory leaks detected"
    TESTS_FAILED=$((TESTS_FAILED + 1))
else
    echo -e "${GREEN}✓ PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Test 3: Multiple sequential connections
echo -n "Test 3: Multiple connections (10x) (no leaks)... "
TESTS_RUN=$((TESTS_RUN + 1))

$BIN -p $PORT > /tmp/test_leak_3.log 2>&1 &
SERVER_PID=$!
sleep 1

for i in {1..10}; do
    echo "test $i" | timeout $TIMEOUT $BIN -c 127.0.0.1:$PORT > /dev/null 2>&1 || true
done

kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

if grep -q "LeakSanitizer" /tmp/test_leak_3.log; then
    echo -e "${RED}✗ FAIL${NC}"
    echo "Memory leaks detected after 10 connections"
    TESTS_FAILED=$((TESTS_FAILED + 1))
else
    echo -e "${GREEN}✓ PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Test 4: TLS server startup and shutdown
if [ -f ./certs/server-cert.pem ] && [ -f ./certs/server-key.pem ]; then
    echo -n "Test 4: TLS server startup/shutdown (no leaks)... "
    TESTS_RUN=$((TESTS_RUN + 1))

    $BIN -e tls13 -p $PORT > /tmp/test_leak_4.log 2>&1 &
    SERVER_PID=$!
    sleep 1

    kill $SERVER_PID
    wait $SERVER_PID 2>/dev/null || true

    if grep -q "LeakSanitizer" /tmp/test_leak_4.log; then
        echo -e "${RED}✗ FAIL${NC}"
        echo "Memory leaks detected in TLS mode"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    else
        echo -e "${GREEN}✓ PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    fi
else
    echo -e "${YELLOW}Test 4: TLS test skipped (certificates not found)${NC}"
fi

# Test 5: Management interface memory test
echo -n "Test 5: Management interface (no leaks)... "
TESTS_RUN=$((TESTS_RUN + 1))

$BIN -p $PORT > /tmp/test_leak_5.log 2>&1 &
SERVER_PID=$!
sleep 1

# Connect to management port and send commands
{
    sleep 0.5
    echo "help"
    sleep 0.5
    echo "show status"
    sleep 0.5
    echo "quit"
} | nc localhost 6969 > /dev/null 2>&1 || true

kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

if grep -q "LeakSanitizer" /tmp/test_leak_5.log; then
    echo -e "${RED}✗ FAIL${NC}"
    echo "Memory leaks detected in management interface"
    TESTS_FAILED=$((TESTS_FAILED + 1))
else
    echo -e "${GREEN}✓ PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
fi

# Summary
echo ""
echo "========================================"
echo " Test Summary"
echo "========================================"
echo "Tests run:    $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"
echo "Tests failed: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}Success rate: 100.0%${NC}"
    echo ""
    echo -e "${GREEN}✓ No memory leaks detected${NC}"
    exit 0
else
    SUCCESS_RATE=$(awk "BEGIN {printf \"%.1f\", ($TESTS_PASSED/$TESTS_RUN)*100}")
    echo "Success rate: $SUCCESS_RATE%"
    echo ""
    echo -e "${RED}✗ Memory leaks detected${NC}"
    echo "Check log files in /tmp/test_leak_*.log for details"
    exit 1
fi
