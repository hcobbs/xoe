# XOE Serial Connector Security Findings

**Date:** 2025-12-24
**Component:** src/connectors/serial/
**Files Reviewed:** serial_buffer.c/h, serial_client.c/h, serial_config.h, serial_port.c/h, serial_protocol.c/h

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 0 |
| High | 3 |
| Medium | 5 |
| Low | 3 |
| Informational | 3 |

---

## High Findings

### SER-001: Weak Checksum Algorithm

**Severity:** High
**CWE Reference:** CWE-328 (Reversible One-Way Hash), CWE-354 (Improper Validation of Integrity Check Value)
**Location:** `src/connectors/serial/serial_protocol.c:223-238`

**Description:**
The checksum algorithm is a simple byte sum with no cryptographic properties. Trivially bypassable by an attacker who can modify packets in transit.

**Evidence:**
```c
static uint32_t calculate_simple_checksum(const void* data, uint32_t len)
{
    uint32_t checksum = 0;
    for (i = 0; i < len; i++) {
        checksum += bytes[i];
    }
    return checksum;
}
```

**Impact:**
Man-in-the-middle attacker can modify packet contents and recalculate valid checksum. Enables arbitrary data injection into serial stream.

**Recommendation:**
Replace with CRC32 minimum. For security-sensitive applications, use HMAC-based authentication.

---

### SER-002: No Device Path Validation

**Severity:** High
**CWE Reference:** CWE-22 (Path Traversal)
**Location:** `src/connectors/serial/serial_port.c:99-124`

**Description:**
The `serial_port_open()` function accepts arbitrary device paths without validating for path traversal sequences, symlink attacks, or device type verification.

**Evidence:**
```c
int serial_port_open(const serial_config_t* config, int* fd)
{
    if (config->device_path[0] == '\0') {
        return E_INVALID_ARGUMENT;
    }
    file_descriptor = open(config->device_path, O_RDWR | O_NOCTTY);
    /* No path validation */
}
```

**Impact:**
Attacker controlling configuration could:
- Open arbitrary files via `/dev/../etc/passwd`
- Follow symlinks to unintended devices
- Open non-serial devices

**Recommendation:**
1. Validate path starts with `/dev/`
2. Use `realpath()` to resolve symlinks
3. Verify fd refers to serial/tty device via `ioctl(TIOCGSERIAL)`

---

### SER-003: Improper Packet Framing

**Severity:** High
**CWE Reference:** CWE-20 (Improper Input Validation), CWE-130 (Improper Handling of Length Parameter)
**Location:** `src/connectors/serial/serial_client.c:354-378`

**Description:**
Network-to-serial thread constructs packet structure with `packet.payload->len = bytes_received` from network data without validating complete XOE packet receipt. TCP stream can fragment packets.

**Impact:**
- Packets shorter than header cause errors
- Checksum from wire never actually parsed or validated
- Packet boundary misalignment

**Recommendation:**
1. Implement proper packet framing with length prefix
2. Parse and validate checksum from wire data
3. Implement packet reassembly for TCP streams

---

## Medium Findings

### SER-004: Unsynchronized Sequence Counters

**Severity:** Medium
**CWE Reference:** CWE-362 (Race Condition)
**Location:** `src/connectors/serial/serial_client.c:280, 397`

**Description:**
`tx_sequence` and `rx_sequence` accessed by I/O threads without synchronization.

**Recommendation:**
Use atomic operations for sequence number access.

---

### SER-005: Per-Packet malloc Causing Memory Pressure

**Severity:** Medium
**CWE Reference:** CWE-400 (Uncontrolled Resource Consumption)
**Location:** `src/connectors/serial/serial_client.c:359-364`

**Description:**
`malloc()` allocates `xoe_payload_t` for every received packet. Flood of malformed packets causes fragmentation.

**Recommendation:**
Allocate payload structure once at thread start and reuse.

---

### SER-006: Potential Stale Pointer State

**Severity:** Medium
**CWE Reference:** CWE-824 (Access of Uninitialized Pointer)
**Location:** `src/connectors/serial/serial_client.c:329, 359`

**Description:**
Loop allocates new `packet.payload` each iteration. If error handling is modified to continue, stale pointer could cause use-after-free.

**Recommendation:**
Use local payload variable within loop scope.

---

### SER-007: Unchecked Mutex Lock Return

**Severity:** Medium
**CWE Reference:** CWE-391 (Unchecked Error Condition)
**Location:** `src/connectors/serial/serial_buffer.c:80-84`

**Description:**
`pthread_mutex_lock()` return value not checked in `serial_buffer_destroy()`.

---

### SER-008: Ambiguous NULL/Checksum Return

**Severity:** Medium
**CWE Reference:** CWE-476 (NULL Pointer Dereference)
**Location:** `src/connectors/serial/serial_protocol.c:148-166`

**Description:**
`serial_protocol_checksum()` returns 0 for NULL inputs. Valid checksum could also be 0.

---

## Low Findings

### SER-009: Missing Buffer Capacity Limit

**Severity:** Low
**CWE:** CWE-190
**Location:** `serial_buffer.c:159-161`

No maximum capacity limit prevents huge allocations.

---

### SER-010: Unchecked cfsetispeed/cfsetospeed

**Severity:** Low
**CWE:** CWE-252
**Location:** `serial_port.c:294-295`

Return values not checked.

---

### SER-011: network_fd Ownership Unclear

**Severity:** Low
**CWE:** CWE-404
**Location:** `serial_client.c:175-195`

Caller must close network_fd but not documented.

---

## Informational

### SER-012: Missing Byte-Order Conversion

Wire format differs between endianness. Use `htons()`/`ntohs()`.

### SER-013: No Encryption for Serial Data

Serial data transmitted without encryption. Integrate TLS or document trusted-network-only.

### SER-014: Debug Info in Logs

`__FILE__` and `__LINE__` reveal source structure.

---

## Risk Assessment

The serial connector is suitable for trusted, controlled environments. It should NOT be deployed in adversarial environments without addressing High severity findings.
