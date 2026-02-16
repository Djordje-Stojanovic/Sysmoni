#include "aura_shell/telemetry_bridge.hpp"

#include <algorithm>
#include <array>
#include <chrono>
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
    using CollectPerCoreCpuFn = int (*)(double*, std::uint32_t, std::uint32_t*, char*, std::size_t);
    using CollectGpuFn = int (*)(aura_gpu_utilization*, char*, std::size_t);
    using CollectDiskFn = int (*)(aura_disk_counters*, char*, std::size_t);
    using CollectNetworkFn = int (*)(aura_network_counters*, char*, std::size_t);
    using CollectThermalFn = int (*)(aura_thermal_reading*, std::uint32_t, std::uint32_t*, char*, std::size_t);
    using CollectProcessDetailsFn = int (*)(const aura_process_query_options*,
        aura_process_detail*, std::uint32_t, std::uint32_t*, char*, std::size_t);
    using TerminateProcessFn = int (*)(std::uint32_t, std::uint32_t, char*, std::size_t);

    HMODULE module_handle{nullptr};
    CollectSnapshotFn collect_snapshot_fn{nullptr};
    CollectProcessesFn collect_processes_fn{nullptr};
    CollectPerCoreCpuFn collect_per_core_cpu_fn{nullptr};
    CollectGpuFn collect_gpu_fn{nullptr};
    CollectDiskFn collect_disk_fn{nullptr};
    CollectNetworkFn collect_network_fn{nullptr};
    CollectThermalFn collect_thermal_fn{nullptr};
    CollectProcessDetailsFn collect_process_details_fn{nullptr};
    TerminateProcessFn terminate_process_fn{nullptr};

    // Delta state for disk/network rate computation
    bool has_prev_disk{false};
    std::uint64_t prev_disk_read_bytes{0};
    std::uint64_t prev_disk_write_bytes{0};
    double prev_disk_timestamp{0.0};

    bool has_prev_network{false};
    std::uint64_t prev_net_recv_bytes{0};
    std::uint64_t prev_net_sent_bytes{0};
    double prev_network_timestamp{0.0};
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

        // Optional extended sensor functions — null is OK, graceful degradation
        impl_->collect_per_core_cpu_fn = reinterpret_cast<Impl::CollectPerCoreCpuFn>(
            GetProcAddress(module, "aura_collect_per_core_cpu")
        );
        impl_->collect_gpu_fn = reinterpret_cast<Impl::CollectGpuFn>(
            GetProcAddress(module, "aura_collect_gpu_utilization")
        );
        impl_->collect_disk_fn = reinterpret_cast<Impl::CollectDiskFn>(
            GetProcAddress(module, "aura_collect_disk_counters")
        );
        impl_->collect_network_fn = reinterpret_cast<Impl::CollectNetworkFn>(
            GetProcAddress(module, "aura_collect_network_counters")
        );
        impl_->collect_thermal_fn = reinterpret_cast<Impl::CollectThermalFn>(
            GetProcAddress(module, "aura_collect_thermal_readings")
        );
        impl_->collect_process_details_fn = reinterpret_cast<Impl::CollectProcessDetailsFn>(
            GetProcAddress(module, "aura_collect_process_details")
        );
        impl_->terminate_process_fn = reinterpret_cast<Impl::TerminateProcessFn>(
            GetProcAddress(module, "aura_terminate_process")
        );

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

std::optional<PerCoreCpuState> TelemetryBridge::collect_per_core_cpu(std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return std::nullopt;
    }
#ifdef _WIN32
    if (impl_->collect_per_core_cpu_fn == nullptr) {
        return std::nullopt;
    }
    constexpr std::uint32_t kMaxCores = 256;
    std::vector<double> percents(kMaxCores, 0.0);
    std::uint32_t core_count = 0;
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = impl_->collect_per_core_cpu_fn(
        percents.data(), kMaxCores, &core_count, error_buffer.data(), error_buffer.size()
    );
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        return std::nullopt;
    }
    PerCoreCpuState state;
    state.core_count = core_count;
    state.core_percents.resize(core_count);
    for (std::uint32_t i = 0; i < core_count; ++i) {
        state.core_percents[i] = clamp_percent(percents[i]);
    }
    return state;
