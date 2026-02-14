#include "aura_shell/telemetry_bridge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "telemetry_abi.h"
#endif

namespace aura::shell {

namespace {

constexpr std::size_t kErrorBufferSize = 256;
constexpr int kStatusOk = 0;
constexpr std::size_t kMaxProcessSamples = 64;

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

std::vector<std::filesystem::path> telemetry_library_candidates() {
    const std::filesystem::path dll_name("aura_telemetry_native.dll");
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(dll_name);

    const auto exe_dir = executable_directory();
    if (!exe_dir.empty()) {
        candidates.push_back((exe_dir / dll_name).lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / "telemetry" / "native" / "build" /
                              "Release" / dll_name)
                                 .lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / ".." / "build" / "telemetry-native" /
                              "Release" / dll_name)
                                 .lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / ".." / "build" / "telemetry-native-tests" /
                              "Release" / dll_name)
                                 .lexically_normal());
    }
    return candidates;
}
#endif

}  // namespace

struct TelemetryBridge::Impl {
#ifdef _WIN32
    using CollectSnapshotFn = int (*)(double*, double*, char*, std::size_t);
    using CollectProcessesFn = int (*)(aura_process_sample*, std::uint32_t, std::uint32_t*, char*, std::size_t);

    HMODULE module_handle{nullptr};
    CollectSnapshotFn collect_snapshot_fn{nullptr};
    CollectProcessesFn collect_processes_fn{nullptr};
#endif
    bool loaded{false};
    std::string loaded_path;
    std::string load_error;
};

TelemetryBridge::TelemetryBridge() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    DWORD last_error_code = 0;
    for (const auto& candidate : telemetry_library_candidates()) {
        const std::wstring path = candidate.wstring();
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr) {
            last_error_code = GetLastError();
            continue;
        }

        auto* collect_snapshot = reinterpret_cast<Impl::CollectSnapshotFn>(
            GetProcAddress(module, "aura_collect_system_snapshot")
        );
        auto* collect_processes = reinterpret_cast<Impl::CollectProcessesFn>(
            GetProcAddress(module, "aura_collect_processes")
        );
        if (collect_snapshot == nullptr || collect_processes == nullptr) {
            last_error_code = GetLastError();
            FreeLibrary(module);
            continue;
        }

        impl_->module_handle = module;
        impl_->collect_snapshot_fn = collect_snapshot;
        impl_->collect_processes_fn = collect_processes;
        impl_->loaded = true;
        impl_->loaded_path = narrow_from_wide(path);
        impl_->load_error.clear();
        return;
    }

    impl_->load_error = "Unable to load aura_telemetry_native.dll";
    const std::string suffix = format_windows_error(last_error_code);
    if (!suffix.empty()) {
        impl_->load_error += ": " + suffix;
    }
#else
    impl_->load_error = "Telemetry bridge is only supported on Windows.";
#endif
}

TelemetryBridge::~TelemetryBridge() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->module_handle != nullptr) {
        FreeLibrary(impl_->module_handle);
        impl_->module_handle = nullptr;
    }
#endif
}

bool TelemetryBridge::available() const {
    return impl_ != nullptr && impl_->loaded;
}

std::optional<TelemetrySnapshot> TelemetryBridge::collect_snapshot(std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    std::array<char, kErrorBufferSize> error_buffer{};
    double cpu_percent = 0.0;
    double memory_percent = 0.0;
    const int status = impl_->collect_snapshot_fn(
        &cpu_percent,
        &memory_percent,
        error_buffer.data(),
        error_buffer.size()
    );
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        if (error.empty()) {
            error = "Telemetry snapshot collection failed.";
        }
        return std::nullopt;
    }

    TelemetrySnapshot snapshot;
    snapshot.cpu_percent = clamp_percent(cpu_percent);
    snapshot.memory_percent = clamp_percent(memory_percent);
    return snapshot;
#else
    error = "Telemetry bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::vector<ProcessSample> TelemetryBridge::collect_top_processes(
    const std::size_t max_samples,
    std::string& error
) {
    error.clear();
    std::vector<ProcessSample> output;
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return output;
    }
    if (max_samples == 0U) {
        return output;
    }

#ifdef _WIN32
    const std::size_t bounded_samples = std::min(max_samples, kMaxProcessSamples);
    std::vector<aura_process_sample> raw_samples(bounded_samples);
    std::array<char, kErrorBufferSize> error_buffer{};
    std::uint32_t out_count = 0U;
    const int status = impl_->collect_processes_fn(
        raw_samples.data(),
        static_cast<std::uint32_t>(raw_samples.size()),
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        if (error.empty()) {
            error = "Telemetry process collection failed.";
        }
        return output;
    }

    const std::size_t result_count = std::min<std::size_t>(out_count, raw_samples.size());
    output.reserve(result_count);
    for (std::size_t i = 0; i < result_count; ++i) {
        const aura_process_sample& raw = raw_samples[i];
        ProcessSample sample;
        sample.pid = raw.pid;
        const std::size_t name_len = c_string_length(raw.name, sizeof(raw.name));
        sample.name.assign(raw.name, name_len);
        if (sample.name.empty()) {
            sample.name = "pid-" + std::to_string(raw.pid);
        }
        sample.cpu_percent = clamp_percent(raw.cpu_percent);
        sample.memory_rss_bytes = raw.memory_rss_bytes;
        output.push_back(std::move(sample));
    }
#else
    error = "Telemetry bridge is only supported on Windows.";
#endif
    return output;
}

std::string TelemetryBridge::loaded_path() const {
    return impl_ != nullptr ? impl_->loaded_path : std::string{};
}

std::string TelemetryBridge::load_error() const {
    return impl_ != nullptr ? impl_->load_error : std::string{};
}

}  // namespace aura::shell
