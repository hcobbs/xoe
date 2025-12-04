# Security Guidelines for XOE

This document provides security guidelines for deploying and using XOE with TLS encryption.

---

## Table of Contents

1. [Encryption Modes](#encryption-modes)
2. [Certificate Management](#certificate-management)
3. [TLS Configuration](#tls-configuration)
4. [Key Management](#key-management)
5. [Security Best Practices](#security-best-practices)
6. [Threat Model](#threat-model)
7. [Vulnerability Reporting](#vulnerability-reporting)

---

## Encryption Modes

XOE supports three runtime-selectable encryption modes:

### None (Plain TCP)
```bash
./bin/xoe -e none
```

**Use Cases**:
- Development and testing
- Trusted networks (e.g., localhost, VPN)
- Performance-critical applications where encryption is handled at another layer

**Security**: ⚠️ **NO ENCRYPTION** - All data transmitted in plaintext

**Risks**:
- Eavesdropping: Anyone on the network can read transmitted data
- Man-in-the-middle attacks: Attackers can modify data in transit
- No authentication: Cannot verify server identity

**Recommendation**: Only use on localhost or fully trusted networks

### TLS 1.2
```bash
./bin/xoe -e tls12
```

**Use Cases**:
- Legacy client compatibility
- Systems that cannot support TLS 1.3
- Transitional deployments

**Security**: ✅ Encrypted with strong ciphers

**Limitations**:
- Older protocol with known weaknesses (mitigated by cipher selection)
- More complex handshake (higher latency)
- Less forward secrecy compared to TLS 1.3

**Recommendation**: Use TLS 1.3 unless legacy compatibility is required

### TLS 1.3 (Recommended)
```bash
./bin/xoe -e tls13
```

**Use Cases**:
- Production deployments
- High-security environments
- Default choice for all new deployments

**Security**: ✅ **RECOMMENDED** - Modern encryption with maximum security

**Advantages**:
- Faster handshake (reduced latency)
- Stronger forward secrecy
- Simplified cipher suite negotiation
- Encrypted handshake (protects metadata)
- No known major vulnerabilities

**Cipher Suites** (in order of preference):
1. `TLS_AES_256_GCM_SHA384` - 256-bit AES with GCM mode
2. `TLS_AES_128_GCM_SHA256` - 128-bit AES with GCM mode
3. `TLS_CHACHA20_POLY1305_SHA256` - ChaCha20-Poly1305 for ARM/mobile

All cipher suites provide:
- Authenticated encryption (prevents tampering)
- Forward secrecy (past sessions remain secure even if key is compromised)
- Protection against replay attacks

---

## Certificate Management

### Development Certificates

**Generation** (self-signed):
```bash
./scripts/generate_test_certs.sh
```

This creates:
- `./certs/server.crt` - Self-signed certificate (valid 365 days)
- `./certs/server.key` - Private key (2048-bit RSA, no passphrase)

**Security**: ⚠️ **DEVELOPMENT ONLY**
- Self-signed certificates are not trusted by default
- Clients must explicitly disable verification or add certificate to trust store
- Private key is generated without passphrase

**Testing with OpenSSL Client**:
```bash
# Accept self-signed certificate
openssl s_client -connect localhost:12345 -tls1_3 -CAfile ./certs/server.crt

# Or skip verification (insecure)
openssl s_client -connect localhost:12345 -tls1_3 -verify_return_error=0
```

### Production Certificates

**Obtaining CA-Signed Certificates**:

#### Option 1: Let's Encrypt (Free, Automated)
```bash
# Install certbot
apt-get install certbot  # Debian/Ubuntu
brew install certbot      # macOS

# Obtain certificate (requires domain name)
sudo certbot certonly --standalone -d yourdomain.com

# Certificates will be at:
# /etc/letsencrypt/live/yourdomain.com/fullchain.pem  <- Certificate
# /etc/letsencrypt/live/yourdomain.com/privkey.pem    <- Private key

# Run xoe with Let's Encrypt certificates
./bin/xoe -e tls13 \
  -cert /etc/letsencrypt/live/yourdomain.com/fullchain.pem \
  -key /etc/letsencrypt/live/yourdomain.com/privkey.pem
```

**Auto-renewal**:
```bash
# Test renewal
sudo certbot renew --dry-run

# Setup cron job for auto-renewal
0 0 * * * certbot renew --quiet --deploy-hook "systemctl reload xoe"
```

#### Option 2: Commercial CA (DigiCert, Sectigo, etc.)

1. Generate Certificate Signing Request (CSR):
   ```bash
   openssl req -new -newkey rsa:2048 -nodes \
     -keyout server.key -out server.csr \
     -subj "/C=US/ST=State/L=City/O=Organization/CN=yourdomain.com"
   ```

2. Submit CSR to CA and obtain signed certificate

3. Install certificate:
   ```bash
   ./bin/xoe -e tls13 -cert /path/to/server.crt -key /path/to/server.key
   ```

#### Option 3: Internal CA (Enterprise)

For internal networks, set up your own Certificate Authority:
```bash
# Create CA
openssl genrsa -out ca.key 4096
openssl req -new -x509 -key ca.key -out ca.crt -days 3650

# Sign server certificate with CA
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 365
```

Distribute `ca.crt` to all clients for validation.

### Certificate Validation

**Recommended Settings**:
- **Key Size**: Minimum 2048-bit RSA (4096-bit for high security)
- **Validity Period**: 90 days (automated renewal) or 1 year maximum
- **Subject Alternative Names (SAN)**: Include all hostnames/IPs
- **Extended Key Usage**: Server Authentication

**Verify Certificate**:
```bash
# Check certificate details
openssl x509 -in server.crt -text -noout

# Verify certificate chain
openssl verify -CAfile ca.crt server.crt

# Check key matches certificate
openssl x509 -noout -modulus -in server.crt | openssl md5
openssl rsa -noout -modulus -in server.key | openssl md5
# Output should match
```

---

## TLS Configuration

### Cipher Suite Selection

XOE uses a hardcoded list of secure TLS 1.3 cipher suites (cannot be changed at runtime):

```c
TLS_AES_256_GCM_SHA384          // Strongest, highest overhead
TLS_AES_128_GCM_SHA256          // Fast, still very secure
TLS_CHACHA20_POLY1305_SHA256    // Best for ARM/mobile
```

**Rationale**:
- All suites provide authenticated encryption
- All suites provide forward secrecy
- No weak or deprecated ciphers
- Ordered by security strength

**Customization** (requires recompilation):
Edit `src/lib/security/tls_config.h`:
```c
#define TLS_CIPHER_SUITES "TLS_AES_256_GCM_SHA384:..."
```

### Protocol Version Selection

**At Runtime**:
```bash
./bin/xoe -e tls13   # TLS 1.3 only (recommended)
./bin/xoe -e tls12   # TLS 1.2 only (legacy)
```

**Version Negotiation**:
- When set to TLS 1.3, server rejects TLS 1.2 and below
- When set to TLS 1.2, server rejects TLS 1.3 and above
- No version downgrade attacks possible

### Session Caching

**Configuration** (compile-time):
```c
#define TLS_SESSION_TIMEOUT 300  // 5 minutes
```

**Purpose**: Allows session resumption for improved performance

**Security Considerations**:
- Session IDs are cryptographically random
- Sessions expire after timeout
- Session cache is per-process (not shared across servers)

**Disable** (requires code change):
```c
/* In src/lib/security/tls_context.c, comment out: */
// SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
// SSL_CTX_set_timeout(ctx, TLS_SESSION_TIMEOUT);
```

### Client Certificate Verification

**Current Status**: ⚠️ NOT IMPLEMENTED

Client certificate verification is disabled by default:
```c
#define TLS_DEFAULT_VERIFY_MODE TLS_VERIFY_NONE
```

**Future Implementation** (requires code changes):
- Add `-client-ca <file>` parameter
- Set `SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL)`
- Load CA certificate for client verification

---

## Key Management

### Private Key Security

**File Permissions** (CRITICAL):
```bash
# Private key should be readable only by server process
chmod 600 /path/to/server.key
chown xoe:xoe /path/to/server.key

# Certificate can be world-readable
chmod 644 /path/to/server.crt
```

**Storage Recommendations**:
- ✅ Store on encrypted filesystem
- ✅ Use hardware security module (HSM) for high-security deployments
- ✅ Restrict access via file permissions and SELinux/AppArmor
- ❌ Never commit keys to version control
- ❌ Never log key contents
- ❌ Never transmit keys over unencrypted channels

### Key Rotation

**Frequency**:
- **Production**: Every 90 days (automated with Let's Encrypt)
- **High Security**: Every 30 days
- **After Breach**: Immediately

**Rotation Process**:
1. Generate new certificate/key pair
2. Test new certificate with staging server
3. Update xoe configuration
4. Restart xoe server
5. Verify TLS connection works
6. Securely delete old private key

**Zero-Downtime Rotation** (requires process management):
```bash
# Generate new certificate
./scripts/generate_test_certs.sh

# Start new xoe instance on different port
./bin/xoe -e tls13 -p 12346 -cert ./certs/server.crt -key ./certs/server.key &

# Update load balancer to route to new instance
# Stop old instance
# Update load balancer to use original port
```

### Key Backup

**Recommendations**:
- ✅ Backup private keys to encrypted storage
- ✅ Store backups offline or in separate security zone
- ✅ Document key recovery procedures
- ⚠️ Limit access to backups (key custodians only)
- ❌ Never backup to cloud storage without encryption

---

## Security Best Practices

### Deployment Checklist

**Before Production**:
- [ ] Use TLS 1.3 mode (`-e tls13`)
- [ ] Obtain CA-signed certificate (not self-signed)
- [ ] Set private key permissions to 600
- [ ] Run server as non-root user (drop privileges)
- [ ] Configure firewall to allow only necessary ports
- [ ] Enable system logging for TLS errors
- [ ] Test certificate expiration monitoring
- [ ] Document certificate rotation procedures
- [ ] Setup automated certificate renewal (Let's Encrypt)
- [ ] Verify client connections with `openssl s_client`

### Network Security

**Firewall Configuration**:
```bash
# Example: iptables rules
iptables -A INPUT -p tcp --dport 12345 -m state --state NEW -j ACCEPT
iptables -A INPUT -p tcp --dport 12345 -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# Rate limiting to prevent DoS
iptables -A INPUT -p tcp --dport 12345 -m limit --limit 25/minute --limit-burst 100 -j ACCEPT
```

**Intrusion Detection**:
- Monitor for repeated TLS handshake failures (potential attack)
- Alert on certificate expiration (30 days before)
- Log all connection attempts with timestamps

### Process Isolation

**Run as Dedicated User**:
```bash
# Create dedicated user
useradd -r -s /bin/false xoe

# Run server as xoe user
sudo -u xoe ./bin/xoe -e tls13
```

**Chroot Jail** (advanced):
```bash
# Create minimal chroot environment
mkdir -p /var/chroot/xoe/{bin,lib,certs}
cp bin/xoe /var/chroot/xoe/bin/
ldd bin/xoe  # Copy required libraries to lib/
cp certs/* /var/chroot/xoe/certs/

# Run in chroot
chroot /var/chroot/xoe /bin/xoe -e tls13
```

### Monitoring and Logging

**Critical Events to Log**:
- TLS handshake failures (potential attack)
- Certificate load errors (configuration issue)
- Client disconnections (for forensics)
- OpenSSL errors (for debugging)

**Log Analysis**:
```bash
# Monitor TLS errors
tail -f /var/log/xoe.log | grep "TLS"

# Count handshake failures
grep "TLS handshake failed" /var/log/xoe.log | wc -l

# Alert on repeated failures from same IP
awk '/TLS handshake failed/ {print $NF}' /var/log/xoe.log | sort | uniq -c | sort -rn
```

### Security Updates

**OpenSSL Updates**:
- XOE depends on OpenSSL 1.1.1+ or 3.x
- Monitor OpenSSL security advisories: https://www.openssl.org/news/
- Apply security patches promptly

**Update Process**:
```bash
# Check OpenSSL version
openssl version

# Update OpenSSL (Debian/Ubuntu)
apt-get update && apt-get upgrade openssl libssl-dev

# Update OpenSSL (macOS)
brew upgrade openssl

# Rebuild xoe after OpenSSL update
make clean && make
```

---

## Threat Model

### In-Scope Threats

**XOE with TLS 1.3 protects against**:
- ✅ Eavesdropping (passive network monitoring)
- ✅ Man-in-the-middle attacks (with proper certificate validation)
- ✅ Replay attacks (TLS 1.3 built-in protection)
- ✅ Tampering (authenticated encryption)
- ✅ Protocol downgrade attacks (version pinning)

### Out-of-Scope Threats

**XOE does NOT protect against**:
- ❌ Compromised client or server (malware)
- ❌ Side-channel attacks (timing, power analysis)
- ❌ Denial of service (application-layer DoS)
- ❌ Social engineering
- ❌ Insider threats (malicious operator)
- ❌ Vulnerabilities in application logic (beyond TLS)

### Assumptions

**Security Depends On**:
- Private key remains confidential
- Certificate authority is trustworthy
- OpenSSL library is correctly implemented
- Operating system is secure and patched
- System time is accurate (for certificate validation)

---

## Vulnerability Reporting

### Reporting a Security Issue

**DO NOT** open a public GitHub issue for security vulnerabilities.

**Instead**:
1. Email security contact (see README.md)
2. Include detailed description of vulnerability
3. Provide proof-of-concept if possible
4. Allow 90 days for fix before public disclosure

**We will**:
- Acknowledge receipt within 48 hours
- Provide estimated fix timeline
- Credit reporter (unless anonymity requested)
- Coordinate disclosure timing

### Known Limitations

**Current Version** (as of 2025-12-02):
1. No client certificate verification
2. No certificate revocation checking (OCSP/CRL)
3. No session ticket support (TLS 1.3 0-RTT)
4. Fixed cipher suite list (not runtime configurable)
5. Blocking I/O only (vulnerable to slowloris DoS)
6. Thread-per-client model (limited scalability)

**Future Improvements**:
- Add OCSP stapling for certificate status
- Implement rate limiting at TLS layer
- Add support for TLS 1.3 0-RTT resumption
- Implement non-blocking I/O with event loop

---

## References

- **TLS 1.3 Specification**: RFC 8446
- **OpenSSL Documentation**: https://www.openssl.org/docs/
- **NIST TLS Guidelines**: SP 800-52 Rev. 2
- **Mozilla SSL Configuration Generator**: https://ssl-config.mozilla.org/
- **SSL Labs Best Practices**: https://github.com/ssllabs/research/wiki/SSL-and-TLS-Deployment-Best-Practices

---

**Last Updated**: 2025-12-02
**Applies to**: XOE v1.0+