#else
    return std::nullopt;
#endif
}

std::optional<GpuState> TelemetryBridge::collect_gpu(std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return std::nullopt;
    }
#ifdef _WIN32
    if (impl_->collect_gpu_fn == nullptr) {
        return std::nullopt;
    }
    aura_gpu_utilization raw{};
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = impl_->collect_gpu_fn(&raw, error_buffer.data(), error_buffer.size());
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        return std::nullopt;
    }
    GpuState state;
    state.available = true;
    state.gpu_percent = clamp_percent(raw.gpu_percent);
    state.vram_percent = clamp_percent(raw.vram_percent);
    state.vram_used_bytes = raw.vram_used_bytes;
    state.vram_total_bytes = raw.vram_total_bytes;
    return state;
#else
    return std::nullopt;
#endif
}

std::optional<DiskIoState> TelemetryBridge::collect_disk_io(std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return std::nullopt;
    }
#ifdef _WIN32
    if (impl_->collect_disk_fn == nullptr) {
        return std::nullopt;
    }
    aura_disk_counters raw{};
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = impl_->collect_disk_fn(&raw, error_buffer.data(), error_buffer.size());
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        return std::nullopt;
    }
    const auto now = std::chrono::steady_clock::now();
    const double now_sec = std::chrono::duration<double>(now.time_since_epoch()).count();

    DiskIoState state;
    if (impl_->has_prev_disk) {
        const double dt = now_sec - impl_->prev_disk_timestamp;
        if (dt > 0.0) {
            const double dr = static_cast<double>(raw.read_bytes - impl_->prev_disk_read_bytes);
            const double dw = static_cast<double>(raw.write_bytes - impl_->prev_disk_write_bytes);
            state.read_bytes_per_sec = std::max(0.0, dr / dt);
            state.write_bytes_per_sec = std::max(0.0, dw / dt);
        }
    }
    impl_->prev_disk_read_bytes = raw.read_bytes;
    impl_->prev_disk_write_bytes = raw.write_bytes;
    impl_->prev_disk_timestamp = now_sec;
    impl_->has_prev_disk = true;
    return state;
#else
    return std::nullopt;
#endif
}

std::optional<NetworkIoState> TelemetryBridge::collect_network_io(std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return std::nullopt;
    }
#ifdef _WIN32
    if (impl_->collect_network_fn == nullptr) {
        return std::nullopt;
    }
    aura_network_counters raw{};
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = impl_->collect_network_fn(&raw, error_buffer.data(), error_buffer.size());
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        return std::nullopt;
    }
    const auto now = std::chrono::steady_clock::now();
    const double now_sec = std::chrono::duration<double>(now.time_since_epoch()).count();

    NetworkIoState state;
    if (impl_->has_prev_network) {
        const double dt = now_sec - impl_->prev_network_timestamp;
        if (dt > 0.0) {
            const double dr = static_cast<double>(raw.bytes_recv - impl_->prev_net_recv_bytes);
            const double ds = static_cast<double>(raw.bytes_sent - impl_->prev_net_sent_bytes);
            state.recv_bytes_per_sec = std::max(0.0, dr / dt);
            state.sent_bytes_per_sec = std::max(0.0, ds / dt);
        }
    }
    impl_->prev_net_recv_bytes = raw.bytes_recv;
    impl_->prev_net_sent_bytes = raw.bytes_sent;
    impl_->prev_network_timestamp = now_sec;
    impl_->has_prev_network = true;
    return state;
#else
    return std::nullopt;
#endif
}

std::optional<ThermalState> TelemetryBridge::collect_thermal(std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return std::nullopt;
    }
