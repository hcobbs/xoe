/**
 * @file nbd_backend.c
 * @brief NBD storage backend implementation
 *
 * Implements storage backend operations for file, zvol, and device types.
 * Uses thread-safe positional I/O (pread/pwrite).
 *
 * [LLM-ASSISTED]
 */

#define _POSIX_C_SOURCE 200112L  /* For fdatasync */

#include "connectors/nbd/nbd_backend.h"
#include "lib/common/definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

/* Platform-specific headers for block device size query */
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>  /* BLKGETSIZE64 */
#endif

#ifdef __APPLE__
#include <sys/disk.h>  /* DKIOCGETBLOCKCOUNT */
#endif

/**
 * nbd_backend_open - Open storage backend
 */
int nbd_backend_open(nbd_backend_t* backend, const char* path,
                     int backend_type, int read_only)
{
    struct stat st = {0};
    int flags = 0;
    int result = 0;

    /* Validate parameters */
    if (backend == NULL || path == NULL) {
        return E_NULL_POINTER;
    }

    if (path[0] == '\0') {
        return E_INVALID_ARGUMENT;
    }

    /* Initialize backend structure */
    memset(backend, 0, sizeof(nbd_backend_t));

    /* Copy path */
    strncpy(backend->path, path, NBD_BACKEND_PATH_MAX - 1);
    backend->path[NBD_BACKEND_PATH_MAX - 1] = '\0';

    /* Check if path exists */
    if (stat(path, &st) != 0) {
        fprintf(stderr, "NBD backend error: path '%s' not found\n", path);
        return E_FILE_NOT_FOUND;
    }

    /* Auto-detect backend type if requested */
    if (backend_type == NBD_BACKEND_AUTO) {
        backend_type = nbd_config_auto_detect_backend(path);
        if (backend_type < 0) {
            return backend_type;  /* Error code */
        }
    }

    backend->backend_type = backend_type;
    backend->read_only = read_only;

    /* Open file descriptor */
    flags = read_only ? O_RDONLY : O_RDWR;

    backend->fd = open(path, flags);
    if (backend->fd < 0) {
        fprintf(stderr, "NBD backend error: failed to open '%s': %s\n",
                path, strerror(errno));
        return E_IO_ERROR;
    }

    /* Query size based on backend type */
    if (backend_type == NBD_BACKEND_FILE) {
        /* Regular file: use fstat */
        if (fstat(backend->fd, &st) != 0) {
            fprintf(stderr, "NBD backend error: fstat failed for '%s': %s\n",
                    path, strerror(errno));
            close(backend->fd);
            return E_IO_ERROR;
        }
        backend->size = (uint64_t)st.st_size;

    } else if (backend_type == NBD_BACKEND_ZVOL ||
               backend_type == NBD_BACKEND_DEVICE) {
        /* Block device: use ioctl to get size */
#ifdef __linux__
        if (ioctl(backend->fd, BLKGETSIZE64, &backend->size) != 0) {
            fprintf(stderr, "NBD backend error: BLKGETSIZE64 failed for '%s': %s\n",
                    path, strerror(errno));
            close(backend->fd);
            return E_IO_ERROR;
        }
#elif defined(__APPLE__)
        {
            uint64_t block_count;
            uint32_t block_size_bytes;

            if (ioctl(backend->fd, DKIOCGETBLOCKCOUNT, &block_count) != 0) {
                fprintf(stderr, "NBD backend error: DKIOCGETBLOCKCOUNT failed for '%s': %s\n",
                        path, strerror(errno));
                close(backend->fd);
                return E_IO_ERROR;
            }

            if (ioctl(backend->fd, DKIOCGETBLOCKSIZE, &block_size_bytes) != 0) {
                fprintf(stderr, "NBD backend error: DKIOCGETBLOCKSIZE failed for '%s': %s\n",
                        path, strerror(errno));
                close(backend->fd);
                return E_IO_ERROR;
            }

            backend->size = block_count * block_size_bytes;
        }
#else
        /* BSD/other: try lseek to determine size */
        {
            off_t size_off = lseek(backend->fd, 0, SEEK_END);
            if (size_off == (off_t)-1) {
                fprintf(stderr, "NBD backend error: lseek failed for '%s': %s\n",
                        path, strerror(errno));
                close(backend->fd);
                return E_IO_ERROR;
            }
            backend->size = (uint64_t)size_off;

            /* Reset to beginning */
            if (lseek(backend->fd, 0, SEEK_SET) == (off_t)-1) {
                close(backend->fd);
                return E_IO_ERROR;
            }
        }
#endif
    }

    /* Set default block size */
    backend->block_size = NBD_DEFAULT_BLOCK_SIZE;

    /* Initialize I/O mutex */
    result = pthread_mutex_init(&backend->io_mutex, NULL);
    if (result != 0) {
        fprintf(stderr, "NBD backend error: mutex init failed\n");
        close(backend->fd);
        return E_INIT_FAILED;
    }

    printf("NBD backend opened: %s (%s, %llu bytes, %s)\n",
           path,
           (backend_type == NBD_BACKEND_FILE) ? "file" :
           (backend_type == NBD_BACKEND_ZVOL) ? "zvol" : "device",
           (unsigned long long)backend->size,
           read_only ? "read-only" : "read-write");

    return SUCCESS;
}

