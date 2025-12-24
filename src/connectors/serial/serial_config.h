/**
 * @file serial_config.h
 * @brief Serial port configuration structures and constants
 *
 * This file defines the configuration structures, constants, and enumerations
 * for serial port communication in the XOE serial connector.
 *
 * [LLM-ASSISTED]
 */

#ifndef SERIAL_CONFIG_H
#define SERIAL_CONFIG_H

/* Maximum path length for device names */
#define SERIAL_DEVICE_PATH_MAX 256

/* Default serial port settings */
#define SERIAL_DEFAULT_BAUD 9600
#define SERIAL_DEFAULT_DATA_BITS 8
#define SERIAL_DEFAULT_STOP_BITS 1
#define SERIAL_DEFAULT_PARITY SERIAL_PARITY_NONE
#define SERIAL_DEFAULT_FLOW SERIAL_FLOW_NONE
#define SERIAL_DEFAULT_TIMEOUT_MS 100

/* Serial buffer sizes */
#define SERIAL_READ_CHUNK_SIZE 256
#define SERIAL_WRITE_CHUNK_SIZE 256

/* Parity options */
#define SERIAL_PARITY_NONE 0
#define SERIAL_PARITY_ODD 1
#define SERIAL_PARITY_EVEN 2

/* Flow control options */
#define SERIAL_FLOW_NONE 0
#define SERIAL_FLOW_XONXOFF 1
#define SERIAL_FLOW_RTSCTS 2

/* Data bits options */
#define SERIAL_DATA_BITS_7 7
#define SERIAL_DATA_BITS_8 8

/* Stop bits options */
#define SERIAL_STOP_BITS_1 1
#define SERIAL_STOP_BITS_2 2

/* Standard baud rates */
#define SERIAL_BAUD_9600 9600
#define SERIAL_BAUD_19200 19200
#define SERIAL_BAUD_38400 38400
#define SERIAL_BAUD_57600 57600
#define SERIAL_BAUD_115200 115200
#define SERIAL_BAUD_230400 230400

/**
 * @brief Validate baud rate against whitelist (FSM-002, SER-002 fix)
 * @param baud_rate Baud rate to validate
 * @return 0 if valid, -1 if invalid
 *
 * Only standard baud rates are accepted: 9600, 19200, 38400, 57600, 115200, 230400
 */
int serial_validate_baud(int baud_rate);

/**
 * @brief Serial port configuration structure
 *
 * Contains all configuration parameters for a serial port connection.
 */
typedef struct {
    char device_path[SERIAL_DEVICE_PATH_MAX];  /* Device path (e.g., /dev/ttyUSB0) */
    int baud_rate;                              /* Baud rate (e.g., 9600, 115200) */
    int data_bits;                              /* Data bits (7 or 8) */
    int stop_bits;                              /* Stop bits (1 or 2) */
    int parity;                                 /* Parity (NONE, ODD, EVEN) */
    int flow_control;                           /* Flow control (NONE, XONXOFF, RTSCTS) */
    int read_timeout_ms;                        /* Read timeout in milliseconds */
} serial_config_t;

/**
 * @brief Initialize serial configuration with default values
 * @param config Pointer to configuration structure to initialize
 * @return 0 on success, negative error code on failure
 */
int serial_config_init_defaults(serial_config_t* config);

/**
 * @brief Validate serial configuration parameters
 * @param config Pointer to configuration structure to validate
 * @return 0 if valid, negative error code if invalid
 */
int serial_config_validate(const serial_config_t* config);

/**
 * @brief Convert baud rate integer to termios speed constant
 * @param baud_rate Baud rate as integer (e.g., 9600)
 * @param speed Output parameter for termios speed constant
 * @return 0 on success, negative error code if unsupported baud rate
 */
int serial_config_baud_to_speed(int baud_rate, unsigned int* speed);

#endif /* SERIAL_CONFIG_H */
