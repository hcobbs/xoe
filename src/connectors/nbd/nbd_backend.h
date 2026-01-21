/**
 * @file nbd_backend.h
 * @brief NBD storage backend interface
 *
 * Provides abstract interface for different storage backend types:
 * - File backend: Regular files as block devices (testing)
 * - Zvol backend: ZFS zvols (/dev/zvol/pool/dataset)
 * - Device backend: Raw block devices (/dev/sda, etc.)
 *
 * All I/O operations use thread-safe positional I/O (pread/pwrite).
 *
 * [LLM-ASSISTED]
 */

#ifndef NBD_BACKEND_H
#define NBD_BACKEND_H

#include "lib/common/types.h"
#include "lib/common/definitions.h"
#include "connectors/nbd/nbd_config.h"
#include <pthread.h>

/* Maximum path length for backend storage */
#define NBD_BACKEND_PATH_MAX 256

/**
 * NBD backend context structure
 *
 * Encapsulates storage backend state and I/O operations.
 * Thread-safe via io_mutex for serializing operations (optional).
 */
typedef struct {
    int fd;                          /* File descriptor */
    int backend_type;                /* NBD_BACKEND_* */
    uint64_t size;                   /* Total size in bytes */
    uint32_t block_size;             /* Block size in bytes */
    int read_only;                   /* Read-only flag */
    char path[NBD_BACKEND_PATH_MAX]; /* Path to backing store */
    pthread_mutex_t io_mutex;        /* Serializes I/O operations */
} nbd_backend_t;

/**
 * nbd_backend_open - Open storage backend
 * @backend: Pointer to backend structure (will be initialized)
 * @path: Path to storage (file, zvol, or device)
 * @backend_type: NBD_BACKEND_* type (use NBD_BACKEND_AUTO for auto-detect)
 * @read_only: Open in read-only mode (TRUE/FALSE)
 *
 * Opens the specified storage backend and initializes the backend structure.
 * Auto-detects backend type if NBD_BACKEND_AUTO is specified.
 *
 * For zvols and devices, queries size via ioctl(BLKGETSIZE64).
 * For files, uses fstat() to determine size.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_backend_open(nbd_backend_t* backend, const char* path,
                     int backend_type, int read_only);

/**
 * nbd_backend_close - Close storage backend
 * @backend: Pointer to backend structure
 *
 * Closes file descriptor and destroys mutex.
 * Safe to call multiple times.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_backend_close(nbd_backend_t* backend);

/**
 * nbd_backend_read - Read from storage backend
 * @backend: Pointer to backend structure
 * @buf: Output buffer for data
 * @offset: Byte offset in storage
 * @length: Number of bytes to read
 *
 * Reads data from backend using thread-safe pread().
 * Validates offset and length against storage size.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_backend_read(nbd_backend_t* backend, void* buf,
                     uint64_t offset, uint32_t length);

/**
 * nbd_backend_write - Write to storage backend
 * @backend: Pointer to backend structure
 * @buf: Input buffer with data
 * @offset: Byte offset in storage
 * @length: Number of bytes to write
 *
 * Writes data to backend using thread-safe pwrite().
 * Validates offset and length against storage size.
 * Fails if backend is read-only.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_backend_write(nbd_backend_t* backend, const void* buf,
                      uint64_t offset, uint32_t length);

/**
 * nbd_backend_flush - Flush pending writes to storage
 * @backend: Pointer to backend structure
 *
 * Forces all buffered writes to persistent storage using fdatasync().
 * Ensures data durability.
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_backend_flush(nbd_backend_t* backend);

/**
 * nbd_backend_trim - Discard/TRIM block range
 * @backend: Pointer to backend structure
 * @offset: Byte offset to start of range
 * @length: Number of bytes to discard
 *
 * Discards/trims the specified range, allowing the backend to reclaim space.
 *
 * For block devices: Uses ioctl(BLKDISCARD) on Linux, no-op on BSD/macOS.
 * For files: No-op (future: could write zeros or use fallocate).
 *
 * Returns: 0 on success, negative error code on failure
 */
int nbd_backend_trim(nbd_backend_t* backend, uint64_t offset, uint32_t length);

/**
 * nbd_backend_get_size - Get backend storage size
 * @backend: Pointer to backend structure
 *
 * Returns: Size in bytes
 */
uint64_t nbd_backend_get_size(const nbd_backend_t* backend);

/**
 * nbd_backend_get_block_size - Get backend block size
 * @backend: Pointer to backend structure
 *
 * Returns: Block size in bytes
 */
uint32_t nbd_backend_get_block_size(const nbd_backend_t* backend);

/**
 * nbd_backend_is_read_only - Check if backend is read-only
 * @backend: Pointer to backend structure
 *
 * Returns: TRUE if read-only, FALSE otherwise
 */
int nbd_backend_is_read_only(const nbd_backend_t* backend);

#endif /* NBD_BACKEND_H */
