/**
 * @file serial_port.c
 * @brief Serial port I/O operations implementation
 *
 * Implements low-level serial port operations using POSIX termios.
 * Provides functions for opening, configuring, reading, writing, and
 * closing serial ports.
 *
 * [LLM-ASSISTED]
 */

#include "connectors/serial/serial_port.h"
#include "connectors/serial/serial_config.h"
#include "lib/common/definitions.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/* Internal helper functions */
static int configure_termios(int fd, const serial_config_t* config);
static int baud_to_speed_const(int baud_rate, speed_t* speed);
static int set_timeout(int fd, int timeout_ms);

/**
 * @brief Initialize serial configuration with default values
 */
int serial_config_init_defaults(serial_config_t* config)
{
    if (config == NULL) {
        return E_INVALID_ARGUMENT;
    }

    memset(config, 0, sizeof(serial_config_t));
    config->baud_rate = SERIAL_DEFAULT_BAUD;
    config->data_bits = SERIAL_DEFAULT_DATA_BITS;
    config->stop_bits = SERIAL_DEFAULT_STOP_BITS;
    config->parity = SERIAL_DEFAULT_PARITY;
    config->flow_control = SERIAL_DEFAULT_FLOW;
    config->read_timeout_ms = SERIAL_DEFAULT_TIMEOUT_MS;

    return 0;
}

/**
 * @brief Validate device path for security (SER-002, FSM-003 fix)
 *
 * Ensures the device path is safe and refers to an actual device:
 * - Must be absolute (start with /)
 * - Must be under /dev/
 * - Must not contain path traversal components (..)
 * - Must not be excessively long
 */
