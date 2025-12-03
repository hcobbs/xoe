/**
 * @file serial_port.h
 * @brief Serial port I/O operations interface
 *
 * This file provides the interface for low-level serial port operations
 * including opening, closing, reading, writing, and status checking.
 * Uses POSIX termios for serial port configuration.
 *
 * [LLM-ASSISTED]
 */

#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

#include "serial_config.h"

/* Fixed-width integer types (C89 compatible) */
typedef unsigned short uint16_t;

/**
 * @brief Open and configure a serial port
 *
 * Opens the specified serial device and configures it according to the
 * provided configuration. The file descriptor is set to blocking mode
 * with the specified timeout.
 *
 * @param config Pointer to serial port configuration
 * @param fd Output parameter for file descriptor
 * @return 0 on success, negative error code on failure
 *         E_INVALID_ARGUMENT - Invalid configuration or NULL pointer
 *         E_FILE_NOT_FOUND - Device does not exist
 *         E_PERMISSION_DENIED - Insufficient permissions
 *         E_UNKNOWN_ERROR - termios configuration failed
 */
int serial_port_open(const serial_config_t* config, int* fd);

/**
 * @brief Close a serial port
 *
 * Closes the serial port file descriptor and restores terminal settings
 * if applicable.
 *
 * @param fd File descriptor to close
 * @return 0 on success, negative error code on failure
 */
int serial_port_close(int fd);

/**
 * @brief Read data from serial port with timeout
 *
 * Reads up to `len` bytes from the serial port. The read operation will
 * block until at least one byte is available or the configured timeout
 * expires.
 *
 * @param fd File descriptor
 * @param buffer Output buffer for received data
 * @param len Maximum number of bytes to read
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
 * @return Number of bytes read on success (may be less than len)
 *         0 on timeout
 *         Negative error code on failure
 */
int serial_port_read(int fd, void* buffer, int len, int timeout_ms);

/**
 * @brief Write data to serial port
 *
 * Writes up to `len` bytes to the serial port. The write operation will
 * block until all data is written or an error occurs.
 *
 * @param fd File descriptor
 * @param buffer Data to write
 * @param len Number of bytes to write
 * @return Number of bytes written on success
 *         Negative error code on failure
 */
int serial_port_write(int fd, const void* buffer, int len);

/**
 * @brief Get error status from serial port
 *
 * Retrieves error flags from the serial port, including parity errors,
 * framing errors, and overrun errors.
 *
 * @param fd File descriptor
 * @param flags Output parameter for error flags
 * @return 0 on success, negative error code on failure
 */
int serial_port_get_status(int fd, uint16_t* flags);

/**
 * @brief Flush serial port buffers
 *
 * Discards data in the input and/or output buffers.
 *
 * @param fd File descriptor
 * @param input_flush TRUE to flush input buffer
 * @param output_flush TRUE to flush output buffer
 * @return 0 on success, negative error code on failure
 */
int serial_port_flush(int fd, int input_flush, int output_flush);

#endif /* SERIAL_PORT_H */
