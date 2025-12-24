/**
 * @file password_hash.h
 * @brief Password hashing API for secure credential storage
 *
 * Provides SHA-256 based password hashing with random salt to prevent
 * plaintext password storage. Used by management interface authentication.
 *
 * Security properties:
 * - Uses cryptographically secure random salt (16 bytes)
 * - SHA-256 hash output (32 bytes)
 * - Constant-time comparison to prevent timing attacks
 *
 * [LLM-ARCH]
 */

#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include "lib/common/types.h"

/* Hash output sizes */
#define PASSWORD_SALT_LEN  16
#define PASSWORD_HASH_LEN  32
#define PASSWORD_TOTAL_LEN (PASSWORD_SALT_LEN + PASSWORD_HASH_LEN)

/* Hex-encoded hash for storage (salt + hash as hex + null) */
#define PASSWORD_HEX_LEN   ((PASSWORD_TOTAL_LEN * 2) + 1)

/**
 * @brief Hash a password with random salt
 *
 * Generates a random 16-byte salt, then computes SHA-256(salt || password).
 * Output is hex-encoded: 32 chars salt + 64 chars hash + null.
 *
 * @param password  Plaintext password to hash
 * @param hash_out  Output buffer (must be PASSWORD_HEX_LEN bytes)
 * @return 0 on success, negative error code on failure
 */
int password_hash(const char* password, char* hash_out);

/**
 * @brief Verify a password against stored hash
 *
 * Extracts salt from stored hash, recomputes SHA-256(salt || password),
 * and compares in constant time.
 *
 * @param password    Plaintext password to verify
 * @param stored_hash Hex-encoded hash from password_hash()
 * @return 1 if password matches, 0 if not, negative on error
 */
int password_verify(const char* password, const char* stored_hash);

#endif /* PASSWORD_HASH_H */
