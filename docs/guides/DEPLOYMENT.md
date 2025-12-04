# XOE Deployment Guide

This guide covers building, configuring, and deploying the XOE server in various environments.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building from Source](#building-from-source)
3. [Configuration](#configuration)
4. [Running the Server](#running-the-server)
5. [Client Connections](#client-connections)
6. [Production Deployment](#production-deployment)
7. [Monitoring and Maintenance](#monitoring-and-maintenance)
8. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### System Requirements

**Supported Operating Systems**:
- Linux (any modern distribution)
- macOS 10.15+
- FreeBSD / OpenBSD / NetBSD
- Any POSIX-compliant Unix

**Windows is NOT supported** (removed as of v1.0)

### Build Dependencies

**Required**:
- GCC 4.x+ or Clang 3.x+ (C89/ANSI C compiler)
- Make (GNU Make or BSD Make)
- OpenSSL 1.1.1+ or 3.x (for TLS support)
- pthread library (usually included with system)

**Optional**:
- valgrind (for memory leak testing)
- gdb (for debugging)

### Installing Dependencies

#### Debian / Ubuntu
```bash
sudo apt-get update
sudo apt-get install build-essential libssl-dev
```

#### Red Hat / CentOS / Fedora
```bash
sudo yum install gcc make openssl-devel
# or on newer versions:
sudo dnf install gcc make openssl-devel
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew OpenSSL
brew install openssl@3
```

#### FreeBSD
```bash
sudo pkg install gcc gmake openssl
```

---

## Building from Source

### Quick Build

```bash
# Clone repository (if from git)
git clone <repository-url> xoe
cd xoe

# Build
make

# Clean and rebuild
make clean
make
```

### Build Output

```
bin/xoe         # Executable
obj/*.o         # Object files (intermediate)
```

### Build Options

**Clean Build**:
```bash
make clean && make
```

**Verbose Build**:
```bash
make V=1
```

**Debug Build** (default includes `-g`):
```bash
# Already includes debug symbols
# To disable optimization:
make CFLAGS="-Wall -Wextra -g -std=c89 -pedantic -O0"
```

**Release Build** (optimized):
```bash
make CFLAGS="-Wall -Wextra -std=c89 -pedantic -O2 -DNDEBUG"
```

### Verification

```bash
# Check build
ls -lh bin/xoe

# Verify dependencies
ldd bin/xoe          # Linux
otool -L bin/xoe     # macOS

# Test execution
./bin/xoe -h
```

---

## Configuration

### Certificate Setup

#### Development (Self-Signed)

```bash
# Generate self-signed certificates
./scripts/generate_test_certs.sh

# Certificates created at:
#   ./certs/server.crt
#   ./certs/server.key
```

**Security Warning**: Self-signed certificates are for development only. Clients must disable certificate verification or manually trust the certificate.

#### Production (Let's Encrypt)

```bash
# Install certbot
sudo apt-get install certbot  # Debian/Ubuntu
brew install certbot          # macOS

# Obtain certificate (requires public domain)
sudo certbot certonly --standalone -d yourdomain.com

# Certificates will be at:
# /etc/letsencrypt/live/yourdomain.com/fullchain.pem
# /etc/letsencrypt/live/yourdomain.com/privkey.pem

# Set permissions
sudo chmod 644 /etc/letsencrypt/live/yourdomain.com/fullchain.pem
sudo chmod 600 /etc/letsencrypt/live/yourdomain.com/privkey.pem
sudo chown xoe:xoe /etc/letsencrypt/live/yourdomain.com/privkey.pem
```

#### Production (Commercial CA)

```bash
# Generate CSR
openssl req -new -newkey rsa:2048 -nodes \
  -keyout server.key -out server.csr \
  -subj "/C=US/ST=State/L=City/O=YourOrg/CN=yourdomain.com"

# Submit server.csr to CA
# Receive signed certificate (server.crt)

# Install certificates
mkdir -p /etc/xoe/certs
cp server.crt /etc/xoe/certs/
cp server.key /etc/xoe/certs/
chmod 644 /etc/xoe/certs/server.crt
chmod 600 /etc/xoe/certs/server.key
```

### File Permissions

**Critical**: Private keys must be readable only by server process

```bash
# Correct permissions
chmod 600 /path/to/server.key
chmod 644 /path/to/server.crt
chown xoe:xoe /path/to/server.key
```

---

## Running the Server

### Command-Line Options

```
Usage: xoe [OPTIONS]

Server Mode Options:
  -i <interface>    Network interface to bind (default: 0.0.0.0, all interfaces)
                    Examples: 127.0.0.1, eth0, 192.168.1.100

  -p <port>         Port to listen on (default: 12345)
                    Range: 1-65535

  -e <mode>         Encryption mode (default: none)
                    none  - Plain TCP (no encryption)
                    tls12 - TLS 1.2 encryption
                    tls13 - TLS 1.3 encryption (recommended)

  -cert <path>      Path to server certificate (default: ./certs/server.crt)
                    Required for TLS modes

  -key <path>       Path to server private key (default: ./certs/server.key)
                    Required for TLS modes

Client Mode Options:
  -c <ip>:<port>    Connect to server as client
                    Example: -c 192.168.1.100:12345

General Options:
  -h                Show this help message
```

### Server Mode Examples

#### Plain TCP (No Encryption)
```bash
# Listen on all interfaces, port 12345
./bin/xoe -e none

# Listen on specific interface and port
./bin/xoe -e none -i 127.0.0.1 -p 8080

# Listen on specific network interface
./bin/xoe -e none -i eth0 -p 12345
```

#### TLS 1.3 (Recommended)
```bash
# Default certificates
./bin/xoe -e tls13

# Custom certificates
./bin/xoe -e tls13 \
  -cert /etc/letsencrypt/live/yourdomain.com/fullchain.pem \
  -key /etc/letsencrypt/live/yourdomain.com/privkey.pem

# Custom port with TLS
./bin/xoe -e tls13 -p 8443 \
  -cert /etc/xoe/certs/server.crt \
  -key /etc/xoe/certs/server.key
```

#### TLS 1.2 (Legacy Compatibility)
```bash
# Use TLS 1.2 for older clients
./bin/xoe -e tls12 \
  -cert /etc/xoe/certs/server.crt \
  -key /etc/xoe/certs/server.key
```

### Client Mode Examples

#### Plain TCP Client
```bash
# Connect to server
./bin/xoe -c 127.0.0.1:12345

# Connect to remote server
./bin/xoe -c 192.168.1.100:8080
```

**Note**: Client mode currently supports plain TCP only. TLS client mode is not yet implemented.

### Background Execution

#### Using nohup
```bash
nohup ./bin/xoe -e tls13 -p 12345 > /var/log/xoe.log 2>&1 &
echo $! > /var/run/xoe.pid
```

#### Using screen
```bash
screen -S xoe
./bin/xoe -e tls13 -p 12345
# Press Ctrl+A, then D to detach
# Reattach: screen -r xoe
```

#### Using tmux
```bash
tmux new -s xoe
./bin/xoe -e tls13 -p 12345
# Press Ctrl+B, then D to detach
# Reattach: tmux attach -t xoe
```

---

## Client Connections

### Testing with OpenSSL s_client

#### TLS 1.3 Server
```bash
# Connect and test echo
echo "Hello World" | openssl s_client -connect localhost:12345 -tls1_3 -CAfile ./certs/server.crt -quiet

# Interactive session
openssl s_client -connect localhost:12345 -tls1_3 -CAfile ./certs/server.crt

# Skip certificate verification (insecure, testing only)
openssl s_client -connect localhost:12345 -tls1_3 -verify_return_error=0
```

#### TLS 1.2 Server
```bash
echo "Test message" | openssl s_client -connect localhost:12345 -tls1_2 -CAfile ./certs/server.crt -quiet
```

#### Plain TCP Server
```bash
# Using telnet
telnet localhost 12345

# Using netcat
nc localhost 12345

# Using OpenSSL (as plain TCP)
openssl s_client -connect localhost:12345 -no_ssl
```

### Testing with curl

```bash
# If xoe implements HTTP-like protocol
curl -k https://localhost:12345/

# With certificate verification
curl --cacert ./certs/server.crt https://localhost:12345/
```

### Testing with Custom Clients

**Python Example**:
```python
import socket
import ssl

# Plain TCP
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 12345))
sock.send(b"Hello from Python\n")
print(sock.recv(1024).decode())
sock.close()

# TLS connection
context = ssl.create_default_context()
context.check_hostname = False
context.verify_mode = ssl.CERT_NONE  # For self-signed certs

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tls_sock = context.wrap_socket(sock, server_hostname='localhost')
tls_sock.connect(('localhost', 12345))
tls_sock.send(b"Hello from Python TLS\n")
print(tls_sock.recv(1024).decode())
tls_sock.close()
```

---

## Production Deployment

### User and Group Setup

```bash
# Create dedicated user (no login shell)
sudo useradd -r -s /bin/false -d /var/lib/xoe -m xoe

# Create directories
sudo mkdir -p /etc/xoe/certs
sudo mkdir -p /var/log/xoe
sudo mkdir -p /var/run/xoe

# Set ownership
sudo chown -R xoe:xoe /var/lib/xoe /var/log/xoe /var/run/xoe
sudo chown -R root:xoe /etc/xoe
```

### Installation

```bash
# Install binary
sudo install -m 755 bin/xoe /usr/local/bin/

# Install certificates
sudo cp certs/server.crt /etc/xoe/certs/
sudo cp certs/server.key /etc/xoe/certs/
sudo chmod 644 /etc/xoe/certs/server.crt
sudo chmod 600 /etc/xoe/certs/server.key
sudo chown root:xoe /etc/xoe/certs/server.key
```

### Systemd Service (Linux)

Create `/etc/systemd/system/xoe.service`:

```ini
[Unit]
Description=XOE Server
After=network.target

[Service]
Type=simple
User=xoe
Group=xoe
ExecStart=/usr/local/bin/xoe -e tls13 -p 12345 \
  -cert /etc/xoe/certs/server.crt \
  -key /etc/xoe/certs/server.key
Restart=on-failure
RestartSec=5s

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/xoe /var/run/xoe

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=xoe

[Install]
WantedBy=multi-user.target
```

**Enable and start**:
```bash
sudo systemctl daemon-reload
sudo systemctl enable xoe
sudo systemctl start xoe
sudo systemctl status xoe
```

### Init Script (BSD / Traditional Unix)

Create `/etc/rc.d/xoe` or `/etc/init.d/xoe`:

```bash
#!/bin/sh
# chkconfig: 345 99 01
# description: XOE Server

NAME=xoe
DAEMON=/usr/local/bin/xoe
PIDFILE=/var/run/xoe/xoe.pid
USER=xoe

DAEMON_OPTS="-e tls13 -p 12345 -cert /etc/xoe/certs/server.crt -key /etc/xoe/certs/server.key"

case "$1" in
  start)
    echo "Starting $NAME..."
    start-stop-daemon --start --quiet --pidfile $PIDFILE \
      --make-pidfile --background --chuid $USER \
      --exec $DAEMON -- $DAEMON_OPTS
    ;;
  stop)
    echo "Stopping $NAME..."
    start-stop-daemon --stop --quiet --pidfile $PIDFILE
    rm -f $PIDFILE
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac

exit 0
```

**Enable and start**:
```bash
sudo chmod +x /etc/init.d/xoe
sudo update-rc.d xoe defaults  # Debian/Ubuntu
sudo chkconfig xoe on          # Red Hat/CentOS
sudo service xoe start
```

### Firewall Configuration

#### iptables (Linux)
```bash
# Allow XOE port
sudo iptables -A INPUT -p tcp --dport 12345 -j ACCEPT

# Rate limiting (prevent DoS)
sudo iptables -A INPUT -p tcp --dport 12345 -m limit --limit 25/minute --limit-burst 100 -j ACCEPT

# Save rules
sudo iptables-save > /etc/iptables/rules.v4
```

#### firewalld (RHEL/CentOS)
```bash
sudo firewall-cmd --permanent --add-port=12345/tcp
sudo firewall-cmd --reload
```

#### ufw (Ubuntu)
```bash
sudo ufw allow 12345/tcp
sudo ufw enable
```

#### pf (BSD/macOS)
Add to `/etc/pf.conf`:
```
pass in proto tcp from any to any port 12345
```

Reload:
```bash
sudo pfctl -f /etc/pf.conf
```

### Reverse Proxy Setup

#### nginx
```nginx
upstream xoe_backend {
    server 127.0.0.1:12345;
}

server {
    listen 443 ssl http2;
    server_name yourdomain.com;

    ssl_certificate /etc/letsencrypt/live/yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/yourdomain.com/privkey.pem;

    location / {
        proxy_pass https://xoe_backend;
        proxy_ssl_verify off;  # XOE handles TLS
    }
}
```

---

## Monitoring and Maintenance

### Health Checks

```bash
# Check if server is listening
netstat -tuln | grep 12345
# or
ss -tuln | grep 12345

# Check process
ps aux | grep xoe
pgrep -fa xoe

# Test connection
echo "ping" | nc localhost 12345
echo "ping" | openssl s_client -connect localhost:12345 -tls1_3 -CAfile ./certs/server.crt -quiet
```

### Log Monitoring

```bash
# Systemd logs
sudo journalctl -u xoe -f

# Traditional logs
sudo tail -f /var/log/xoe.log

# Filter for errors
sudo journalctl -u xoe | grep -i error
```

### Performance Monitoring

```bash
# CPU and memory usage
top -p $(pgrep xoe)

# Network connections
netstat -an | grep 12345 | wc -l

# Detailed connection info
ss -tnp | grep xoe
```

### Certificate Expiration Monitoring

```bash
# Check certificate expiration
openssl x509 -in /etc/xoe/certs/server.crt -noout -dates

# Days until expiration
echo $(( ($(date -d "$(openssl x509 -in /etc/xoe/certs/server.crt -noout -enddate | cut -d= -f2)" +%s) - $(date +%s)) / 86400 ))

# Alert if expiring soon
#!/bin/bash
CERT_FILE="/etc/xoe/certs/server.crt"
DAYS_WARNING=30

EXPIRY=$(openssl x509 -in $CERT_FILE -noout -enddate | cut -d= -f2)
EXPIRY_EPOCH=$(date -d "$EXPIRY" +%s)
NOW_EPOCH=$(date +%s)
DAYS_LEFT=$(( ($EXPIRY_EPOCH - $NOW_EPOCH) / 86400 ))

if [ $DAYS_LEFT -lt $DAYS_WARNING ]; then
    echo "WARNING: Certificate expires in $DAYS_LEFT days!"
    # Send alert email
    mail -s "XOE Certificate Expiring" admin@example.com <<< "Certificate expires in $DAYS_LEFT days"
fi
```

### Backup Procedures

```bash
# Backup certificates
sudo tar czf xoe-certs-backup-$(date +%Y%m%d).tar.gz /etc/xoe/certs/

# Backup configuration
sudo cp /etc/systemd/system/xoe.service /backup/xoe.service.$(date +%Y%m%d)

# Secure backup (encrypted)
sudo tar czf - /etc/xoe/certs/ | openssl enc -aes-256-cbc -salt -out xoe-backup-$(date +%Y%m%d).tar.gz.enc
```

### Certificate Renewal

```bash
# Let's Encrypt (automatic)
sudo certbot renew --quiet --deploy-hook "systemctl reload xoe"

# Manual renewal
./scripts/generate_test_certs.sh
sudo cp certs/server.crt /etc/xoe/certs/
sudo cp certs/server.key /etc/xoe/certs/
sudo systemctl restart xoe
```

---

## Troubleshooting

### Common Issues

#### Issue: "Failed to initialize TLS"

**Cause**: Certificate or key file not found

**Solution**:
```bash
# Check files exist
ls -l ./certs/server.crt ./certs/server.key

# Generate if missing
./scripts/generate_test_certs.sh

# Check paths in command
./bin/xoe -e tls13 -cert ./certs/server.crt -key ./certs/server.key
```

#### Issue: "Permission denied" on port < 1024

**Cause**: Non-root user cannot bind to privileged ports (1-1023)

**Solution 1** - Use higher port:
```bash
./bin/xoe -e tls13 -p 12345  # Port > 1024
```

**Solution 2** - Grant capability (Linux):
```bash
sudo setcap cap_net_bind_service=+ep /usr/local/bin/xoe
./bin/xoe -e tls13 -p 443
```

**Solution 3** - Use reverse proxy (nginx/haproxy on port 443)

#### Issue: "Address already in use"

**Cause**: Another process is using the port

**Solution**:
```bash
# Find process using port
sudo lsof -i :12345
sudo fuser 12345/tcp

# Kill process
sudo kill $(sudo fuser 12345/tcp 2>/dev/null)

# Or use different port
./bin/xoe -e tls13 -p 12346
```

#### Issue: "TLS handshake failed"

**Cause**: Client and server TLS version mismatch

**Solution**:
```bash
# Server is TLS 1.3, client must use -tls1_3
openssl s_client -connect localhost:12345 -tls1_3

# Or switch server to TLS 1.2
./bin/xoe -e tls12
```

#### Issue: "Private key does not match certificate"

**Cause**: Certificate and key are from different generations

**Solution**:
```bash
# Regenerate matching pair
./scripts/generate_test_certs.sh

# Verify match
openssl x509 -noout -modulus -in server.crt | openssl md5
openssl rsa -noout -modulus -in server.key | openssl md5
# Output should be identical
```

### Debug Mode

```bash
# Run in foreground with verbose output
./bin/xoe -e tls13

# Use gdb for debugging
gdb --args ./bin/xoe -e tls13 -p 12345

# Use valgrind for memory leaks
valgrind --leak-check=full ./bin/xoe -e none -p 12345
```

### Logging

```bash
# Redirect output to log file
./bin/xoe -e tls13 2>&1 | tee /var/log/xoe.log

# Increase OpenSSL verbosity (requires code change)
# In tls_error.c, use ERR_print_errors_fp(stderr)
```

---

## Performance Tuning

### System Limits

```bash
# Increase open file limit
ulimit -n 65535

# Or set in /etc/security/limits.conf
xoe soft nofile 65535
xoe hard nofile 65535

# Increase connection backlog
# Edit /etc/sysctl.conf
net.core.somaxconn = 1024
net.ipv4.tcp_max_syn_backlog = 2048

# Apply
sudo sysctl -p
```

### XOE Configuration

Current limits (compile-time, edit `xoe.h`):
```c
#define MAX_CLIENTS 32        // Maximum concurrent connections
#define BUFFER_SIZE 1024      // Buffer size per connection
```

To change, edit and recompile:
```bash
# Edit src/core/config.h
#define MAX_CLIENTS 128

# Rebuild
make clean && make
```

---

## Security Checklist

Before going to production:

- [ ] Use TLS 1.3 encryption (`-e tls13`)
- [ ] Obtain CA-signed certificate (not self-signed)
- [ ] Set private key permissions to 600
- [ ] Run as non-root user
- [ ] Configure firewall rules
- [ ] Enable SELinux/AppArmor (if available)
- [ ] Setup log monitoring
- [ ] Configure certificate expiration alerts
- [ ] Setup automated certificate renewal
- [ ] Test disaster recovery procedures
- [ ] Document deployment for team

---

## Support

For issues, questions, or contributions:
- GitHub Issues: (see README.md)
- Documentation: SECURITY.md, CODE_REVIEW.md
- Email: (see README.md)

---

**Last Updated**: 2025-12-02
**Version**: 1.0