#ifdef _WIN32
    if (impl_->collect_thermal_fn == nullptr) {
        return std::nullopt;
    }
    constexpr std::uint32_t kMaxReadings = 32;
    std::vector<aura_thermal_reading> raw_readings(kMaxReadings);
    std::uint32_t out_count = 0;
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = impl_->collect_thermal_fn(
        raw_readings.data(), kMaxReadings, &out_count, error_buffer.data(), error_buffer.size()
    );
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        return std::nullopt;
    }
    ThermalState state;
    state.available = true;
    const std::size_t count = std::min<std::size_t>(out_count, kMaxReadings);
    state.sensors.reserve(count);
    double hottest = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        ThermalSensorReading reading;
        const std::size_t label_len = c_string_length(raw_readings[i].label, sizeof(raw_readings[i].label));
        reading.label.assign(raw_readings[i].label, label_len);
        reading.current_celsius = raw_readings[i].current_celsius;
        reading.high_celsius = raw_readings[i].high_celsius;
        reading.critical_celsius = raw_readings[i].critical_celsius;
        reading.has_high = raw_readings[i].has_high != 0;
        reading.has_critical = raw_readings[i].has_critical != 0;
        if (reading.current_celsius > hottest) {
            hottest = reading.current_celsius;
        }
        state.sensors.push_back(std::move(reading));
    }
    state.hottest_celsius = hottest;
    return state;
#else
    return std::nullopt;
#endif
}

std::vector<ProcessSample> TelemetryBridge::collect_process_details(
    const std::size_t max_results,
    const std::uint8_t sort_column,
    const bool sort_descending,
    std::string& error
) {
    error.clear();
    std::vector<ProcessSample> output;
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return output;
    }
    if (max_results == 0U) {
        return output;
    }

#ifdef _WIN32
    if (impl_->collect_process_details_fn == nullptr) {
        return collect_top_processes(max_results, error);
    }
    const std::size_t bounded = std::min(max_results, static_cast<std::size_t>(256));
    aura_process_query_options opts{};
    opts.max_results = static_cast<std::uint32_t>(bounded);
    opts.sort_column = sort_column;
    opts.sort_descending = sort_descending ? 1 : 0;

    std::vector<aura_process_detail> raw(bounded);
    std::array<char, kErrorBufferSize> error_buffer{};
    std::uint32_t out_count = 0;
    const int status = impl_->collect_process_details_fn(
        &opts, raw.data(), static_cast<std::uint32_t>(bounded),
        &out_count, error_buffer.data(), error_buffer.size()
    );
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        if (error.empty()) {
            error = "Process detail collection failed.";
        }
        return output;
    }
    const std::size_t count = std::min<std::size_t>(out_count, bounded);
    output.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        ProcessSample s;
        s.pid = raw[i].pid;
        const std::size_t name_len = c_string_length(raw[i].name, sizeof(raw[i].name));
        s.name.assign(raw[i].name, name_len);
        if (s.name.empty()) {
            s.name = "pid-" + std::to_string(raw[i].pid);
        }
        s.cpu_percent = clamp_percent(raw[i].cpu_percent);
        s.memory_rss_bytes = raw[i].memory_rss_bytes;
        output.push_back(std::move(s));
    }
#else
    error = "Telemetry bridge is only supported on Windows.";
#endif
    return output;
}

bool TelemetryBridge::terminate_process(const std::uint32_t pid, std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Telemetry bridge is unavailable.";
        return false;
    }
#ifdef _WIN32
    if (impl_->terminate_process_fn == nullptr) {
        error = "Process termination is not supported by the loaded telemetry DLL.";
        return false;
    }
    std::array<char, kErrorBufferSize> error_buffer{};
    const int status = impl_->terminate_process_fn(pid, 1, error_buffer.data(), error_buffer.size());
    if (status != kStatusOk) {
        error.assign(error_buffer.data(), c_string_length(error_buffer.data(), error_buffer.size()));
        if (error.empty()) {
            error = "Failed to terminate process " + std::to_string(pid) + ".";
        }
        return false;
    }
    return true;
#else
    error = "Telemetry bridge is only supported on Windows.";
    return false;
#endif
}

std::string TelemetryBridge::loaded_path() const {
    return impl_ != nullptr ? impl_->loaded_path : std::string{};
}

std::string TelemetryBridge::load_error() const {
    return impl_ != nullptr ? impl_->load_error : std::string{};
}

}  // namespace aura::shell