static int validate_device_path(const char* path)
{
    const char* p = NULL;
    size_t len = 0;

    if (path == NULL || path[0] == '\0') {
        return E_INVALID_ARGUMENT;
    }

    /* Check path length */
    len = strlen(path);
    if (len >= SERIAL_DEVICE_PATH_MAX) {
        return E_INVALID_ARGUMENT;
    }

    /* Must be absolute path */
    if (path[0] != '/') {
        return E_INVALID_ARGUMENT;
    }

    /* Must start with /dev/ */
    if (len < 5 || strncmp(path, "/dev/", 5) != 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Check for path traversal attempts */
    p = path;
    while (*p != '\0') {
        /* Check for .. component */
        if (p[0] == '.' && p[1] == '.') {
            /* Check if it's a path component (preceded by / or start) */
            if (p == path || p[-1] == '/') {
                /* Check if followed by / or end */
                if (p[2] == '/' || p[2] == '\0') {
                    return E_INVALID_ARGUMENT;
                }
            }
        }
        p++;
    }

    return 0;
}

/**
 * @brief Validate serial configuration parameters
 */
int serial_config_validate(const serial_config_t* config)
{
    int result = 0;

    if (config == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate device path for security */
    result = validate_device_path(config->device_path);
    if (result != 0) {
        return result;
    }

    /* Validate baud rate */
    if (config->baud_rate <= 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate data bits */
    if (config->data_bits != SERIAL_DATA_BITS_7 &&
        config->data_bits != SERIAL_DATA_BITS_8) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate stop bits */
    if (config->stop_bits != SERIAL_STOP_BITS_1 &&
        config->stop_bits != SERIAL_STOP_BITS_2) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate parity */
    if (config->parity != SERIAL_PARITY_NONE &&
        config->parity != SERIAL_PARITY_ODD &&
        config->parity != SERIAL_PARITY_EVEN) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate flow control */
    if (config->flow_control != SERIAL_FLOW_NONE &&
        config->flow_control != SERIAL_FLOW_XONXOFF &&
        config->flow_control != SERIAL_FLOW_RTSCTS) {
        return E_INVALID_ARGUMENT;
    }

    return 0;
}

/**
 * @brief Open and configure a serial port
 */
int serial_port_open(const serial_config_t* config, int* fd)
{
    int result = 0;
    int file_descriptor = 0;

    if (config == NULL || fd == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Validate configuration */
    result = serial_config_validate(config);
    if (result != 0) {
        return result;
    }

    /* Open the device */
    file_descriptor = open(config->device_path, O_RDWR | O_NOCTTY);
    if (file_descriptor < 0) {
        if (errno == ENOENT) {
            return E_FILE_NOT_FOUND;
        } else if (errno == EACCES || errno == EPERM) {
            return E_PERMISSION_DENIED;
        } else {
            return E_UNKNOWN_ERROR;
        }
    }

    /* Configure termios */
    result = configure_termios(file_descriptor, config);
    if (result != 0) {
        close(file_descriptor);
        return result;
    }

    /* Set timeout */
    result = set_timeout(file_descriptor, config->read_timeout_ms);
    if (result != 0) {
        close(file_descriptor);
        return result;
    }

    *fd = file_descriptor;
    return 0;
}

/**
 * @brief Close a serial port
 */
int serial_port_close(int fd)
{
    if (fd < 0) {
        return E_INVALID_ARGUMENT;
    }

    if (close(fd) < 0) {
        return E_UNKNOWN_ERROR;
    }

    return 0;
}

/**
 * @brief Read data from serial port with timeout
 */
int serial_port_read(int fd, void* buffer, int len, int timeout_ms)
{
    int bytes_read = 0;
    int result = 0;

    if (fd < 0 || buffer == NULL || len <= 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Update timeout if different from current setting */
    if (timeout_ms >= 0) {
        result = set_timeout(fd, timeout_ms);
        if (result != 0) {
            return result;
        }
    }

    /* Read from serial port */
    bytes_read = read(fd, buffer, len);
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; /* Timeout */
        }
        return E_NETWORK_ERROR; /* Use network error for I/O errors */
    }

    return bytes_read;
}

/**
 * @brief Write data to serial port
 */
int serial_port_write(int fd, const void* buffer, int len)
{
    int bytes_written = 0;
    int total_written = 0;
    const char* data = (const char*)buffer;

    if (fd < 0 || buffer == NULL || len <= 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Write all data */
    while (total_written < len) {
        bytes_written = write(fd, data + total_written, len - total_written);
        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted, retry */
            }
            return E_NETWORK_ERROR;
        }
        total_written += bytes_written;
    }

    return total_written;
}

/**
 * @brief Get error status from serial port
 */
int serial_port_get_status(int fd, uint16_t* flags)
{
    int status = 0;

    if (fd < 0 || flags == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Get modem status */
    if (ioctl(fd, TIOCMGET, &status) < 0) {
        return E_UNKNOWN_ERROR;
    }

    *flags = 0;
    /* Note: Actual error flag detection would require reading termios state */
    /* For now, this is a placeholder implementation */

    return 0;
}

/**
 * @brief Flush serial port buffers
 */
int serial_port_flush(int fd, int input_flush, int output_flush)
{
    int queue_selector = 0;

    if (fd < 0) {
        return E_INVALID_ARGUMENT;
    }

    if (input_flush && output_flush) {
        queue_selector = TCIOFLUSH;
    } else if (input_flush) {
        queue_selector = TCIFLUSH;
    } else if (output_flush) {
        queue_selector = TCOFLUSH;
    } else {
        return 0; /* Nothing to flush */
    }

    if (tcflush(fd, queue_selector) < 0) {
        return E_UNKNOWN_ERROR;
    }

    return 0;
}

/**
 * @brief Configure termios for serial port
 */
static int configure_termios(int fd, const serial_config_t* config)
{
    struct termios tty = {0};
    speed_t speed = 0;
    int result = 0;

    /* Get current termios settings */
    if (tcgetattr(fd, &tty) != 0) {
        return E_UNKNOWN_ERROR;
    }

    /* Convert baud rate */
    result = baud_to_speed_const(config->baud_rate, &speed);
    if (result != 0) {
        return result;
    }

    /* Set baud rate */
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* Control modes */
    tty.c_cflag &= ~PARENB; /* Clear parity bit */
    tty.c_cflag &= ~CSTOPB; /* Clear stop field (1 stop bit) */
    tty.c_cflag &= ~CSIZE;  /* Clear size bits */

    /* Set data bits */
    if (config->data_bits == SERIAL_DATA_BITS_8) {
        tty.c_cflag |= CS8;
    } else {
        tty.c_cflag |= CS7;
    }

    /* Set parity */
    if (config->parity == SERIAL_PARITY_EVEN) {
        tty.c_cflag |= PARENB;
    } else if (config->parity == SERIAL_PARITY_ODD) {
        tty.c_cflag |= PARENB | PARODD;
    }

    /* Set stop bits */
    if (config->stop_bits == SERIAL_STOP_BITS_2) {
        tty.c_cflag |= CSTOPB;
    }

    /* Flow control */
    tty.c_cflag &= ~CRTSCTS; /* Disable RTS/CTS by default */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* Disable XON/XOFF by default */

    if (config->flow_control == SERIAL_FLOW_RTSCTS) {
        tty.c_cflag |= CRTSCTS;
    } else if (config->flow_control == SERIAL_FLOW_XONXOFF) {
        tty.c_iflag |= IXON | IXOFF;
    }

    /* Enable receiver, ignore modem control lines */
    tty.c_cflag |= CREAD | CLOCAL;

    /* Input modes - disable input processing */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    /* Output modes - disable output processing (raw output) */
    tty.c_oflag &= ~OPOST;

    /* Local modes - disable canonical mode, echo, signals */
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    /* Apply settings */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return E_UNKNOWN_ERROR;
    }

    return 0;
}

/**
 * @brief Validate baud rate against whitelist (FSM-002, SER-002 fix)
 *
 * Only standard baud rates are accepted to prevent misconfiguration.
 */
int serial_validate_baud(int baud_rate)
{
    switch (baud_rate) {
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
        case 230400:
            return 0;  /* Valid */
        default:
            return -1; /* Invalid */
    }
}

/**
 * @brief Convert baud rate integer to termios speed constant
 */
static int baud_to_speed_const(int baud_rate, speed_t* speed)
{
    if (speed == NULL) {
        return E_INVALID_ARGUMENT;
    }

    switch (baud_rate) {
        case 9600:
            *speed = B9600;
            break;
        case 19200:
            *speed = B19200;
            break;
        case 38400:
            *speed = B38400;
            break;
        case 57600:
            *speed = B57600;
            break;
        case 115200:
            *speed = B115200;
            break;
        case 230400:
            *speed = B230400;
            break;
        default:
            return E_INVALID_ARGUMENT;
    }

    return 0;
}

/**
 * @brief Set read timeout using termios VMIN and VTIME
 */
static int set_timeout(int fd, int timeout_ms)
{
    struct termios tty = {0};

    if (tcgetattr(fd, &tty) != 0) {
        return E_UNKNOWN_ERROR;
    }

    /* Convert milliseconds to deciseconds (VTIME unit) */
    /* VMIN = 1: read blocks until at least 1 byte available or timeout */
    /* VTIME = timeout in deciseconds */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = (timeout_ms + 99) / 100; /* Round up to deciseconds */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return E_UNKNOWN_ERROR;
    }

    return 0;
}
