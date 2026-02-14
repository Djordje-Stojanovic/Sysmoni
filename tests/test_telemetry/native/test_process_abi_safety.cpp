// Process Panel C ABI Safety Tests
//
// This file tests the C ABI contract for extended process telemetry functions.
// Tests verify proper handling of null pointers, invalid inputs, and buffer overflows.

#include "telemetry_abi.h"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

constexpr size_t kErrorBufferSize = 512;
constexpr uint32_t kTestPid = 999999;  // Unlikely to exist

bool expect_true(const bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}

bool contains(const std::string& text, const std::string& pattern) {
    return text.find(pattern) != std::string::npos;
}

// Test: aura_get_process_by_pid with null output pointer
bool test_get_process_by_pid_null_output() {
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = aura_get_process_by_pid(
        kTestPid,
        nullptr,
        error_buffer.data(),
        error_buffer.size()
    );
    bool ok = true;
    ok &= expect_true(status == AURA_STATUS_ERROR, "null output should return error");
    ok &= expect_true(contains(error_buffer.data(), "null") || contains(error_buffer.data(), "invalid"),
                      "error should mention null/invalid");
    return ok;
}

// Test: aura_get_process_by_pid with null error buffer
bool test_get_process_by_pid_null_error_buffer() {
    aura_process_detail detail{};
    const int status = aura_get_process_by_pid(
        kTestPid,
        &detail,
        nullptr,
        0
    );
    // Function should not crash with null error buffer
    return expect_true(status == AURA_STATUS_ERROR || status == AURA_STATUS_UNAVAILABLE,
                      "should not crash with null error buffer");
}

// Test: aura_terminate_process with PID 0
bool test_terminate_process_pid_zero() {
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = aura_terminate_process(
        0,
        1,
        error_buffer.data(),
        error_buffer.size()
    );
    bool ok = true;
    ok &= expect_true(status == AURA_STATUS_ERROR, "PID 0 should return error");
    ok &= expect_true(contains(error_buffer.data(), "0") || contains(error_buffer.data(), "invalid"),
                      "error should mention PID 0 or invalid");
    return ok;
}

// Test: aura_terminate_process with non-existent PID
bool test_terminate_process_not_found() {
    const uint32_t non_existent_pid = 9999999;
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = aura_terminate_process(
        non_existent_pid,
        1,
        error_buffer.data(),
        error_buffer.size()
    );
    // Should fail - cannot open process that doesn't exist
    return expect_true(status != AURA_STATUS_OK, "non-existent PID should fail");
}

// Test: aura_terminate_process with null error buffer
bool test_terminate_process_null_error_buffer() {
    const int status = aura_terminate_process(
        kTestPid,
        1,
        nullptr,
        0
    );
    // Function should not crash with null error buffer
    return expect_true(status == AURA_STATUS_ERROR || status == AURA_STATUS_UNAVAILABLE,
                      "should not crash with null error buffer");
}

// Test: aura_set_process_priority with PID 0
bool test_set_process_priority_pid_zero() {
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = aura_set_process_priority(
        0,
        AURA_PRIORITY_NORMAL,
        error_buffer.data(),
        error_buffer.size()
    );
    bool ok = true;
    ok &= expect_true(status == AURA_STATUS_ERROR, "PID 0 should return error");
    ok &= expect_true(contains(error_buffer.data(), "0") || contains(error_buffer.data(), "invalid"),
                      "error should mention PID 0 or invalid");
    return ok;
}

// Test: aura_set_process_priority with invalid priority value
bool test_set_process_priority_invalid_value() {
    std::array<char, kErrorBufferSize> error_buffer{};
    // Use an invalid priority value outside the enum range
    const int status = aura_set_process_priority(
        kTestPid,
        0x12345678,  // Invalid priority
        error_buffer.data(),
        error_buffer.size()
    );
    // Should fail - invalid priority value
    return expect_true(status != AURA_STATUS_OK, "invalid priority should fail");
}

// Test: aura_set_process_priority with null error buffer
bool test_set_process_priority_null_error_buffer() {
    const int status = aura_set_process_priority(
        kTestPid,
        AURA_PRIORITY_NORMAL,
        nullptr,
        0
    );
    // Function should not crash with null error buffer
    return expect_true(status == AURA_STATUS_ERROR || status == AURA_STATUS_UNAVAILABLE,
                      "should not crash with null error buffer");
}

// Test: Non-Windows platform returns UNAVAILABLE
#ifndef _WIN32
bool test_non_windows_unavailable() {
    std::array<char, kErrorBufferSize> error_buffer{};
    aura_process_detail detail{};
    const int status = aura_get_process_by_pid(
        kTestPid,
        &detail,
        error_buffer.data(),
        error_buffer.size()
    );
    bool ok = true;
    ok &= expect_true(status == AURA_STATUS_UNAVAILABLE, "non-Windows should return unavailable");
    ok &= expect_true(contains(error_buffer.data(), "unavailable"),
                      "error should mention unavailable");
    return ok;
}
#endif

}  // namespace

int main() {
    int failures = 0;

    // Run all tests
    if (!test_get_process_by_pid_null_output()) {
        ++failures;
    }
    if (!test_get_process_by_pid_null_error_buffer()) {
        ++failures;
    }
    if (!test_terminate_process_pid_zero()) {
        ++failures;
    }
    if (!test_terminate_process_not_found()) {
        ++failures;
    }
    if (!test_terminate_process_null_error_buffer()) {
        ++failures;
    }
    if (!test_set_process_priority_pid_zero()) {
        ++failures;
    }
    if (!test_set_process_priority_invalid_value()) {
        ++failures;
    }
    if (!test_set_process_priority_null_error_buffer()) {
        ++failures;
    }
#ifndef _WIN32
    if (!test_non_windows_unavailable()) {
        ++failures;
    }
#endif

    if (failures == 0) {
        std::cout << "All process ABI safety tests passed." << '\n';
        return 0;
    }

    std::cerr << failures << " process ABI safety tests failed." << '\n';
    return 1;
}
