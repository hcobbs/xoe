# XOE TLS/SSL Security Findings

**Date:** 2025-12-24
**Component:** src/lib/security/
**Files Reviewed:** tls_config.h, tls_context.c/h, tls_error.c/h, tls_io.c/h, tls_session.c/h, tls_types.h

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 2 |
| High | 0 |
| Medium | 0 |
| Low | 2 |
| Informational | 1 |

---

## Findings

### TLS-001: Client Certificate Verification Completely Disabled

**Severity:** Critical
**CWE Reference:** CWE-295 (Improper Certificate Validation)
**Location:** `src/lib/security/tls_context.c:188`

**Description:**
The TLS client mode explicitly disables all server certificate verification by setting `SSL_VERIFY_NONE`. This allows connections to any server presenting any certificate, including self-signed, expired, revoked, or certificates issued for entirely different domains.

**Impact:**
- Complete compromise of TLS confidentiality and integrity
- Man-in-the-middle attacks are trivially possible
- Any network position attacker can intercept all communications
- Sensitive data exposed to attackers

**Evidence:**
```c
/* Lines 175-188 in tls_context.c */
SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
```

**Recommendation:**
1. Change default to `SSL_VERIFY_PEER`
2. Add command-line option for CA certificate path (`-ca <path>`)
3. Add explicit `--insecure` flag for testing only
4. Print warning to stderr when running in insecure mode

---

### TLS-002: Missing Hostname Verification

**Severity:** Critical
**CWE Reference:** CWE-297 (Improper Validation of Certificate with Host Mismatch)
**Location:** `src/lib/security/tls_session.c:136-201`

**Description:**
Even if certificate verification were enabled, the client code performs no hostname verification. There is no call to `SSL_set1_host()`, `SSL_set_hostflags()`, or `X509_VERIFY_PARAM_set1_host()`. A valid certificate issued to `attacker.com` would be accepted when connecting to `victim.com`.

**Impact:**
- An attacker with any valid CA-signed certificate could impersonate any server
- Reduces the benefit of PKI infrastructure to zero

**Recommendation:**
1. Add hostname parameter to `tls_session_create_client()`
2. Call `SSL_set1_host(ssl, hostname)` before handshake
3. Set `SSL_set_tlsext_host_name()` for SNI support
4. Configure `SSL_set_hostflags()` with `X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS`

---

### TLS-003: Server Does Not Require Client Certificates

**Severity:** Low (by design)
**CWE Reference:** CWE-306 (Missing Authentication for Critical Function)
**Location:** `src/lib/security/tls_config.h:45`

**Description:**
The server does not require or verify client certificates. Default verification mode is `TLS_VERIFY_NONE`. For a protocol bridge handling sensitive data, mutual TLS (mTLS) would provide stronger authentication.

**Recommendation:**
1. Add command-line option for mTLS: `--client-cert-required`
2. Add option to specify trusted client CA: `--client-ca <path>`
3. Document security implications

---

### TLS-004: Error Information Leakage via stderr

**Severity:** Informational
**CWE Reference:** CWE-209 (Generation of Error Message Containing Sensitive Information)
**Location:** `src/lib/security/tls_error.c:142-160`

**Description:**
The `tls_print_errors()` function prints detailed OpenSSL error information including internal error codes, library names, function names, and reason strings.

**Recommendation:**
Add compile-time or runtime flag for verbose vs. minimal error output.

---

### TLS-005: Thread-Local Error Buffer Allocation Failure Fallback

**Severity:** Low
**CWE Reference:** CWE-754 (Improper Check for Unusual or Exceptional Conditions)
**Location:** `src/lib/security/tls_error.c:53-66`

**Description:**
When thread-local error buffer allocation fails, the code returns a pointer to a static fallback buffer shared across all threads.

**Recommendation:**
Pre-allocate thread-local buffers during thread initialization, or make fallback thread-specific.

---

## Secure Areas (No Issues Found)

### Protocol Version Enforcement
TLS 1.2/1.3 only. SSLv3, TLS 1.0, 1.1 not supported. Downgrade attacks prevented.

### Cipher Suite Selection
Strong cipher suites with forward secrecy (ECDHE). No RC4, DES, or export ciphers.

### Secure Options
- `SSL_OP_NO_COMPRESSION` (CRIME attack mitigation)
- `SSL_OP_NO_RENEGOTIATION` (renegotiation attack mitigation)

### Memory Safety
No buffer overflows, use-after-free, or double-free detected in TLS code.

---

## Risk Summary

| Finding | Severity | Exploitability |
|---------|----------|----------------|
| TLS-001 | Critical | Trivial (MITM) |
| TLS-002 | Critical | Trivial (with TLS-001 fixed) |
| TLS-003 | Low | N/A (design choice) |
| TLS-004 | Informational | N/A |
| TLS-005 | Low | Unlikely |