/**
 * nbd_backend_close - Close storage backend
 */
int nbd_backend_close(nbd_backend_t* backend) {
    if (backend == NULL) {
        return E_NULL_POINTER;
    }

    if (backend->fd >= 0) {
        close(backend->fd);
        backend->fd = -1;
    }

    pthread_mutex_destroy(&backend->io_mutex);

    return SUCCESS;
}

/**
 * nbd_backend_read - Read from storage backend
 */
int nbd_backend_read(nbd_backend_t* backend, void* buf,
                     uint64_t offset, uint32_t length)
{
    ssize_t bytes_read;

    /* Validate parameters */
    if (backend == NULL || buf == NULL) {
        return E_NULL_POINTER;
    }

    if (backend->fd < 0) {
        return E_INVALID_STATE;
    }

    /* Validate offset and length */
    if (offset + length > backend->size) {
        fprintf(stderr, "NBD backend read error: offset %llu + length %u exceeds size %llu\n",
                (unsigned long long)offset, length, (unsigned long long)backend->size);
        return E_INVALID_ARGUMENT;
    }

    /* Thread-safe positional read */
    pthread_mutex_lock(&backend->io_mutex);
    bytes_read = pread(backend->fd, buf, length, (off_t)offset);
    pthread_mutex_unlock(&backend->io_mutex);

    if (bytes_read < 0) {
        fprintf(stderr, "NBD backend read error at offset %llu: %s\n",
                (unsigned long long)offset, strerror(errno));
        return E_IO_ERROR;
    }

    if ((uint32_t)bytes_read != length) {
        fprintf(stderr, "NBD backend read error: short read (%ld of %u bytes)\n",
                (long)bytes_read, length);
        return E_IO_ERROR;
    }

    return SUCCESS;
}

/**
 * nbd_backend_write - Write to storage backend
 */
