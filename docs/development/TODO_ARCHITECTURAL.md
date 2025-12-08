# Architectural Improvements (Deferred)

This document tracks architectural improvements identified during code review that are
deferred for future refactoring efforts. These are valid improvements but not critical
for the current release.

## Status: Tracking Only

These items are documented for future consideration. They do not block any PR.

---

## 1. Encapsulate Global Variables in Server Context

**File:** `src/core/server.c`

**Issue:** The project relies on several global variables (`client_pool`, `pool_mutex`,
`g_tls_ctx`, `g_usb_server`). While common in C, this can lead to tight coupling and
make refactoring or testing in isolation more challenging.

**Recommendation:** Consider encapsulating these globals within a `server_context_t`
or similar structure that is explicitly passed to functions. This would improve
modularity and make the data dependencies clearer.

**Priority:** Low

---

## 2. Abstract TLS Conditional Compilation

**File:** `src/core/server.c`

**Issue:** The heavy use of `#if TLS_ENABLED` creates two distinct code paths within
`handle_client`, reducing readability and increasing maintenance burden.

**Recommendation:** Explore design patterns that use function pointers for read/write
operations (e.g., `xoe_read_func_t`, `xoe_write_func_t`) that are set during server
initialization based on `encryption_mode`. This can abstract away the conditional
compilation from the core logic.

**Priority:** Low

---

## 3. Verify shutdown() Usage with TLS

**File:** `src/core/server.c`

**Issue:** The `shutdown(client_socket, SHUT_RDWR)` call is made after
`tls_session_shutdown` and `tls_session_destroy`. This may be redundant or could
interfere with the internal state of TLS shutdown.

**Recommendation:** Verify if this `shutdown` call is necessary or redundant.
Typically, `close()` on its own is sufficient after SSL cleanup, unless specific
half-close semantics are required.

**Priority:** Low

---

## 4. Replace pthread_exit(NULL) with return NULL

**File:** `src/core/server.c:handle_client`

**Issue:** The `handle_client` function uses `pthread_exit(NULL)`. For a detached
thread, using `return NULL;` achieves the same effect and is often considered slightly
cleaner as it doesn't bypass normal function return semantics.

**Recommendation:** Replace `pthread_exit(NULL);` with `return NULL;` at the end of
the `handle_client` function.

**Priority:** Low

---

## 5. Replace sleep(1) Polling in wait_for_clients

**File:** `src/core/server.c`

**Issue:** Relying on `sleep(1)` to poll for client exit is inefficient and introduces
brittleness.

**Recommendation:** For a fixed-size client pool, use `pthread_join` on each client
thread (if they are not detached) or a condition variable to signal when a client slot
becomes available, rather than polling. If threads are detached, a separate counter
protected by a mutex could track active clients, and a condition variable could signal
when it reaches zero.

**Priority:** Low

---

## Review Source

These items were identified in the third-party code review:
`~/dev/consultation/xoe/CLAUDE_CODE_REVIEW.md`

## History

- 2025-12-07: Document created from code review findings
