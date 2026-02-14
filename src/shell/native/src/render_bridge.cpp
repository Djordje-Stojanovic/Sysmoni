#include "aura_shell/render_bridge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "aura_render.h"
#endif

namespace aura::shell {

namespace {

constexpr std::size_t kOutBufferSize = 256;

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

std::vector<std::filesystem::path> render_library_candidates() {
    const std::filesystem::path dll_name("aura_render_native.dll");
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(dll_name);

    const auto exe_dir = executable_directory();
    if (!exe_dir.empty()) {
        candidates.push_back((exe_dir / dll_name).lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / "render" / "native" / "build" /
                              "Release" / dll_name)
                                 .lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / ".." / "build" / "render-native" /
                              "Release" / dll_name)
                                 .lexically_normal());
        candidates.push_back((exe_dir / ".." / ".." / ".." / ".." / ".." / "build" / "render-native-tests" /
                              "Release" / dll_name)
                                 .lexically_normal());
    }
    return candidates;
}
#endif

}  // namespace

struct RenderBridge::Impl {
#ifdef _WIN32
    using SanitizePercentFn = double (*)(double);
    using ComposeFrameFn =
        AuraCockpitFrameState (*)(double, double, double, double, AuraFrameDiscipline, double);
    using FormatSnapshotLinesFn = AuraSnapshotLines (*)(double, double, double);
    using FormatProcessRowFn = void (*)(int, const char*, double, double, int, char*, std::size_t);
    using FormatStreamStatusFn =
        void (*)(const char*, int, int, const char*, char*, std::size_t);

    HMODULE module_handle{nullptr};
    SanitizePercentFn sanitize_percent_fn{nullptr};
    ComposeFrameFn compose_frame_fn{nullptr};
    FormatSnapshotLinesFn format_snapshot_lines_fn{nullptr};
    FormatProcessRowFn format_process_row_fn{nullptr};
    FormatStreamStatusFn format_stream_status_fn{nullptr};
#endif
    bool loaded{false};
    std::string loaded_path;
    std::string load_error;
};

RenderBridge::RenderBridge() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    DWORD last_error_code = 0;
    for (const auto& candidate : render_library_candidates()) {
        const std::wstring path = candidate.wstring();
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr) {
            last_error_code = GetLastError();
            continue;
        }

        auto* sanitize_percent = reinterpret_cast<Impl::SanitizePercentFn>(
            GetProcAddress(module, "aura_sanitize_percent")
        );
        auto* compose_frame = reinterpret_cast<Impl::ComposeFrameFn>(
            GetProcAddress(module, "aura_compose_cockpit_frame")
        );
        auto* format_snapshot_lines = reinterpret_cast<Impl::FormatSnapshotLinesFn>(
            GetProcAddress(module, "aura_format_snapshot_lines")
        );
        auto* format_process_row = reinterpret_cast<Impl::FormatProcessRowFn>(
            GetProcAddress(module, "aura_format_process_row")
        );
        auto* format_stream_status = reinterpret_cast<Impl::FormatStreamStatusFn>(
            GetProcAddress(module, "aura_format_stream_status")
        );

        if (sanitize_percent == nullptr || compose_frame == nullptr || format_snapshot_lines == nullptr ||
            format_process_row == nullptr || format_stream_status == nullptr) {
            last_error_code = GetLastError();
            FreeLibrary(module);
            continue;
        }

        impl_->module_handle = module;
        impl_->sanitize_percent_fn = sanitize_percent;
        impl_->compose_frame_fn = compose_frame;
        impl_->format_snapshot_lines_fn = format_snapshot_lines;
        impl_->format_process_row_fn = format_process_row;
        impl_->format_stream_status_fn = format_stream_status;
        impl_->loaded = true;
        impl_->loaded_path = narrow_from_wide(path);
        impl_->load_error.clear();
        return;
    }

    impl_->load_error = "Unable to load aura_render_native.dll";
    const std::string suffix = format_windows_error(last_error_code);
    if (!suffix.empty()) {
        impl_->load_error += ": " + suffix;
    }