int nbd_backend_write(nbd_backend_t* backend, const void* buf,
                      uint64_t offset, uint32_t length)
{
    ssize_t bytes_written;

    /* Validate parameters */
    if (backend == NULL || buf == NULL) {
        return E_NULL_POINTER;
    }

    if (backend->fd < 0) {
        return E_INVALID_STATE;
    }

    /* Check read-only flag */
    if (backend->read_only) {
        fprintf(stderr, "NBD backend write error: backend is read-only\n");
        return E_PERMISSION_DENIED;
    }

    /* Validate offset and length */
    if (offset + length > backend->size) {
        fprintf(stderr, "NBD backend write error: offset %llu + length %u exceeds size %llu\n",
                (unsigned long long)offset, length, (unsigned long long)backend->size);
        return E_INVALID_ARGUMENT;
    }

    /* Thread-safe positional write */
    pthread_mutex_lock(&backend->io_mutex);
    bytes_written = pwrite(backend->fd, buf, length, (off_t)offset);
    pthread_mutex_unlock(&backend->io_mutex);

    if (bytes_written < 0) {
        fprintf(stderr, "NBD backend write error at offset %llu: %s\n",
                (unsigned long long)offset, strerror(errno));
        return E_IO_ERROR;
    }

    if ((uint32_t)bytes_written != length) {
        fprintf(stderr, "NBD backend write error: short write (%ld of %u bytes)\n",
                (long)bytes_written, length);
        return E_IO_ERROR;
    }

    return SUCCESS;
}

/**
 * nbd_backend_flush - Flush pending writes to storage
 */
int nbd_backend_flush(nbd_backend_t* backend) {
    int result;

    if (backend == NULL) {
        return E_NULL_POINTER;
    }

    if (backend->fd < 0) {
        return E_INVALID_STATE;
    }

    /* Read-only backends don't need flushing */
    if (backend->read_only) {
        return SUCCESS;
    }

    /* Flush data to persistent storage */
    pthread_mutex_lock(&backend->io_mutex);
#ifdef __linux__
    result = fdatasync(backend->fd);  /* Linux: data only */
#else
    result = fsync(backend->fd);      /* BSD/macOS: data + metadata */
#endif
    pthread_mutex_unlock(&backend->io_mutex);

    if (result != 0) {
        fprintf(stderr, "NBD backend flush error: %s\n", strerror(errno));
        return E_IO_ERROR;
    }

    return SUCCESS;
}

/**
 * nbd_backend_trim - Discard/TRIM block range
 */
int nbd_backend_trim(nbd_backend_t* backend, uint64_t offset, uint32_t length) {
    /* Validate parameters */
    if (backend == NULL) {
        return E_NULL_POINTER;
    }

    if (backend->fd < 0) {
        return E_INVALID_STATE;
    }

    /* Read-only backends can't be trimmed */
    if (backend->read_only) {
        return E_PERMISSION_DENIED;
    }

    /* Validate offset and length */
    if (offset + length > backend->size) {
        return E_INVALID_ARGUMENT;
    }

    /* Platform-specific TRIM implementation */
#ifdef __linux__
    if (backend->backend_type == NBD_BACKEND_ZVOL ||
        backend->backend_type == NBD_BACKEND_DEVICE) {
        uint64_t range[2];
        range[0] = offset;
        range[1] = length;

        pthread_mutex_lock(&backend->io_mutex);
        if (ioctl(backend->fd, BLKDISCARD, &range) != 0) {
            pthread_mutex_unlock(&backend->io_mutex);
            /* TRIM failure is non-fatal; log and continue */
            fprintf(stderr, "NBD backend TRIM warning: ioctl failed: %s\n",
                    strerror(errno));
            return SUCCESS;  /* Don't fail on TRIM errors */
        }
        pthread_mutex_unlock(&backend->io_mutex);
    }
#else
    /* BSD/macOS/other: TRIM not implemented, no-op */
    (void)offset;
    (void)length;
#endif

    return SUCCESS;
}

/**
 * nbd_backend_get_size - Get backend storage size
 */
uint64_t nbd_backend_get_size(const nbd_backend_t* backend) {
    if (backend == NULL) {
        return 0;
    }
    return backend->size;
}

/**
 * nbd_backend_get_block_size - Get backend block size
 */
uint32_t nbd_backend_get_block_size(const nbd_backend_t* backend) {
    if (backend == NULL) {
        return 0;
    }
    return backend->block_size;
}

/**
 * nbd_backend_is_read_only - Check if backend is read-only
 */
int nbd_backend_is_read_only(const nbd_backend_t* backend) {
    if (backend == NULL) {
        return TRUE;
    }
    return backend->read_only;
}
