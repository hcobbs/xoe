# XOE Network Server Security Findings

**Date:** 2025-12-24
**Component:** src/core/server.c, src/core/mgmt/
**Files Reviewed:** server.c/h, config.h, main.c, globals.c, mgmt_server.c/h, mgmt_commands.c/h, mgmt_config.c/h

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 2 |
| High | 2 |
| Medium | 5 |
| Low | 5 |
| Informational | 2 |

---

## Critical Findings

### NET-001: Management Interface Lacks Authentication by Default

**Severity:** Critical
**CWE Reference:** CWE-306 (Missing Authentication for Critical Function)
**Location:** `src/core/config.h:75`, `src/core/mgmt/mgmt_server.c:82-86, 288-300`

**Description:**
The management interface password is optional (NULL by default). When no password is configured, any client connecting to the management port gains full unauthenticated access to runtime configuration commands including `restart`, `shutdown`, and configuration modification.

**Evidence:**
```c
/* config.h:75 */
char *mgmt_password;  /* Management password (NULL = no auth) */

/* mgmt_server.c:288-300 */
if (session->password[0] != '\0') {
    if (!authenticate_session(session)) { /* ... */ }
} else {
    session->authenticated = 1;  /* Auto-authenticate! */
}
```

**Impact:**
Any local process can reconfigure the server, trigger restarts, or gather configuration information without credentials.

**Recommendation:**
Require mandatory authentication or generate random password at startup.

---

### NET-006: Dangerous Pointer Received from Network

**Severity:** Critical
**CWE Reference:** CWE-822 (Untrusted Pointer Dereference)
**Location:** `src/lib/protocol/protocol.h:89-98`, `src/core/server.c:143-148`

**Description:**
The `xoe_packet_t` structure contains a `payload` pointer which is read directly from the network. If subsequently dereferenced, an attacker can cause arbitrary memory access.

**Evidence:**
```c
/* protocol.h:89-98 */
typedef struct {
    uint16_t protocol_id;
    uint16_t protocol_version;
    xoe_payload_t* payload;    /* POINTER from network! */
    uint32_t checksum;
} xoe_packet_t;

/* server.c:143-148 */
bytes_received = recv(client_socket, &packet, sizeof(xoe_packet_t), 0);
```

**Impact:**
- Server crash via invalid pointer
- Arbitrary memory read
- Potential code execution

**Recommendation:**
Redesign wire format to use flat buffer format. Never read pointers from network.

---

## High Findings

### NET-002: Management Password Stored in Plaintext

**Severity:** High
**CWE Reference:** CWE-256 (Unprotected Storage of Credentials)
**Location:** `src/core/mgmt/mgmt_server.c:39`, `src/core/mgmt/mgmt_internal.h:20`

**Description:**
Password stored in plaintext in server structure and copied to each session structure.

**Recommendation:**
Hash password at startup using bcrypt/argon2. Use `mlock()` to prevent swapping.

---

### NET-005: Insufficient Protocol Packet Validation

**Severity:** High
**CWE Reference:** CWE-20 (Improper Input Validation)
**Location:** `src/core/server.c:138-169`

**Description:**
Server reads network data directly into `xoe_packet_t` without validating protocol_id, protocol_version, or packet integrity before routing.

**Recommendation:**
Validate all protocol header fields. Implement and verify checksums.

---

## Medium Findings

### NET-003: Password Comparison Vulnerable to Timing Attack

**Severity:** Medium
**CWE Reference:** CWE-208 (Observable Timing Discrepancy)
**Location:** `src/core/mgmt/mgmt_server.c:346`

**Description:**
Uses `strcmp()` which returns early on first mismatch.

**Recommendation:**
Implement constant-time comparison.

---

### NET-004: No Rate Limiting on Password Authentication

**Severity:** Medium
**CWE Reference:** CWE-307 (Improper Restriction of Excessive Authentication Attempts)
**Location:** `src/core/mgmt/mgmt_server.c:322-360`

**Description:**
Only 3 attempts per connection but no delay between reconnections.

**Recommendation:**
Implement per-IP rate limiting with exponential backoff.

---

### NET-007: Thread-Unsafe Access to Shared State

**Severity:** Medium
**CWE Reference:** CWE-362 (Race Condition)
**Location:** `src/core/globals.c:12`

**Description:**
`g_mgmt_restart_requested` uses `volatile sig_atomic_t` which is not thread-safe for check-then-act patterns.

**Recommendation:**
Use atomic operations or mutex protection.

---

### NET-012: Fixed Client Pool DoS Potential

**Severity:** Medium
**CWE Reference:** CWE-400 (Uncontrolled Resource Consumption)
**Location:** `src/core/fsm/state_server_mode.c:197-206`

**Description:**
When pool is full, server still accepts and closes connections, consuming CPU.

**Recommendation:**
Implement connection rate limiting per source IP.

---

### NET-013: No TLS Client Certificate Verification

**Severity:** Medium
**CWE Reference:** CWE-295 (Improper Certificate Validation)
**Location:** `src/lib/security/tls_config.h:45`

**Description:**
`TLS_DEFAULT_VERIFY_MODE` is `TLS_VERIFY_NONE`.

**Recommendation:**
Document implications. Consider mTLS for high-security deployments.

---

## Low Findings

### NET-008: Release of Session Slot Without Mutex

**Severity:** Low
**CWE:** CWE-362
**Location:** `mgmt_server.c:387-393`

---

### NET-009: Incomplete Signal Handler

**Severity:** Low
**CWE:** CWE-479
**Location:** `state_server_mode.c:40-43`

Use `sigaction()` instead of `signal()`.

---

### NET-010: Echo Command Information Leak

**Severity:** Low
**CWE:** CWE-200
**Location:** `mgmt_commands.c:153-155`

Unknown commands echoed back.

---

### NET-015: Password Remains in Read Buffer

**Severity:** Low
**CWE:** CWE-316
**Location:** `mgmt_server.c:327-350`

Password persists after comparison.

---

### NET-016: Command Parsing Argument Injection Potential

**Severity:** Low
**CWE:** CWE-88
**Location:** `mgmt_commands.c:76-102`

No quote or escape handling.

---

## Informational

### NET-011: Management Interface Loopback Only (Good)

Correctly binds to 127.0.0.1 only.

### NET-014: SO_REUSEADDR Usage

Standard practice but documented for awareness.
