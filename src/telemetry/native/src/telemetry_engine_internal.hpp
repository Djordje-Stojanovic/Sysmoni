#pragma once

#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>

namespace aura {
namespace telemetry {
namespace detail {

inline constexpr double kCelsiusMin = -30.0;
inline constexpr double kCelsiusMax = 150.0;
inline constexpr double kCelsiusOptionalMax = 250.0;

inline bool is_finite(double value) {
    return std::isfinite(value) != 0;
}

inline double clamp_percent(double value) {
    if (!is_finite(value)) {
        return 0.0;
    }
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 100.0) {
        return 100.0;
    }
    return value;
}

inline std::string decode_error_buffer(const char* error_buffer, size_t error_buffer_len) {
    if (error_buffer == nullptr || error_buffer_len == 0) {
        return {};
    }
    size_t used = 0;
    while (used < error_buffer_len && error_buffer[used] != '\0') {
        ++used;
    }
    return std::string(error_buffer, used);
}

inline std::string build_status_error(
    const char* operation,
    int status,
    const char* error_buffer,
    size_t error_buffer_len
) {
    std::ostringstream oss;
    oss << operation << " failed with status=" << status;
    std::string native_message = decode_error_buffer(error_buffer, error_buffer_len);
    if (!native_message.empty()) {
        oss << ": " << native_message;
    }
    return oss.str();
}

inline std::string decode_fixed_utf8(const char* data, size_t data_len) {
    if (data == nullptr || data_len == 0) {
        return {};
    }
    size_t used = 0;
    while (used < data_len && data[used] != '\0') {
        ++used;
    }
    std::string decoded(data, used);
    const auto first_non_ws = decoded.find_first_not_of(" \t\r\n");
    if (first_non_ws == std::string::npos) {
        return {};
    }
    const auto last_non_ws = decoded.find_last_not_of(" \t\r\n");
    return decoded.substr(first_non_ws, last_non_ws - first_non_ws + 1);
}

inline void clear_error(std::string* error_message) {
    if (error_message != nullptr) {
        error_message->clear();
    }
}

}  // namespace detail
}  // namespace telemetry
}  // namespace aura
