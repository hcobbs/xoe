#!/bin/bash
# Test management interface

exec 3<>/dev/tcp/localhost/6969

# Read welcome
read -u 3 welcome
echo "Server: $welcome"

# Send help
echo "help" >&3
sleep 0.2
while read -t 0.1 -u 3 line; do
    echo "$line"
done

# Send show status
echo "show status" >&3
sleep 0.2
while read -t 0.1 -u 3 line; do
    echo "$line"
done

# Send set commands
echo "set port 9999" >&3
sleep 0.1
read -u 3 line && echo "$line"

echo "set mode client" >&3
sleep 0.1
read -u 3 line && echo "$line"

# Check pending
echo "pending" >&3
sleep 0.2
while read -t 0.1 -u 3 line; do
    echo "$line"
done

# Validate
echo "validate" >&3
sleep 0.2
while read -t 0.1 -u 3 line; do
    echo "$line"
done

# Show status again
echo "show status" >&3
sleep 0.2
while read -t 0.1 -u 3 line; do
    echo "$line"
done

# Quit
echo "quit" >&3

exec 3>&-
