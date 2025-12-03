#ifndef COMMON_DEFINITIONS_H
#define COMMON_DEFINITIONS_H

/* Boolean Logic Defines */
#define TRUE  1
#define FALSE 0

/* Error Definitions */
#define E_UNKNOWN_ERROR        -1
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

#endif /* COMMON_DEFINITIONS_H */
