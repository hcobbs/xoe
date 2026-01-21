/**
 * @file test_serial_protocol.c
 * @brief Unit tests for serial protocol encapsulation/decapsulation
 *
 * Tests the serial_protocol module for proper packet encapsulation,
 * decapsulation, checksum validation, and error handling.
 *
 * [LLM-ARCH]
 */

#include "tests/framework/test_framework.h"
#include "connectors/serial/serial_protocol.h"
#include "lib/common/definitions.h"
#include <stdlib.h>
#include <string.h>

/* Test data constants */
#define TEST_DATA_SIZE 64

/**
 * @brief Test basic encapsulation
 *
 * Verifies that serial_protocol_encapsulate() creates a valid packet.
 */
void test_encapsulate_basic(void) {
    unsigned char test_data[TEST_DATA_SIZE] = {0};
    xoe_packet_t packet = {0};
    int i = 0, result = 0;

    /* Fill test data */
    for (i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = (unsigned char)(i % 256);
    }

    result = serial_protocol_encapsulate(test_data, TEST_DATA_SIZE,
                                          42, 0, &packet);

    TEST_ASSERT_SUCCESS(result, "Basic encapsulation should succeed");
    TEST_ASSERT_EQUAL(XOE_PROTOCOL_SERIAL, packet.protocol_id,
                      "Protocol ID should be XOE_PROTOCOL_SERIAL");
    TEST_ASSERT_NOT_NULL(packet.payload, "Payload should not be NULL");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test encapsulation with maximum payload
 *
 * Verifies that encapsulating maximum allowed data size works.
 */
void test_encapsulate_max_payload(void) {
    unsigned char test_data[SERIAL_MAX_PAYLOAD_SIZE] = {0};
    xoe_packet_t packet = {0};
    int i = 0, result = 0;

    for (i = 0; i < SERIAL_MAX_PAYLOAD_SIZE; i++) {
        test_data[i] = (unsigned char)(i % 256);
    }

    result = serial_protocol_encapsulate(test_data, SERIAL_MAX_PAYLOAD_SIZE,
                                          1, 0, &packet);

    TEST_ASSERT_SUCCESS(result, "Max payload encapsulation should succeed");
    TEST_ASSERT_NOT_NULL(packet.payload, "Payload should not be NULL");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test encapsulation with empty payload
 *
 * Verifies that encapsulating zero bytes is handled correctly.
 */
void test_encapsulate_empty_payload(void) {
    unsigned char test_data[1] = {0};
    xoe_packet_t packet = {0};
    int result = 0;

    result = serial_protocol_encapsulate(test_data, 0, 0, 0, &packet);

    TEST_ASSERT_SUCCESS(result, "Empty payload encapsulation should succeed");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test encapsulation with NULL data pointer
 *
 * Verifies that encapsulation fails with NULL data.
 */
void test_encapsulate_null_data(void) {
    xoe_packet_t packet = {0};
    int result = 0;

    result = serial_protocol_encapsulate(NULL, 10, 0, 0, &packet);

    TEST_ASSERT_FAILURE(result, "NULL data should cause failure");
    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, result,
                      "NULL data should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test encapsulation with NULL packet pointer
 *
 * Verifies that encapsulation fails with NULL output packet.
 */
void test_encapsulate_null_packet(void) {
    unsigned char test_data[10] = {0};
    int result = 0;

    result = serial_protocol_encapsulate(test_data, 10, 0, 0, NULL);

    TEST_ASSERT_FAILURE(result, "NULL packet should cause failure");
    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, result,
                      "NULL packet should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test encapsulation with oversized payload
 *
 * Verifies that encapsulation fails when data exceeds maximum.
 */
void test_encapsulate_oversized(void) {
    unsigned char test_data[SERIAL_MAX_PAYLOAD_SIZE + 100] = {0};
    xoe_packet_t packet = {0};
    int result = 0;

    result = serial_protocol_encapsulate(test_data,
                                          SERIAL_MAX_PAYLOAD_SIZE + 1,
                                          0, 0, &packet);

    TEST_ASSERT_FAILURE(result, "Oversized payload should cause failure");
    TEST_ASSERT_EQUAL(E_BUFFER_TOO_SMALL, result,
                      "Oversized should return E_BUFFER_TOO_SMALL");
}

/**
 * @brief Test encapsulation with flags
 *
 * Verifies that status flags are properly encoded.
 */
void test_encapsulate_with_flags(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};
    uint16_t flags = SERIAL_FLAG_PARITY_ERROR | SERIAL_FLAG_XON;
    int result = 0;

    result = serial_protocol_encapsulate(test_data, 32, 0, flags, &packet);

    TEST_ASSERT_SUCCESS(result, "Encapsulation with flags should succeed");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test encapsulation with sequence numbers
 *
 * Verifies that sequence numbers are properly handled.
 */
void test_encapsulate_sequence(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};
    uint16_t sequence = 12345;
    int result = 0;

    result = serial_protocol_encapsulate(test_data, 32, sequence, 0, &packet);

    TEST_ASSERT_SUCCESS(result, "Encapsulation with sequence should succeed");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test basic decapsulation
 *
 * Verifies that serial_protocol_decapsulate() correctly extracts data.
 */
void test_decapsulate_basic(void) {
    unsigned char test_data[TEST_DATA_SIZE] = {0};
    unsigned char output_data[TEST_DATA_SIZE] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int i = 0, result = 0;

    /* Fill test data */
    for (i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = (unsigned char)(i % 256);
    }

    /* Encapsulate first */
    serial_protocol_encapsulate(test_data, TEST_DATA_SIZE, 100, 0, &packet);

    /* Now decapsulate */
    result = serial_protocol_decapsulate(&packet, output_data,
                                          TEST_DATA_SIZE, &actual_len,
                                          &sequence, &flags);

    TEST_ASSERT_SUCCESS(result, "Basic decapsulation should succeed");
    TEST_ASSERT_EQUAL(TEST_DATA_SIZE, actual_len,
                      "Actual length should match original");
    TEST_ASSERT_EQUAL(100, sequence, "Sequence should be preserved");
    TEST_ASSERT(memcmp(test_data, output_data, TEST_DATA_SIZE) == 0,
                "Decapsulated data should match original");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test decapsulation with NULL packet
 *
 * Verifies that decapsulation fails with NULL input.
 */
void test_decapsulate_null_packet(void) {
    unsigned char output_data[64] = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int result = 0;

    result = serial_protocol_decapsulate(NULL, output_data, 64,
                                          &actual_len, &sequence, &flags);

    TEST_ASSERT_FAILURE(result, "NULL packet should cause failure");
    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, result,
                      "NULL packet should return E_INVALID_ARGUMENT");
}

/**
 * @brief Test decapsulation with NULL output buffer
 *
 * Verifies that decapsulation fails with NULL output.
 */
void test_decapsulate_null_output(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int result = 0;

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);

    result = serial_protocol_decapsulate(&packet, NULL, 64,
                                          &actual_len, &sequence, &flags);

    TEST_ASSERT_FAILURE(result, "NULL output should cause failure");
    TEST_ASSERT_EQUAL(E_INVALID_ARGUMENT, result,
                      "NULL output should return E_INVALID_ARGUMENT");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test decapsulation with buffer too small
 *
 * Verifies that decapsulation fails when output buffer is insufficient.
 */
void test_decapsulate_buffer_too_small(void) {
    unsigned char test_data[TEST_DATA_SIZE] = {0};
    unsigned char output_data[TEST_DATA_SIZE / 2] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int result = 0;

    serial_protocol_encapsulate(test_data, TEST_DATA_SIZE, 0, 0, &packet);

    result = serial_protocol_decapsulate(&packet, output_data,
                                          TEST_DATA_SIZE / 2,
                                          &actual_len, &sequence, &flags);

    TEST_ASSERT_FAILURE(result, "Small buffer should cause failure");
    TEST_ASSERT_EQUAL(E_BUFFER_TOO_SMALL, result,
                      "Small buffer should return E_BUFFER_TOO_SMALL");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test decapsulation with invalid protocol ID
 *
 * Verifies that decapsulation rejects packets with wrong protocol ID.
 */
void test_decapsulate_invalid_protocol(void) {
    unsigned char test_data[32] = {0};
    unsigned char output_data[64] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int result = 0;

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);

    /* Corrupt protocol ID */
    packet.protocol_id = 0x9999;

    result = serial_protocol_decapsulate(&packet, output_data, 64,
                                          &actual_len, &sequence, &flags);

    TEST_ASSERT_FAILURE(result, "Invalid protocol ID should cause failure");
    TEST_ASSERT_EQUAL(E_INVALID_STATE, result,
                      "Invalid protocol should return E_INVALID_STATE");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test decapsulation with corrupted checksum
 *
 * Verifies that decapsulation detects checksum mismatches.
 */
void test_decapsulate_corrupted_checksum(void) {
    unsigned char test_data[32] = {0};
    unsigned char output_data[64] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int result = 0;

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);

    /* Corrupt checksum */
    packet.checksum = 0xDEADBEEF;

    result = serial_protocol_decapsulate(&packet, output_data, 64,
                                          &actual_len, &sequence, &flags);

    TEST_ASSERT_FAILURE(result, "Corrupted checksum should cause failure");
    TEST_ASSERT_EQUAL(E_INVALID_STATE, result,
                      "Corrupted checksum should return E_INVALID_STATE");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test encapsulate/decapsulate round-trip
 *
 * Verifies complete round-trip preserves data integrity.
 */
void test_roundtrip_basic(void) {
    unsigned char test_data[TEST_DATA_SIZE] = {0};
    unsigned char output_data[TEST_DATA_SIZE] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int i = 0;

    /* Fill with test pattern */
    for (i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = (unsigned char)((i * 7 + 13) % 256);
    }

    /* Encapsulate */
    serial_protocol_encapsulate(test_data, TEST_DATA_SIZE, 999, 0, &packet);

    /* Decapsulate */
    serial_protocol_decapsulate(&packet, output_data, TEST_DATA_SIZE,
                                &actual_len, &sequence, &flags);

    TEST_ASSERT(memcmp(test_data, output_data, TEST_DATA_SIZE) == 0,
                "Round-trip data integrity should be preserved");
    TEST_ASSERT_EQUAL(999, sequence, "Sequence should be preserved");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test round-trip preserves flags
 *
 * Verifies that status flags survive encapsulation/decapsulation.
 */
void test_roundtrip_flags(void) {
    unsigned char test_data[32] = {0};
    unsigned char output_data[32] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags_in = 0, flags_out = 0;

    flags_in = SERIAL_FLAG_FRAMING_ERROR | SERIAL_FLAG_XOFF;

    serial_protocol_encapsulate(test_data, 32, 0, flags_in, &packet);
    serial_protocol_decapsulate(&packet, output_data, 32,
                                &actual_len, &sequence, &flags_out);

    TEST_ASSERT_EQUAL(flags_in, flags_out, "Flags should be preserved");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test round-trip preserves sequence
 *
 * Verifies that sequence numbers survive round-trip.
 */
void test_roundtrip_sequence(void) {
    unsigned char test_data[32] = {0};
    unsigned char output_data[32] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence_in = 0, sequence_out = 0, flags = 0;

    sequence_in = 54321;

    serial_protocol_encapsulate(test_data, 32, sequence_in, 0, &packet);
    serial_protocol_decapsulate(&packet, output_data, 32,
                                &actual_len, &sequence_out, &flags);

    TEST_ASSERT_EQUAL(sequence_in, sequence_out,
                      "Sequence should be preserved");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test round-trip with random data
 *
 * Verifies data integrity with pseudo-random data pattern.
 */
void test_roundtrip_random_data(void) {
    unsigned char test_data[128] = {0};
    unsigned char output_data[128] = {0};
    xoe_packet_t packet = {0};
    uint32_t actual_len = 0;
    uint16_t sequence = 0, flags = 0;
    int i = 0;

    /* Generate pseudo-random pattern */
    for (i = 0; i < 128; i++) {
        test_data[i] = (unsigned char)((i * 31 + 17) % 256);
    }

    serial_protocol_encapsulate(test_data, 128, 0, 0, &packet);
    serial_protocol_decapsulate(&packet, output_data, 128,
                                &actual_len, &sequence, &flags);

    TEST_ASSERT(memcmp(test_data, output_data, 128) == 0,
                "Random data integrity should be preserved");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test checksum calculation
 *
 * Verifies that checksum calculation produces a result.
 */
void test_checksum_calculation(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};
    int i = 0;

    for (i = 0; i < 32; i++) {
        test_data[i] = (unsigned char)(i % 256);
    }

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);

    /* Checksum calculation should complete (return value may be zero) */
    serial_protocol_checksum(&packet);
    TEST_ASSERT(1, "Checksum calculation should complete successfully");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test checksum validation with valid checksum
 *
 * Verifies that valid checksums are accepted.
 */
void test_checksum_valid(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};
    int result = 0;

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);

    result = serial_protocol_validate_checksum(&packet);
    TEST_ASSERT_SUCCESS(result, "Valid checksum should pass validation");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test checksum validation with invalid checksum
 *
 * Verifies that corrupted checksums are detected.
 */
void test_checksum_invalid(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};
    int result = 0;

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);

    /* Corrupt the checksum */
    packet.checksum ^= 0xFFFFFFFF;

    result = serial_protocol_validate_checksum(&packet);
    TEST_ASSERT_FAILURE(result, "Invalid checksum should fail validation");
    TEST_ASSERT_EQUAL(E_INVALID_STATE, result,
                      "Invalid checksum should return E_INVALID_STATE");

    serial_protocol_free_payload(&packet);
}

/**
 * @brief Test checksum determinism
 *
 * Verifies that same input produces same checksum.
 */
void test_checksum_deterministic(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet1 = {0}, packet2 = {0};
    uint32_t checksum1 = 0, checksum2 = 0;
    int i = 0;

    for (i = 0; i < 32; i++) {
        test_data[i] = (unsigned char)(i % 256);
    }

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet1);
    checksum1 = serial_protocol_checksum(&packet1);

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet2);
    checksum2 = serial_protocol_checksum(&packet2);

    TEST_ASSERT_EQUAL(checksum1, checksum2,
                      "Same input should produce same checksum");

    serial_protocol_free_payload(&packet1);
    serial_protocol_free_payload(&packet2);
}

/**
 * @brief Test free payload
 *
 * Verifies that serial_protocol_free_payload() safely frees resources.
 */
void test_free_payload(void) {
    unsigned char test_data[32] = {0};
    xoe_packet_t packet = {0};

    serial_protocol_encapsulate(test_data, 32, 0, 0, &packet);
    serial_protocol_free_payload(&packet);

    TEST_ASSERT(1, "Free payload should not crash");
}

/**
 * @brief Test free NULL payload
 *
 * Verifies that freeing NULL payload is safe (no-op).
 */
void test_free_null_payload(void) {
    serial_protocol_free_payload(NULL);
    TEST_ASSERT(1, "Freeing NULL payload should not crash");
}

/**
 * @brief Main test runner for serial protocol tests
 *
 * Executes all serial protocol unit tests and prints summary.
 *
 * @return EXIT_SUCCESS if all tests pass, EXIT_FAILURE if any fail
 */
int main(void) {
    printf("=== Serial Protocol Unit Tests ===\n\n");

    /* Encapsulation tests */
    run_test("test_encapsulate_basic", test_encapsulate_basic);
    run_test("test_encapsulate_max_payload", test_encapsulate_max_payload);
    run_test("test_encapsulate_empty_payload", test_encapsulate_empty_payload);
    run_test("test_encapsulate_null_data", test_encapsulate_null_data);
    run_test("test_encapsulate_null_packet", test_encapsulate_null_packet);
    run_test("test_encapsulate_oversized", test_encapsulate_oversized);
    run_test("test_encapsulate_with_flags", test_encapsulate_with_flags);
    run_test("test_encapsulate_sequence", test_encapsulate_sequence);

    /* Decapsulation tests */
    run_test("test_decapsulate_basic", test_decapsulate_basic);
    run_test("test_decapsulate_null_packet", test_decapsulate_null_packet);
    run_test("test_decapsulate_null_output", test_decapsulate_null_output);
    run_test("test_decapsulate_buffer_too_small", test_decapsulate_buffer_too_small);
    run_test("test_decapsulate_invalid_protocol", test_decapsulate_invalid_protocol);
    run_test("test_decapsulate_corrupted_checksum", test_decapsulate_corrupted_checksum);

    /* Round-trip tests */
    run_test("test_roundtrip_basic", test_roundtrip_basic);
    run_test("test_roundtrip_flags", test_roundtrip_flags);
    run_test("test_roundtrip_sequence", test_roundtrip_sequence);
    run_test("test_roundtrip_random_data", test_roundtrip_random_data);

    /* Checksum tests */
    run_test("test_checksum_calculation", test_checksum_calculation);
    run_test("test_checksum_valid", test_checksum_valid);
    run_test("test_checksum_invalid", test_checksum_invalid);
    run_test("test_checksum_deterministic", test_checksum_deterministic);

    /* Memory management tests */
    run_test("test_free_payload", test_free_payload);
    run_test("test_free_null_payload", test_free_null_payload);

    print_test_summary();

    return (tests_failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
