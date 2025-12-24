# XOE Protocol and Library Security Findings

**Date:** 2025-12-24
**Component:** src/lib/protocol/, src/lib/common/, src/lib/net/
**Files Reviewed:** protocol.h, definitions.h, types.h, net_resolve.c/h

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 1 |
| High | 2 |
| Medium | 3 |
| Low | 3 |
| Informational | 2 |

---

## Critical Finding

### LIB-001: Network Transmission of Non-Packed Structure with Pointer

**Severity:** Critical
**CWE Reference:** CWE-119 (Buffer Overflow), CWE-704 (Incorrect Type Conversion)
**Location:** `src/lib/protocol/protocol.h:89-98`, `src/core/server.c:143-148`

**Description:**
The `xoe_packet_t` structure contains a pointer (`xoe_payload_t* payload`) and is transmitted directly over the network. This has multiple critical issues:
1. Pointer value is meaningless on receiving end
2. No packing directive means layout varies by compiler/architecture
3. 64-bit pointer = 8 bytes, 32-bit = 4 bytes (cross-architecture incompatibility)

**Evidence:**
```c
/* protocol.h:89-98 */
typedef struct {
    uint16_t protocol_id;
    uint16_t protocol_version;
    xoe_payload_t* payload;     /* POINTER transmitted over network! */
    uint32_t checksum;
} xoe_packet_t;

/* server.c:145-148 */
bytes_received = recv(client_socket, &packet, sizeof(xoe_packet_t), 0);
/* sizeof(xoe_packet_t) is 16 on 32-bit, 24 on 64-bit */
```

**Impact:**
- Remote code execution via controlled pointer values
- DoS through memory corruption
- Complete protocol incompatibility between architectures
- Information leakage of sender's memory layout

**Recommendation:**
1. Create separate "wire format" structure with fixed-size fields, no pointers
2. Use explicit serialization/deserialization
3. Add `__attribute__((packed))` if direct struct I/O required
4. Never transmit pointers over network

---

## High Findings

### LIB-002: Weak Checksum Algorithm

**Severity:** High
**CWE Reference:** CWE-328 (Use of Weak Hash), CWE-354 (Improper Validation)
**Location:** `src/connectors/serial/serial_protocol.c:223-238`

**Description:**
Simple byte sum provides minimal error detection and zero integrity protection against malicious modification.

**Recommendation:**
Replace with CRC-32 for error detection. Use HMAC-SHA256 for integrity protection.

---

### LIB-003: Missing Endianness Conversion in Serial Header

**Severity:** High
**CWE Reference:** CWE-188 (Reliance on Data/Memory Layout)
**Location:** `src/connectors/serial/serial_protocol.c:57-60`, `serial_protocol.c:132-134`

**Description:**
`serial_header_t` fields written without endianness conversion. Communication between big-endian and little-endian systems will fail silently.

**Evidence:**
```c
/* Encapsulation - no htons() */
header->flags = flags;
header->sequence = sequence;

/* Decapsulation - no ntohs() */
*sequence = header->sequence;
*flags = header->flags;
```

**Recommendation:**
Use `htons()` when writing and `ntohs()` when reading.

---

## Medium Findings

### LIB-004: Integer Overflow in Payload Size Calculation

**Severity:** Medium
**CWE Reference:** CWE-190 (Integer Overflow)
**Location:** `src/connectors/serial/serial_protocol.c:41-42`

**Description:**
`total_payload_size = SERIAL_HEADER_SIZE + len` could overflow if check is removed or MAX increased.

**Recommendation:**
Add explicit overflow check before addition.

---

### LIB-005: TOCTOU Race in DNS Resolution

**Severity:** Medium
**CWE Reference:** CWE-367 (Time-of-Check Time-of-Use)
**Location:** `src/lib/net/net_resolve.c:50-104`, `net_resolve.c:106-204`

**Description:**
DNS can change between resolution and connection (DNS rebinding attack).

**Impact:**
- DNS rebinding attacks
- SSRF in multi-tenant environments

**Recommendation:**
1. Cache DNS with short TTL
2. Verify certificate CN/SAN matches hostname
3. Consider IP pinning for sensitive connections

---

### LIB-006: IPv4-Only Limitation

**Severity:** Medium
**CWE Reference:** CWE-1188 (Insecure Default Initialization)
**Location:** `src/lib/net/net_resolve.c:83, 159`

**Description:**
Hardcoded to `AF_INET` (IPv4 only). IPv6 silently ignored.

**Recommendation:**
Support `AF_UNSPEC` for dual-stack. Document limitation.

---

## Low Findings

### LIB-007: Type Width Assumptions

**Severity:** Low
**CWE:** CWE-681
**Location:** `types.h:29-43`

C89 fallback assumes `short` = 16 bits, `int` = 32 bits.

**Recommendation:**
Add compile-time assertions to verify type sizes.

---

### LIB-008: Error Code Information Leakage

**Severity:** Low
**CWE:** CWE-209
**Location:** `definitions.h:1-51`

Detailed error codes (TLS, USB) aid fingerprinting.

**Recommendation:**
Map to generic external errors. Log details server-side only.

---

### LIB-009: Fragile Length Check Ordering

**Severity:** Low
**CWE:** CWE-476, CWE-125
**Location:** `serial_protocol.c:120-122`

Length check depends on code ordering. Safe but fragile.

---

## Informational

### LIB-010: Boolean as Integer

**Severity:** Informational
**CWE:** CWE-704
**Location:** `definitions.h:4-6`

`TRUE`/`FALSE` as macros. `if (value == TRUE)` fails for non-1 values.

---

### LIB-011: Protocol Version Not Validated

**Severity:** Informational
**CWE:** CWE-354
**Location:** `serial_protocol.c:160-164`

Checksum includes version but decapsulation only validates protocol_id.

---

## Architecture Note

The Critical finding (LIB-001) represents a **fundamental architectural issue**. The current protocol design is unsafe for cross-platform or adversarial network environments. This must be addressed before any other remediation.
