# XOE FSM and Configuration Security Findings

**Date:** 2025-12-24
**Component:** src/core/fsm/, src/core/config.h
**Files Reviewed:** state_init.c, state_parse_args.c, state_validate_config.c, state_apply_config.c, state_mode_select.c, state_server_mode.c, state_client_std.c, state_client_serial.c, state_client_usb.c, state_start_mgmt.c, state_mode_stop.c, state_cleanup.c

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 0 |
| High | 3 |
| Medium | 6 |
| Low | 5 |
| Informational | 1 |

---

## High Findings

### FSM-003: Path Traversal via File Paths

**Severity:** High
**CWE Reference:** CWE-22 (Improper Limitation of a Pathname to a Restricted Directory)
**Location:** `src/core/fsm/state_parse_args.c:112, 201, 211`

**Description:**
User-supplied file paths for serial devices and TLS certificates are used without sanitization or validation. Enables path traversal and symlink attacks.

**Evidence:**
```c
/* Serial device path - no validation */
config->serial_device = optarg;

/* Certificate path - no validation */
strncpy(config->cert_path, argv[optind + 1], TLS_CERT_PATH_MAX - 1);
```

**Impact:**
- Path traversal attacks (`/dev/../etc/passwd`)
- Symlink exploitation
- Access to unintended files

**Recommendation:**
1. Use `realpath()` to resolve symlinks
2. Validate resolved paths are within allowed directories
3. Check file types match expectations

---

### FSM-006: Cleartext Management Protocol

**Severity:** High
**CWE Reference:** CWE-319 (Cleartext Transmission of Sensitive Information)
**Location:** `src/core/mgmt/mgmt_server.c`

**Description:**
Management interface transmits all data including passwords in cleartext over TCP. While bound to localhost, local attackers or forwarded ports could intercept credentials.

**Impact:**
- Local privilege escalation through password sniffing
- In containerized scenarios, remote credential capture

**Recommendation:**
1. Implement TLS for management interface
2. Consider Unix domain sockets with file permissions
3. Warn when password auth enabled without encryption

---

### FSM-012: No Default Authentication on Management Interface

**Severity:** High
**CWE Reference:** CWE-306 (Missing Authentication for Critical Function)
**Location:** `src/core/fsm/state_init.c:70`, `src/core/fsm/state_start_mgmt.c:23-56`

**Description:**
Management interface runs without password authentication by default. Any local user can reconfigure server.

**Evidence:**
```c
/* state_init.c:70 */
config->mgmt_password = NULL;  /* No auth by default */
```

**Recommendation:**
Require password by default or on first run.

---

## Medium Findings

### FSM-001: Use of atoi() Without Complete Validation

**Severity:** Medium
**CWE Reference:** CWE-20 (Improper Input Validation)
**Location:** `src/core/fsm/state_parse_args.c:61, 80, 118, 246, 262, 302`

**Description:**
`atoi()` used throughout argument parsing. Returns 0 for non-numeric strings (indistinguishable from "0"). Has undefined behavior on overflow.

**Recommendation:**
Replace with `strtol()` with proper error checking.

---

### FSM-004: Relative Certificate Paths

**Severity:** Medium
**CWE Reference:** CWE-426 (Untrusted Search Path)
**Location:** `src/lib/security/tls_config.h:23-24`

**Description:**
Default TLS certificate paths use relative paths (`./certs/server.crt`). Vulnerable to certificate substitution if CWD is attacker-controlled.

**Recommendation:**
Use absolute paths for default certificates.

---

### FSM-005: Plaintext Password Storage

**Severity:** Medium
**CWE Reference:** CWE-256 (Plaintext Storage of a Password)
**Location:** `src/core/mgmt/mgmt_server.c:83-86, 95-96`

**Description:**
Password stored in plaintext and copied to each session structure.

**Recommendation:**
Hash password at startup. Zero out plaintext immediately.

---

### FSM-009: No Auth Rate Limiting

**Severity:** Medium
**CWE Reference:** CWE-307 (Improper Restriction of Excessive Authentication Attempts)
**Location:** `src/core/mgmt/mgmt_server.c:322-360`

**Description:**
3-attempt limit per connection but no reconnection delay. Unlimited brute force via rapid reconnection.

---

### FSM-010: DNS Rebinding Potential

**Severity:** Medium
**CWE Reference:** CWE-350 (Reliance on Reverse DNS Resolution)
**Location:** `src/lib/net/net_resolve.c:163`

**Description:**
DNS resolution uses `getaddrinfo()` without caching. DNS response can change between lookups.

**Recommendation:**
Cache DNS results. Pin certificates or IPs on initial connection.

---

### FSM-013: DoS via Restart Command

**Severity:** Medium
**CWE Reference:** CWE-400 (Uncontrolled Resource Consumption)
**Location:** `src/core/mgmt/mgmt_commands.c:368-397`

**Description:**
Any authenticated (or unauthenticated by default) user can trigger server restart.

---

## Low Findings

### FSM-002: Missing Baud Rate Whitelist

**Severity:** Low
**CWE:** CWE-20
**Location:** `state_parse_args.c:117-126`

Only checks for positive, not supported values.

---

### FSM-007: Missing NULL Check for optarg

**Severity:** Low
**CWE:** CWE-476
**Location:** `state_parse_args.c:71-78`

---

### FSM-008: Timing Attack on Password

**Severity:** Low
**CWE:** CWE-208
**Location:** `mgmt_server.c:346`

Uses `strcmp()`.

---

### FSM-011: USB PID=0 Rejected

**Severity:** Low
**CWE:** CWE-20
**Location:** `state_parse_args.c:157-161`

Some devices may legitimately have PID=0.

---

### FSM-014: Global State Race Conditions

**Severity:** Low
**CWE:** CWE-362
**Location:** `config.h:8, 12`

`sig_atomic_t` not sufficient for multi-thread access.

---

## Informational

### FSM-015: Server Binds to All Interfaces by Default

**Severity:** Informational
**CWE:** CWE-668
**Location:** `state_server_mode.c:131`

`INADDR_ANY` used when no listen address specified. Consider defaulting to localhost.
