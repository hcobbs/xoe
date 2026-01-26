/**
 * @file usb_auth.h
 * @brief USB device registration authentication API
 *
 * Provides HMAC-SHA256 based challenge-response authentication for
 * USB device registration. Prevents unauthorized device injection
 * attacks (e.g., HID keyboard injection).
 *
 * Security properties:
 * - 32-byte random challenge (nonce)
 * - HMAC-SHA256 response
 * - Constant-time comparison
 * - Device class whitelist
 * - HID class blocked by default
 *
 * [LLM-ARCH]
 */

#ifndef USB_AUTH_H
#define USB_AUTH_H

#include "lib/common/types.h"

/* Authentication constants */
#define USB_AUTH_CHALLENGE_LEN  32  /* Challenge nonce size */
#define USB_AUTH_RESPONSE_LEN   32  /* HMAC-SHA256 output size */
#define USB_AUTH_SECRET_MAX     64  /* Max shared secret length */

/* Authentication payload structure (wire format) */
typedef struct {
    uint8_t challenge[USB_AUTH_CHALLENGE_LEN];  /* Server-generated nonce */
    uint8_t response[USB_AUTH_RESPONSE_LEN];    /* HMAC response from client */
    uint32_t device_id;                         /* Device VID:PID */
    uint8_t device_class;                       /* USB device class */
    uint8_t padding[3];                         /* Alignment padding */
} __attribute__((packed)) usb_auth_payload_t;

/* Wire size for validation */
#define USB_AUTH_PAYLOAD_WIRE_SIZE 72

/**
 * @brief Generate random authentication challenge
 *
 * Generates a cryptographically secure 32-byte random nonce
 * using /dev/urandom.
 *
 * @param challenge  Output buffer (must be USB_AUTH_CHALLENGE_LEN bytes)
 * @return 0 on success, negative error code on failure
 */
int usb_auth_generate_challenge(uint8_t* challenge);

/**
 * @brief Compute HMAC-SHA256 response to authentication challenge
 *
 * Computes HMAC-SHA256(secret, challenge || device_id || device_class).
 * Used by both client (to generate response) and server (to verify).
 *
 * @param secret       Shared secret string
 * @param challenge    32-byte challenge from server
 * @param device_id    USB device identifier (VID:PID)
 * @param device_class USB device class code
 * @param response_out Output buffer (must be USB_AUTH_RESPONSE_LEN bytes)
 * @return 0 on success, negative error code on failure
 */
int usb_auth_compute_response(const char* secret,
                              const uint8_t* challenge,
                              uint32_t device_id,
                              uint8_t device_class,
                              uint8_t* response_out);

/**
 * @brief Verify authentication response
 *
 * Computes expected HMAC and compares to client response in constant time.
 *
 * @param secret          Shared secret string
 * @param challenge       32-byte challenge that was sent
 * @param device_id       USB device identifier
 * @param device_class    USB device class code
 * @param client_response 32-byte response from client
 * @return 1 if valid, 0 if invalid, negative on error
 */
int usb_auth_verify_response(const char* secret,
                             const uint8_t* challenge,
                             uint32_t device_id,
                             uint8_t device_class,
                             const uint8_t* client_response);

/**
 * @brief Check if device class is allowed by whitelist
 *
 * @param device_class      USB device class to check
 * @param allowed_classes   Array of allowed class codes
 * @param allowed_count     Number of entries in whitelist
 * @return 1 if allowed, 0 if blocked
 *
 * Note: If allowed_count is 0, all classes are allowed (no whitelist).
 *       USB_CLASS_ANY (0xFF) in whitelist allows all classes.
 *       HID class (0x03) requires explicit whitelist entry.
 */
int usb_auth_check_class_whitelist(uint8_t device_class,
                                   const uint8_t* allowed_classes,
                                   int allowed_count);

/**
 * @brief Log authentication event for audit trail
 *
 * @param client_ip    Client IP address string
 * @param device_id    USB device identifier
 * @param device_class USB device class
 * @param success      1 if auth succeeded, 0 if failed
 * @param reason       Human-readable reason (for failures)
 */
void usb_auth_log_event(const char* client_ip,
                        uint32_t device_id,
                        uint8_t device_class,
                        int success,
                        const char* reason);

#endif /* USB_AUTH_H */
