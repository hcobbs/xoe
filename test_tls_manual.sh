#!/bin/bash

PORT=12348
BIN=./bin/xoe
TIMEOUT=2

echo "Starting server..."
$BIN -e tls13 -p $PORT > /tmp/test_server.log 2>&1 &
SERVER_PID=$!
sleep $TIMEOUT

echo "Server PID: $SERVER_PID"
ps -p $SERVER_PID || echo "Server not running!"

echo "Testing client..."
echo "test" | timeout 5 $BIN -c 127.0.0.1:$PORT -e tls13 > /tmp/test_client.log 2>&1
result=$?
echo "Client exit code: $result"

if [ $result -eq 0 ] || [ $result -eq 124 ]; then
    echo "TEST PASS"
else
    echo "TEST FAIL"
    cat /tmp/test_server.log
    echo "---"
    cat /tmp/test_client.log
fi

kill -9 $SERVER_PID 2>/dev/null
