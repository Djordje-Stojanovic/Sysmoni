#include "telemetry_utils.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

void write_error(char* error_buffer, size_t error_buffer_len, const char* message) {
    if (error_buffer == nullptr || error_buffer_len == 0) {
        return;
    }
    error_buffer[0] = '\0';
    if (message == nullptr || message[0] == '\0') {
        return;
    }
    std::strncpy(error_buffer, message, error_buffer_len - 1);
    error_buffer[error_buffer_len - 1] = '\0';
}

#ifdef _WIN32
uint64_t filetime_to_uint64(const FILETIME& ft) {
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return static_cast<uint64_t>(value.QuadPart);
}

uint64_t now_100ns() {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    return filetime_to_uint64(now);
}

double clamp_percent(double value) {
    if (!std::isfinite(value)) {
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

std::string utf8_from_utf16(const wchar_t* input) {
    if (input == nullptr || input[0] == L'\0') {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        input,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        input,
        -1,
        output.data(),
        required,
        nullptr,
        nullptr
    );
    if (written <= 0) {
        return {};
    }

    if (!output.empty() && output.back() == '\0') {
        output.pop_back();
    }
    return output;
}

void write_utf8_name(char* destination, size_t destination_size, const std::string& value) {
    if (destination == nullptr || destination_size == 0) {
        return;
    }
    destination[0] = '\0';
    if (value.empty()) {
        return;
    }
    const size_t copy_len = std::min(destination_size - 1, value.size());
    std::memcpy(destination, value.data(), copy_len);
    destination[copy_len] = '\0';
}

int logical_cpu_count() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const DWORD cpus = info.dwNumberOfProcessors;
    if (cpus == 0) {
        return 1;
    }
    return static_cast<int>(cpus);
}
#endif
