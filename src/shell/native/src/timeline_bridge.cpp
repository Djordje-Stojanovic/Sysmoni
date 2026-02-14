#include "aura_shell/timeline_bridge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "aura_platform.h"
#endif

namespace aura::shell {

namespace {

constexpr int kStatusOk = 0;
constexpr double kReadOnlyRetentionSeconds = 60.0 * 60.0 * 24.0 * 365.0 * 10.0;

double clamp_percent(const double value) {
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

std::size_t c_string_length(const char* text, const std::size_t max_length) {
    if (text == nullptr) {
        return 0U;
    }
    std::size_t len = 0U;
    while (len < max_length && text[len] != '\0') {
        ++len;
    }
    return len;
}

#ifdef _WIN32
std::string narrow_from_wide(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string output(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
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

std::string format_windows_error(const DWORD code) {
    if (code == 0) {
        return {};
    }

    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD language_id = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const DWORD size =
        FormatMessageW(flags, nullptr, code, language_id, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    std::wstring message = L"error=" + std::to_wstring(code);
    if (size != 0 && buffer != nullptr) {
        message = buffer;
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
            message.pop_back();
        }
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return narrow_from_wide(message);
}

std::filesystem::path executable_directory() {
    std::array<wchar_t, 2048> buffer{};
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(buffer.data()).parent_path();
}

std::vector<std::filesystem::path> runtime_library_candidates() {
    const std::filesystem::path dll_name("aura_platform.dll");
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(dll_name);

    const auto exe_dir = executable_directory();
    if (!exe_dir.empty()) {
        candidates.push_back((exe_dir / dll_name).lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / "runtime" / "native" / "build" /
                              "Release" / dll_name)
                                 .lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / ".." / "build" / "runtime-native" /
                              "Release" / dll_name)
                                 .lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / ".." / "build" / "platform-native" /
                              "Release" / dll_name)
                                 .lexically_normal());
    }
    return candidates;
}

std::string aura_error_message(const aura_error_t& raw_error, const std::string& fallback) {
    const std::size_t len = c_string_length(raw_error.message, sizeof(raw_error.message));
    if (len > 0U) {
        return std::string(raw_error.message, len);
    }
    if (raw_error.code != AURA_OK) {
        return fallback + " (code=" + std::to_string(raw_error.code) + ")";
    }
    return fallback;
}
#endif

}  // namespace

struct TimelineBridge::Impl {
#ifdef _WIN32
    using StoreOpenFn = int (*)(const char*, double, aura_store_t**, aura_error_t*);
    using StoreCloseFn = int (*)(aura_store_t*);
    using QueryTimelineFn =
        int (*)(aura_store_t*, int, double, int, double, int, aura_snapshot_t*, int, int*, aura_error_t*);

    HMODULE module_handle{nullptr};
    StoreOpenFn store_open_fn{nullptr};
    StoreCloseFn store_close_fn{nullptr};
    QueryTimelineFn query_timeline_fn{nullptr};
    aura_store_t* open_store{nullptr};
    std::string open_store_path;
#endif
    bool loaded{false};
    std::string loaded_path;
    std::string load_error;
};

TimelineBridge::TimelineBridge() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    DWORD last_error_code = 0;
    for (const auto& candidate : runtime_library_candidates()) {
        const std::wstring path = candidate.wstring();
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr) {
            last_error_code = GetLastError();
            continue;
        }

        auto* store_open = reinterpret_cast<Impl::StoreOpenFn>(GetProcAddress(module, "aura_store_open"));
        auto* store_close = reinterpret_cast<Impl::StoreCloseFn>(GetProcAddress(module, "aura_store_close"));
        auto* query_timeline =
            reinterpret_cast<Impl::QueryTimelineFn>(GetProcAddress(module, "aura_dvr_query_timeline"));
        if (store_open == nullptr || store_close == nullptr || query_timeline == nullptr) {
            last_error_code = GetLastError();
            FreeLibrary(module);
            continue;
        }

        impl_->module_handle = module;
        impl_->store_open_fn = store_open;
        impl_->store_close_fn = store_close;
        impl_->query_timeline_fn = query_timeline;
        impl_->loaded = true;
        impl_->loaded_path = narrow_from_wide(path);
        impl_->load_error.clear();
        return;
    }

