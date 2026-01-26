/**
 * @file usb_auth.c
 * @brief USB device registration authentication implementation
 *
 * Implements HMAC-SHA256 based challenge-response authentication
 * using OpenSSL EVP API for cryptographic operations.
 *
 * [LLM-ARCH]
 */

#include "usb_auth.h"
#include "lib/common/definitions.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

/* USB device class constants (duplicated here to avoid circular includes) */
#define USB_CLASS_HID_LOCAL  0x03

/**
 * @brief Securely clear sensitive memory
 *
 * Uses volatile pointer to prevent compiler optimization.
 */
static void secure_zero(void* ptr, size_t len)
{
    volatile unsigned char* p = (volatile unsigned char*)ptr;
    while (len--) {
        *p++ = 0;
    }
}

/**
 * @brief Generate cryptographically secure random bytes
 */
static int generate_random_bytes(uint8_t* buffer, size_t len)
{
    int fd = -1;
    ssize_t bytes_read = 0;
    size_t total_read = 0;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return E_UNKNOWN_ERROR;
    }

    while (total_read < len) {
        bytes_read = read(fd, buffer + total_read, len - total_read);
        if (bytes_read <= 0) {
            close(fd);
            return E_UNKNOWN_ERROR;
        }
        total_read += (size_t)bytes_read;
    }

    close(fd);
    return 0;
}

/**
 * @brief Constant-time comparison to prevent timing attacks
 */
static int constant_time_compare(const uint8_t* a, const uint8_t* b, size_t len)
{
    size_t i = 0;
    uint8_t result = 0;

    for (i = 0; i < len; i++) {
        result |= a[i] ^ b[i];
    }

    return result == 0;
}

int usb_auth_generate_challenge(uint8_t* challenge)
{
    if (challenge == NULL) {
        return E_INVALID_ARGUMENT;
    }

    return generate_random_bytes(challenge, USB_AUTH_CHALLENGE_LEN);
}

int usb_auth_compute_response(const char* secret,
                              const uint8_t* challenge,
                              uint32_t device_id,
                              uint8_t device_class,
                              uint8_t* response_out)
{
    unsigned char message[USB_AUTH_CHALLENGE_LEN + 5];
    unsigned int hmac_len = 0;
    unsigned char* result = NULL;
    size_t secret_len = 0;

    if (secret == NULL || challenge == NULL || response_out == NULL) {
        return E_INVALID_ARGUMENT;
    }

    secret_len = strlen(secret);
    if (secret_len == 0) {
        return E_INVALID_ARGUMENT;
    }

    /* Build message: challenge || device_id (4 bytes BE) || device_class (1 byte) */
    memcpy(message, challenge, USB_AUTH_CHALLENGE_LEN);
    message[USB_AUTH_CHALLENGE_LEN + 0] = (uint8_t)((device_id >> 24) & 0xFF);
    message[USB_AUTH_CHALLENGE_LEN + 1] = (uint8_t)((device_id >> 16) & 0xFF);
    message[USB_AUTH_CHALLENGE_LEN + 2] = (uint8_t)((device_id >> 8) & 0xFF);
    message[USB_AUTH_CHALLENGE_LEN + 3] = (uint8_t)(device_id & 0xFF);
    message[USB_AUTH_CHALLENGE_LEN + 4] = device_class;

    /* Compute HMAC-SHA256 */
    result = HMAC(EVP_sha256(),
                  secret, (int)secret_len,
                  message, sizeof(message),
                  response_out, &hmac_len);

    /* Clear message buffer */
    secure_zero(message, sizeof(message));

    if (result == NULL || hmac_len != USB_AUTH_RESPONSE_LEN) {
        return E_UNKNOWN_ERROR;
    }

    return 0;
}

int usb_auth_verify_response(const char* secret,
                             const uint8_t* challenge,
                             uint32_t device_id,
                             uint8_t device_class,
                             const uint8_t* client_response)
{
    uint8_t expected_response[USB_AUTH_RESPONSE_LEN];
    int result = 0;
    int match = 0;

    if (secret == NULL || challenge == NULL || client_response == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Compute expected response */
    result = usb_auth_compute_response(secret, challenge, device_id,
                                        device_class, expected_response);
    if (result != 0) {
        secure_zero(expected_response, sizeof(expected_response));
        return result;
    }

    /* Constant-time comparison */
    match = constant_time_compare(expected_response, client_response,
                                  USB_AUTH_RESPONSE_LEN);

    /* Clear sensitive buffer */
    secure_zero(expected_response, sizeof(expected_response));

    return match;
}

int usb_auth_check_class_whitelist(uint8_t device_class,
                                   const uint8_t* allowed_classes,
                                   int allowed_count)
{
    int i = 0;

    /* If no whitelist, apply default policy: block HID only */
    if (allowed_count == 0 || allowed_classes == NULL) {
        /* HID class (0x03) blocked by default for security */
        if (device_class == USB_CLASS_HID_LOCAL) {
            return 0;  /* Blocked */
        }
        return 1;  /* All other classes allowed */
    }

    /* Check whitelist */
    for (i = 0; i < allowed_count; i++) {
        if (allowed_classes[i] == 0xFF) {
            /* USB_CLASS_ANY allows everything */
            return 1;
        }
        if (allowed_classes[i] == device_class) {
            return 1;  /* Explicitly allowed */
        }
    }

    return 0;  /* Not in whitelist */
}

void usb_auth_log_event(const char* client_ip,
                        uint32_t device_id,
                        uint8_t device_class,
                        int success,
                        const char* reason)
{
    time_t now = 0;
    struct tm* tm_info = NULL;
    char time_buf[32];
    uint16_t vid = 0;
    uint16_t pid = 0;

    now = time(NULL);
    tm_info = localtime(&now);

    if (tm_info != NULL) {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(time_buf, sizeof(time_buf), "(time error)");
    }

    vid = (uint16_t)((device_id >> 16) & 0xFFFF);
    pid = (uint16_t)(device_id & 0xFFFF);

    if (success) {
        printf("[%s] USB AUTH: SUCCESS - IP=%s VID:PID=%04x:%04x class=0x%02x\n",
               time_buf,
               client_ip ? client_ip : "(unknown)",
               vid, pid, device_class);
    } else {
        fprintf(stderr, "[%s] USB AUTH: FAILED - IP=%s VID:PID=%04x:%04x "
                "class=0x%02x reason=%s\n",
                time_buf,
                client_ip ? client_ip : "(unknown)",
                vid, pid, device_class,
                reason ? reason : "(unknown)");
    }
}
