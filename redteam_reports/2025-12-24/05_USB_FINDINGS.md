# XOE USB Connector Security Findings

**Date:** 2025-12-24
**Component:** src/connectors/usb/
**Files Reviewed:** usb_client.c/h, usb_config.c/h, usb_device.c/h, usb_protocol.c/h, usb_server.c/h, usb_transfer.c/h, lib/usb_compat.h

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 1 |
| High | 5 |
| Medium | 11 |
| Low | 3 |
| Informational | 1 |

---

## Critical Finding

### USB-001: Integer Overflow in Device Allocation

**Severity:** Critical
**CWE Reference:** CWE-190 (Integer Overflow), CWE-122 (Heap-based Buffer Overflow)
**Location:** `src/connectors/usb/usb_client.c:86`

**Description:**
Device array allocation uses multiplication without overflow checking. When `max_devices` is large, `sizeof(usb_device_t) * max_devices` can wrap around to a small value, resulting in undersized allocation followed by out-of-bounds writes.

**Evidence:**
```c
client->devices = (usb_device_t*)malloc(sizeof(usb_device_t) * max_devices);
/* ... */
memset(client->devices, 0, sizeof(usb_device_t) * max_devices);  /* OOB write */
```

**Impact:**
Heap corruption, potential code execution.

**Recommendation:**
Validate `max_devices <= (SIZE_MAX / sizeof(usb_device_t))` before allocation. Use maximum reasonable limit (USB_MAX_DEVICES = 8).

---

## High Findings

### USB-002: Weak Checksum Algorithm

**Severity:** High
**CWE Reference:** CWE-328 (Use of Weak Hash), CWE-354 (Improper Validation of Integrity Check Value)
**Location:** `src/connectors/usb/usb_protocol.c:115-144`

**Description:**
Simple additive checksum trivially susceptible to manipulation.

**Impact:**
MITM attacker can modify USB data without detection. Could allow injection of malicious USB commands.

**Recommendation:**
Replace with CRC32 or HMAC-SHA256.

---

### USB-003: Use-After-Free in Transfer Thread Exit

**Severity:** High
**CWE Reference:** CWE-416 (Use After Free)
**Location:** `src/connectors/usb/usb_client.c:1071`

**Description:**
Thread context freed before dereferenced for exit log message.

**Evidence:**
```c
free(ctx);  /* Freed here */
printf("Transfer thread exiting for device %d\n", ctx->device_index + 1);  /* UAF */
```

**Recommendation:**
Save `ctx->device_index` to local variable before freeing.

---

### USB-004: Race Condition in Pending Request Matching

**Severity:** High
**CWE Reference:** CWE-362 (Race Condition)
**Location:** `src/connectors/usb/usb_client.c:1267-1298`

**Description:**
Pending list searched under lock, but lock released before modifying found request. Another thread could modify or free request in the gap.

**Impact:**
Crashes, memory corruption, incorrect response matching.

**Recommendation:**
Hold `pending_lock` during entire operation or use reference counting.

---

### USB-005: Insufficient Network Packet Validation

**Severity:** High
**CWE Reference:** CWE-20 (Improper Input Validation), CWE-131 (Incorrect Buffer Size Calculation)
**Location:** `src/connectors/usb/usb_client.c:555`

**Description:**
Receives `xoe_packet_t` including garbage/attacker-controlled pointer values. Pointer then dereferenced without validation.

**Impact:**
Arbitrary memory access, information disclosure, code execution.

**Recommendation:**
Implement proper packet framing. Receive header first, validate, then receive payload separately.

---

### USB-015: No Authentication for Client Registration

**Severity:** High
**CWE Reference:** CWE-306 (Missing Authentication for Critical Function)
**Location:** `src/connectors/usb/usb_server.c:88-124`

**Description:**
Any client can register any device ID without authentication. No verification that client is authorized to claim device ID.

**Impact:**
Device hijacking, traffic interception, impersonation.

**Recommendation:**
Implement authentication. Consider TLS with client certificates.

---

## Medium Findings

### USB-006: Missing Length Validation Before memcpy

**Severity:** Medium
**CWE:** CWE-120
**Location:** `usb_protocol.c:287-296`

Untrusted `payload_data_len` used without checking against `USB_MAX_DATA_SIZE`.

---

### USB-007: No Device Path Validation

**Severity:** Medium
**CWE:** CWE-22, CWE-250
**Location:** `usb_device.c:204-217`

Device opened by VID/PID only. No authorization or whitelist.

---

### USB-008: Race After Lock Release

**Severity:** Medium
**CWE:** CWE-362, CWE-125
**Location:** `usb_server.c:219`

Loop variable used after releasing `registry_lock`.

---

### USB-009: Unchecked pthread Return Values

**Severity:** Medium
**CWE:** CWE-252
**Location:** Multiple in usb_client.c, usb_server.c

pthread_mutex_init, pthread_cond_init returns ignored.

---

### USB-010: Memory Leak on Thread Creation Failure

**Severity:** Medium
**CWE:** CWE-401
**Location:** `usb_client.c:289-310`

Partial cleanup when some threads fail to create.

---

### USB-011: Timing Side-Channel in Checksum

**Severity:** Medium
**CWE:** CWE-208
**Location:** `usb_protocol.c:282-284`

Direct equality comparison leaks timing info.

---

### USB-012: Missing Timeout on Unregistration

**Severity:** Medium
**CWE:** CWE-400
**Location:** `usb_client.c:701-710`

Can block indefinitely waiting for response.

---

### USB-014: Missing URB Header Validation

**Severity:** Medium
**CWE:** CWE-20
**Location:** `usb_server.c:359-380`

URB fields used without bounds checking.

---

### USB-017: No Rate Limiting on Registration

**Severity:** Medium
**CWE:** CWE-770
**Location:** `usb_server.c:88-124`

Can fill all USB_MAX_CLIENTS slots rapidly (DoS).

---

### USB-018: Device ID Collision Not Handled

**Severity:** Medium
**CWE:** CWE-694
**Location:** `usb_server.c:171-240`

Multiple clients can register same device_id. First match wins routing.

---

### USB-019: libusb Context Thread Safety

**Severity:** Medium
**CWE:** CWE-362
**Location:** `usb_device.c:192-201`

Shared libusb contexts may not be thread-safe.

---

## Low Findings

### USB-013: Information Disclosure in Errors

**Severity:** Low
**CWE:** CWE-209

Detailed internal info in error messages.

---

### USB-016: NULL Dereference in Statistics

**Severity:** Low
**CWE:** CWE-476
**Location:** `usb_server.c:407-417`

Casting away const to lock mutex during stats.

---

### USB-020: Incorrect Error Mapping

**Severity:** Low
**CWE:** CWE-754
**Location:** `usb_transfer.c:432-438`

libusb_transfer_status passed to wrong mapping function.

---

## Informational

### USB-021: Missing server_ip Length Check

**Severity:** Informational
**CWE:** CWE-120
**Location:** `usb_client.c:74-81`

No maximum length check before allocation.
