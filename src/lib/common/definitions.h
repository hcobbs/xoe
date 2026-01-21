#ifndef COMMON_DEFINITIONS_H
#define COMMON_DEFINITIONS_H

/* Boolean Logic Defines */
#define TRUE  1
#define FALSE 0

/* Success Code */
#define SUCCESS 0

/* Error Definitions */
#define E_UNKNOWN_ERROR        -1
#define E_NULL_POINTER         -26  /* NULL pointer argument */
#define E_INIT_FAILED          -27  /* Initialization failed */
#define E_INVALID_ARGUMENT     -2
#define E_OUT_OF_MEMORY        -3
#define E_FILE_NOT_FOUND       -4
#define E_PERMISSION_DENIED    -5
#define E_NETWORK_ERROR        -6
#define E_TIMEOUT              -7
#define E_INVALID_STATE        -8
#define E_NOT_IMPLEMENTED      -9
#define E_BUFFER_TOO_SMALL    -10

/* TLS-specific Error Definitions */
#define E_TLS_HANDSHAKE_FAILED -11
#define E_TLS_CERT_INVALID     -12
#define E_TLS_CERT_EXPIRED     -13
#define E_TLS_VERSION_MISMATCH -14
#define E_TLS_CIPHER_MISMATCH  -15
#define E_TLS_PROTOCOL_ERROR   -16

/* General protocol error (serial, USB, etc.) */
#define E_PROTOCOL_ERROR       -17
#define E_CHECKSUM_MISMATCH    -18
#define E_NOT_FOUND            -19
#define E_IO_ERROR             -20  /* I/O error */
#define E_DEVICE_BUSY          -21  /* Device busy */
#define E_INTERRUPTED          -22  /* Operation interrupted */
#define E_NOT_SUPPORTED        -23  /* Operation not supported */
#define E_USB_TRANSFER_ERROR   -24  /* Generic USB transfer error */
#define E_DNS_ERROR            -25  /* DNS resolution failed */

/* NBD-specific Error Definitions (start at -200) */
#define E_NBD_HANDSHAKE_FAILED  -200  /* NBD handshake negotiation failed */
#define E_NBD_INVALID_MAGIC     -201  /* Invalid protocol magic number */
#define E_NBD_BACKEND_ERROR     -202  /* Backend I/O error */
#define E_NBD_EXPORT_NOT_FOUND  -203  /* Export path doesn't exist */
#define E_NBD_READ_ONLY         -204  /* Write attempted on read-only export */
#define E_NBD_OFFSET_ERROR      -205  /* Offset beyond export size */
#define E_NBD_LENGTH_ERROR      -206  /* Request length too large */

/* USB-specific Error Definitions (start at -100) */
#define E_USB_NOT_FOUND        -100  /* USB device not found */
#define E_USB_ACCESS_DENIED    -101  /* Permission denied */
#define E_USB_BUSY             -102  /* Device or resource busy */
#define E_USB_NO_DEVICE        -103  /* Device disconnected */
#define E_USB_NOT_SUPPORTED    -104  /* Operation not supported */
#define E_USB_PIPE_ERROR       -105  /* Pipe/endpoint error */
#define E_USB_OVERFLOW         -106  /* Buffer overflow */
#define E_USB_TIMEOUT          -107  /* Transfer timeout */
#define E_USB_CANCELLED        -108  /* Transfer cancelled */
#define E_USB_STALL            -109  /* Endpoint stalled */
#define E_USB_PROTOCOL_ERROR   -110  /* Protocol error */

#endif /* COMMON_DEFINITIONS_H */
