/**
 * @file password_hash.c
 * @brief Password hashing implementation using OpenSSL
 *
 * Implements secure password hashing with SHA-256 and random salt.
 * Uses /dev/urandom for salt generation and OpenSSL for hashing.
 *
 * [LLM-ARCH]
 */

#include "password_hash.h"
#include "lib/common/definitions.h"
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/* Hex encoding lookup table */
static const char hex_chars[] = "0123456789abcdef";

/**
 * @brief Securely clear sensitive memory (NET-015 fix)
 *
 * Uses volatile pointer to prevent compiler optimization from removing the clear.
 * This is the C89-compatible approach to explicit_bzero().
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
    int fd = 0;
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
 * @brief Convert binary data to hex string
 */
static void bytes_to_hex(const uint8_t* bytes, size_t len, char* hex_out)
{
    size_t i;
    for (i = 0; i < len; i++) {
        hex_out[i * 2] = hex_chars[(bytes[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[bytes[i] & 0x0F];
    }
}

/**
 * @brief Convert hex string to binary data
 */
static int hex_to_bytes(const char* hex, uint8_t* bytes_out, size_t len)
{
    size_t i = 0;
    unsigned int byte = 0;

    for (i = 0; i < len; i++) {
        if (sscanf(hex + (i * 2), "%2x", &byte) != 1) {
            return E_INVALID_ARGUMENT;
        }
        bytes_out[i] = (uint8_t)byte;
    }
    return 0;
}

/**
 * @brief Compute SHA-256 hash of salt concatenated with password
 */
static int compute_hash(const uint8_t* salt, const char* password,
                        uint8_t* hash_out)
{
    EVP_MD_CTX* ctx = NULL;
    unsigned int hash_len = 0;
    size_t password_len = 0;
    int result = 0;

    if (salt == NULL || password == NULL || hash_out == NULL) {
        return E_INVALID_ARGUMENT;
    }

    password_len = strlen(password);

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return E_OUT_OF_MEMORY;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        result = E_UNKNOWN_ERROR;
        goto cleanup;
    }

    if (EVP_DigestUpdate(ctx, salt, PASSWORD_SALT_LEN) != 1) {
        result = E_UNKNOWN_ERROR;
        goto cleanup;
    }

    if (EVP_DigestUpdate(ctx, password, password_len) != 1) {
        result = E_UNKNOWN_ERROR;
        goto cleanup;
    }

    if (EVP_DigestFinal_ex(ctx, hash_out, &hash_len) != 1) {
        result = E_UNKNOWN_ERROR;
        goto cleanup;
    }

cleanup:
    EVP_MD_CTX_free(ctx);
    return result;
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

int password_hash(const char* password, char* hash_out)
{
    uint8_t salt[PASSWORD_SALT_LEN] = {0};
    uint8_t hash[PASSWORD_HASH_LEN] = {0};
    int result = 0;

    if (password == NULL || hash_out == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Generate random salt */
    result = generate_random_bytes(salt, PASSWORD_SALT_LEN);
    if (result != 0) {
        secure_zero(salt, sizeof(salt));
        return result;
    }

    /* Compute hash */
    result = compute_hash(salt, password, hash);
    if (result != 0) {
        secure_zero(salt, sizeof(salt));
        secure_zero(hash, sizeof(hash));
        return result;
    }

    /* Output as hex: salt (32 chars) + hash (64 chars) + null */
    bytes_to_hex(salt, PASSWORD_SALT_LEN, hash_out);
    bytes_to_hex(hash, PASSWORD_HASH_LEN, hash_out + (PASSWORD_SALT_LEN * 2));
    hash_out[PASSWORD_HEX_LEN - 1] = '\0';

    /* Clear intermediate buffers (NET-015 fix) */
    secure_zero(salt, sizeof(salt));
    secure_zero(hash, sizeof(hash));

    return 0;
}

int password_verify(const char* password, const char* stored_hash)
{
    uint8_t salt[PASSWORD_SALT_LEN] = {0};
    uint8_t stored_hash_bytes[PASSWORD_HASH_LEN] = {0};
    uint8_t computed_hash[PASSWORD_HASH_LEN] = {0};
    int result = 0;
    int match = 0;
    size_t hash_len = 0;

    if (password == NULL || stored_hash == NULL) {
        return E_INVALID_ARGUMENT;
    }

    /* Verify stored hash length */
    hash_len = strlen(stored_hash);
    if (hash_len != PASSWORD_HEX_LEN - 1) {
        return 0; /* Invalid hash format */
    }

    /* Extract salt from stored hash */
    result = hex_to_bytes(stored_hash, salt, PASSWORD_SALT_LEN);
    if (result != 0) {
        secure_zero(salt, sizeof(salt));
        return 0; /* Invalid hex */
    }

    /* Extract hash from stored hash */
    result = hex_to_bytes(stored_hash + (PASSWORD_SALT_LEN * 2),
                          stored_hash_bytes, PASSWORD_HASH_LEN);
    if (result != 0) {
        secure_zero(salt, sizeof(salt));
        secure_zero(stored_hash_bytes, sizeof(stored_hash_bytes));
        return 0; /* Invalid hex */
    }

    /* Compute hash with same salt */
    result = compute_hash(salt, password, computed_hash);
    if (result != 0) {
        secure_zero(salt, sizeof(salt));
        secure_zero(stored_hash_bytes, sizeof(stored_hash_bytes));
        secure_zero(computed_hash, sizeof(computed_hash));
        return result; /* Error */
    }

    /* Constant-time comparison */
    match = constant_time_compare(stored_hash_bytes, computed_hash,
                                  PASSWORD_HASH_LEN);

    /* Clear sensitive buffers (NET-015 fix) */
    secure_zero(salt, sizeof(salt));
    secure_zero(stored_hash_bytes, sizeof(stored_hash_bytes));
    secure_zero(computed_hash, sizeof(computed_hash));

    return match;
}