    impl_->load_error = "Unable to load aura_platform.dll";
    const std::string suffix = format_windows_error(last_error_code);
    if (!suffix.empty()) {
        impl_->load_error += ": " + suffix;
    }
#else
    impl_->load_error = "Timeline bridge is only supported on Windows.";
#endif
}

TimelineBridge::~TimelineBridge() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->open_store != nullptr && impl_->store_close_fn != nullptr) {
        impl_->store_close_fn(impl_->open_store);
        impl_->open_store = nullptr;
        impl_->open_store_path.clear();
    }
    if (impl_ != nullptr && impl_->module_handle != nullptr) {
        FreeLibrary(impl_->module_handle);
        impl_->module_handle = nullptr;
    }
#endif
}

bool TimelineBridge::available() const {
    return impl_ != nullptr && impl_->loaded;
}

std::vector<TimelinePoint> TimelineBridge::query_recent(
    const std::string& db_path,
    const double end_timestamp,
    const double window_seconds,
    const int resolution,
    std::string& error
) {
    error.clear();
    std::vector<TimelinePoint> output;

    if (db_path.empty()) {
        error = "db_path cannot be empty when querying timeline.";
        return output;
    }
    if (!std::isfinite(end_timestamp)) {
        error = "end_timestamp must be finite.";
        return output;
    }
    if (!std::isfinite(window_seconds) || window_seconds <= 0.0) {
        error = "window_seconds must be finite and greater than 0.";
        return output;
    }
    if (resolution < 2) {
        error = "resolution must be >= 2.";
        return output;
    }
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Timeline bridge unavailable.";
        return output;
    }

#ifdef _WIN32
    if (impl_->open_store != nullptr && impl_->open_store_path != db_path && impl_->store_close_fn != nullptr) {
        impl_->store_close_fn(impl_->open_store);
        impl_->open_store = nullptr;
        impl_->open_store_path.clear();
    }

    if (impl_->open_store == nullptr) {
        aura_store_t* opened_store = nullptr;
        aura_error_t open_error{};
        const int open_status =
            impl_->store_open_fn(db_path.c_str(), kReadOnlyRetentionSeconds, &opened_store, &open_error);
        if (open_status != kStatusOk || opened_store == nullptr) {
            error = aura_error_message(open_error, "Failed to open runtime timeline store.");
            return output;
        }
        impl_->open_store = opened_store;
        impl_->open_store_path = db_path;
    }

    const double start_timestamp = end_timestamp - window_seconds;
    if (!std::isfinite(start_timestamp)) {
        error = "Computed timeline start timestamp is invalid.";
        return output;
    }

    const int bounded_resolution = std::clamp(resolution, 2, 2048);
    const int out_capacity = std::clamp(bounded_resolution * 4, 64, 4096);
    std::vector<aura_snapshot_t> raw(static_cast<std::size_t>(out_capacity));
    aura_error_t query_error{};
    int out_count = 0;
    const int status = impl_->query_timeline_fn(
        impl_->open_store,
        1,
        start_timestamp,
        1,
        end_timestamp,
        bounded_resolution,
        raw.data(),
        out_capacity,
        &out_count,
        &query_error
    );
    if (status != kStatusOk) {
        error = aura_error_message(query_error, "Failed to query runtime timeline.");
        return output;
    }
    if (out_count < 0 || out_count > out_capacity) {
        error = "Runtime timeline query returned invalid count.";
        return output;
    }

    output.reserve(static_cast<std::size_t>(out_count));
    for (int i = 0; i < out_count; ++i) {
        const aura_snapshot_t& point = raw[static_cast<std::size_t>(i)];
        if (!std::isfinite(point.timestamp)) {
            continue;
        }
        TimelinePoint next;
        next.timestamp = point.timestamp;
        next.cpu_percent = clamp_percent(point.cpu_percent);
        next.memory_percent = clamp_percent(point.memory_percent);
        output.push_back(next);
    }
    std::sort(
        output.begin(),
        output.end(),
        [](const TimelinePoint& lhs, const TimelinePoint& rhs) {
            return lhs.timestamp < rhs.timestamp;
        }
    );
    return output;
#else
    error = "Timeline bridge is only supported on Windows.";
    return output;
#endif
}

std::string TimelineBridge::loaded_path() const {
    return impl_ != nullptr ? impl_->loaded_path : std::string{};
}

std::string TimelineBridge::load_error() const {
    return impl_ != nullptr ? impl_->load_error : std::string{};
}

}  // namespace aura::shell