#else
    impl_->load_error = "Render bridge is only supported on Windows.";
#endif
}

RenderBridge::~RenderBridge() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->module_handle != nullptr) {
        FreeLibrary(impl_->module_handle);
        impl_->module_handle = nullptr;
    }
#endif
}

bool RenderBridge::available() const {
    return impl_ != nullptr && impl_->loaded;
}

double RenderBridge::sanitize_percent(const double value) const {
#ifdef _WIN32
    if (available() && impl_->sanitize_percent_fn != nullptr) {
        return impl_->sanitize_percent_fn(value);
    }
#endif
    return clamp_percent(value);
}

std::optional<FrameState> RenderBridge::compose_frame(
    const double previous_phase,
    const double elapsed_since_last_frame,
    const double cpu_percent,
    const double memory_percent,
    std::string& error
) const {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Render bridge unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    AuraFrameDiscipline discipline{};
    discipline.target_fps = 60;
    discipline.max_catchup_frames = 2;
    const AuraCockpitFrameState state = impl_->compose_frame_fn(
        previous_phase,
        elapsed_since_last_frame,
        cpu_percent,
        memory_percent,
        discipline,
        0.35
    );

    if (!std::isfinite(state.phase) || !std::isfinite(state.accent_intensity) ||
        !std::isfinite(state.next_delay_seconds)) {
        error = "Render compose returned non-finite values.";
        return std::nullopt;
    }

    FrameState out;
    out.phase = state.phase;
    out.accent_intensity = std::clamp(state.accent_intensity, 0.0, 1.0);
    out.next_delay_seconds = std::max(0.0, state.next_delay_seconds);
    return out;
#else
    error = "Render bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::optional<SnapshotLines> RenderBridge::format_snapshot_lines(
    const double timestamp,
    const double cpu_percent,
    const double memory_percent,
    std::string& error
) const {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Render bridge unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    const AuraSnapshotLines lines = impl_->format_snapshot_lines_fn(timestamp, cpu_percent, memory_percent);
    SnapshotLines out;
    out.cpu.assign(lines.cpu, c_string_length(lines.cpu, sizeof(lines.cpu)));
    out.memory.assign(lines.memory, c_string_length(lines.memory, sizeof(lines.memory)));
    out.timestamp.assign(lines.timestamp, c_string_length(lines.timestamp, sizeof(lines.timestamp)));
    return out;
#else
    error = "Render bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::optional<std::string> RenderBridge::format_process_row(
    const int rank,
    const std::string& name,
    const double cpu_percent,
    const double memory_rss_bytes,
    const int max_chars,
    std::string& error
) const {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Render bridge unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    std::array<char, kOutBufferSize> output{};
    impl_->format_process_row_fn(
        rank,
        name.c_str(),
        cpu_percent,
        memory_rss_bytes,
        max_chars,
        output.data(),
        output.size()
    );
    return std::string(output.data(), c_string_length(output.data(), output.size()));
#else
    error = "Render bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::optional<std::string> RenderBridge::format_stream_status(
    const std::optional<std::string>& db_path,
    const std::optional<int>& sample_count,
    const std::optional<std::string>& stream_error,
    std::string& error
) const {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Render bridge unavailable.";
        return std::nullopt;
    }

#ifdef _WIN32
    std::array<char, kOutBufferSize> output{};
    impl_->format_stream_status_fn(
        db_path.has_value() ? db_path->c_str() : nullptr,
        sample_count.has_value() ? 1 : 0,
        sample_count.value_or(0),
        stream_error.has_value() ? stream_error->c_str() : nullptr,
        output.data(),
        output.size()
    );
    return std::string(output.data(), c_string_length(output.data(), output.size()));
#else
    error = "Render bridge is only supported on Windows.";
    return std::nullopt;
#endif
}

std::string RenderBridge::loaded_path() const {
    return impl_ != nullptr ? impl_->loaded_path : std::string{};
}

std::string RenderBridge::load_error() const {
    return impl_ != nullptr ? impl_->load_error : std::string{};
}

}  // namespace aura::shell

