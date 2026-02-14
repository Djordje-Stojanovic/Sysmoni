#include "aura_render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <utility>

#include "render_native/formatting.hpp"
#include "render_native/math.hpp"
#include "render_native/qt_hooks.hpp"
#include "render_native/status.hpp"
#include "render_native/theme.hpp"
#include "render_native/widgets.hpp"

struct AuraQtRenderHooks {
    explicit AuraQtRenderHooks(aura::render_native::QtRenderCallbacks callbacks, void* user_data)
        : hooks(std::move(callbacks), user_data) {}

    aura::render_native::QtRenderHooks hooks;
};

namespace {

constexpr int kDefaultTargetFps = 60;
constexpr double kDefaultAccentIntensity = 0.15;
constexpr char kFallbackHexColor[] = "#000000";
constexpr char kFallbackProcessRow[] = " 0. unavailable           CPU   0.0%  RAM     0.0 MB";
constexpr char kFallbackInitialStatus[] = "Collecting telemetry...";
constexpr char kFallbackStreamStatus[] = "Streaming telemetry";
constexpr char kFallbackLastError[] = "";

thread_local char g_last_error[512] = "";

void write_c_chars(const char* value, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return;
    }
    if (value == nullptr) {
        out[0] = '\0';
        return;
    }
    const size_t value_size = std::strlen(value);
    const size_t copy_size = std::min(value_size, out_size - 1);
    std::memcpy(out, value, copy_size);
    out[copy_size] = '\0';
}

void write_c_string(const std::string& value, char* out, size_t out_size) {
    write_c_chars(value.c_str(), out, out_size);
}

void clear_last_error() {
    g_last_error[0] = '\0';
}

void set_last_error(const char* api_name, const char* message) {
    const char* safe_api = api_name == nullptr ? "aura_render" : api_name;
    const char* safe_message = message == nullptr ? "unknown error" : message;
    const int written =
        std::snprintf(g_last_error, sizeof(g_last_error), "%s: %s", safe_api, safe_message);
    if (written < 0) {
        write_c_chars("aura_render: unknown error", g_last_error, sizeof(g_last_error));
    }
}

void set_last_error(const char* api_name, const std::string& message) {
    set_last_error(api_name, message.c_str());
}

void set_last_error_exception(const char* api_name, const std::exception& ex) {
    set_last_error(api_name, ex.what());
}

void set_last_error_unknown(const char* api_name) {
    set_last_error(api_name, "unknown exception");
}

template <typename T, typename Func>
T call_with_fallback(const char* api_name, T fallback, Func&& func) {
    try {
        T value = func();
        clear_last_error();
        return value;
    } catch (const std::exception& ex) {
        set_last_error_exception(api_name, ex);
        return fallback;
    } catch (...) {
        set_last_error_unknown(api_name);
        return fallback;
    }
}

template <typename Func>
bool call_void_with_error(const char* api_name, Func&& func) {
    try {
        func();
        clear_last_error();
        return true;
    } catch (const std::exception& ex) {
        set_last_error_exception(api_name, ex);
        return false;
    } catch (...) {
        set_last_error_unknown(api_name);
        return false;
    }
}

double normalize_phase(double phase) {
    if (!std::isfinite(phase)) {
        return 0.0;
    }
    double normalized = std::fmod(phase, 1.0);
    if (!std::isfinite(normalized)) {
        return 0.0;
    }
    if (normalized < 0.0) {
        normalized += 1.0;
    }
    return normalized;
}

double fallback_next_delay_seconds() {
    return 1.0 / static_cast<double>(kDefaultTargetFps);
}

double fallback_accent_floor(double floor) {
    if (!std::isfinite(floor)) {
        return kDefaultAccentIntensity;
    }
    return aura::render_native::clamp_unit(floor);
}

AuraCockpitFrameState fallback_cockpit_frame(double previous_phase) {
    return AuraCockpitFrameState{
        normalize_phase(previous_phase),
        kDefaultAccentIntensity,
        fallback_next_delay_seconds(),
    };
}

AuraRenderStyleTokens fallback_style_tokens(double previous_phase) {
    const double accent = kDefaultAccentIntensity;
    AuraRenderStyleTokens tokens{};
    tokens.phase = normalize_phase(previous_phase);
    tokens.next_delay_seconds = fallback_next_delay_seconds();
    tokens.accent_intensity = accent;
    tokens.accent_red = aura::render_native::clamp_unit(0.12 + (accent * 0.65));
    tokens.accent_green = aura::render_native::clamp_unit(0.30 + (accent * 0.50));
    tokens.accent_blue = aura::render_native::clamp_unit(0.48 + (accent * 0.42));
    tokens.accent_alpha = aura::render_native::clamp_unit(0.62 + (accent * 0.33));
    tokens.frost_intensity = aura::render_native::clamp_unit(0.05 + (accent * 0.30));
    tokens.tint_strength = aura::render_native::clamp_unit(0.10 + (accent * 0.50));
    tokens.ring_line_width = 1.0 + (accent * 6.0);
    tokens.ring_glow_strength = aura::render_native::clamp_unit(0.20 + (accent * 0.75));
    tokens.cpu_alpha = 0.20;
    tokens.memory_alpha = 0.20;
    return tokens;
}

AuraSnapshotLines fallback_snapshot_lines() {
    AuraSnapshotLines out{};
    write_c_chars("CPU 0.0%", out.cpu, sizeof(out.cpu));
    write_c_chars("Memory 0.0%", out.memory, sizeof(out.memory));
    write_c_chars("Updated 00:00:00 UTC", out.timestamp, sizeof(out.timestamp));
    return out;
}

std::optional<std::string> optional_string_from_nullable(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

aura::render_native::FrameDiscipline to_internal(AuraFrameDiscipline discipline) {
    return aura::render_native::FrameDiscipline{
        discipline.target_fps,
        discipline.max_catchup_frames,
    };
}

aura::render_native::QtRenderCallbacks to_internal(AuraQtRenderCallbacks callbacks) {
    return aura::render_native::QtRenderCallbacks{
        callbacks.begin_frame,
        callbacks.set_accent_rgba,
        callbacks.set_panel_frost,
        callbacks.set_ring_style,
        callbacks.set_timeline_emphasis,
        callbacks.commit_frame,
    };
}

aura::render_native::QtRenderFrameInput to_internal(AuraQtRenderFrameInput input) {
    return aura::render_native::QtRenderFrameInput{
        input.cpu_percent,
        input.memory_percent,
        input.elapsed_since_last_frame,
        input.pulse_hz,
        input.target_fps,
        input.max_catchup_frames,
    };
}

aura::render_native::QtRenderFrameInput to_internal(AuraRenderStyleTokensInput input) {
    return aura::render_native::QtRenderFrameInput{
        input.cpu_percent,
        input.memory_percent,
        input.elapsed_since_last_frame,
        input.pulse_hz,
        input.target_fps,
        input.max_catchup_frames,
    };
}

AuraRenderStyleTokens to_external(const aura::render_native::QtRenderStyleTokens& input) {
    return AuraRenderStyleTokens{
        input.phase,
        input.next_delay_seconds,
        input.accent_intensity,
        input.accent_red,
        input.accent_green,
        input.accent_blue,
        input.accent_alpha,
        input.frost_intensity,
        input.tint_strength,
        input.ring_line_width,
        input.ring_glow_strength,
        input.cpu_alpha,
        input.memory_alpha,
    };
}

}  // namespace

extern "C" {

const char* aura_last_error(void) {
    return g_last_error[0] == '\0' ? kFallbackLastError : g_last_error;
}

void aura_clear_error(void) {
    clear_last_error();
}

double aura_sanitize_percent(double value) {
    return call_with_fallback<double>("aura_sanitize_percent", 0.0, [&]() {
        return aura::render_native::sanitize_percent(value);
    });
}

double aura_sanitize_non_negative(double value) {
    return call_with_fallback<double>("aura_sanitize_non_negative", 0.0, [&]() {
        return aura::render_native::sanitize_non_negative(value);
    });
}

int aura_quantize_accent_intensity(double accent_intensity) {
    return call_with_fallback<int>("aura_quantize_accent_intensity", 0, [&]() {
        return aura::render_native::quantize_accent_intensity(accent_intensity);
    });
}

int aura_widget_backend_available(void) {
    return call_with_fallback<int>("aura_widget_backend_available", 0, [&]() {
        return aura::render_native::widget_backend_available() ? 1 : 0;
    });
}

const char* aura_widget_backend_name(void) {
    static std::string backend = "qt_widgets_rhi_stub";
    try {
        backend = aura::render_native::widget_backend_name();
        clear_last_error();
        return backend.c_str();
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_widget_backend_name", ex);
    } catch (...) {
        set_last_error_unknown("aura_widget_backend_name");
    }
    return "qt_widgets_rhi_stub";
}

double aura_advance_phase(
    double phase,
    double delta_seconds,
    double cycles_per_second,
    AuraFrameDiscipline discipline
) {
    return call_with_fallback<double>("aura_advance_phase", normalize_phase(phase), [&]() {
        return aura::render_native::advance_phase(
            phase,
            delta_seconds,
            cycles_per_second,
            to_internal(discipline)
        );
    });
}

double aura_compute_accent_intensity(
    double cpu_percent,
    double memory_percent,
    double phase,
    double floor,
    double ceiling,
    double pulse_strength
) {
    return call_with_fallback<double>(
        "aura_compute_accent_intensity",
        fallback_accent_floor(floor),
        [&]() {
            return aura::render_native::compute_accent_intensity(
                cpu_percent,
                memory_percent,
                phase,
                floor,
                ceiling,
                pulse_strength
            );
        }
    );
}

AuraCockpitFrameState aura_compose_cockpit_frame(
    double previous_phase,
    double elapsed_since_last_frame,
    double cpu_percent,
    double memory_percent,
    AuraFrameDiscipline discipline,
    double pulse_hz
) {
    return call_with_fallback<AuraCockpitFrameState>(
        "aura_compose_cockpit_frame",
        fallback_cockpit_frame(previous_phase),
        [&]() {
            const aura::render_native::CockpitFrameState state =
                aura::render_native::compose_cockpit_frame(
                    previous_phase,
                    elapsed_since_last_frame,
                    cpu_percent,
                    memory_percent,
                    to_internal(discipline),
                    pulse_hz
                );
            return AuraCockpitFrameState{
                state.phase,
                state.accent_intensity,
                state.next_delay_seconds,
            };
        }
    );
}

AuraRenderStyleTokens aura_compute_style_tokens(AuraRenderStyleTokensInput input) {
    return call_with_fallback<AuraRenderStyleTokens>(
        "aura_compute_style_tokens",
        fallback_style_tokens(input.previous_phase),
        [&]() {
            const aura::render_native::QtRenderStyleTokens tokens =
                aura::render_native::compute_qt_style_tokens(input.previous_phase, to_internal(input));
            return to_external(tokens);
        }
    );
}

void aura_blend_hex_color(
    const char* start,
    const char* end,
    double ratio,
    char* out_hex,
    size_t out_hex_size
) {
    const bool ok = call_void_with_error("aura_blend_hex_color", [&]() {
        const std::string value = aura::render_native::blend_hex_color(
            std::string(start == nullptr ? "" : start),
            std::string(end == nullptr ? "" : end),
            ratio
        );
        write_c_string(value, out_hex, out_hex_size);
    });
    if (!ok) {
        write_c_chars(kFallbackHexColor, out_hex, out_hex_size);
    }
}

AuraSnapshotLines aura_format_snapshot_lines(
    double timestamp,
    double cpu_percent,
    double memory_percent
) {
    return call_with_fallback<AuraSnapshotLines>(
        "aura_format_snapshot_lines",
        fallback_snapshot_lines(),
        [&]() {
            const aura::render_native::SnapshotLines lines =
                aura::render_native::format_snapshot_lines(timestamp, cpu_percent, memory_percent);
            AuraSnapshotLines out{};
            write_c_string(lines.cpu, out.cpu, sizeof(out.cpu));
            write_c_string(lines.memory, out.memory, sizeof(out.memory));
            write_c_string(lines.timestamp, out.timestamp, sizeof(out.timestamp));
            return out;
        }
    );
}

void aura_format_process_row(
    int rank,
    const char* name,
    double cpu_percent,
    double memory_rss_bytes,
    int max_chars,
    char* out_row,
    size_t out_row_size
) {
    const bool ok = call_void_with_error("aura_format_process_row", [&]() {
        const std::string value = aura::render_native::format_process_row(
            rank,
            std::string(name == nullptr ? "" : name),
            cpu_percent,
            memory_rss_bytes,
            max_chars
        );
        write_c_string(value, out_row, out_row_size);
    });
    if (!ok) {
        write_c_chars(kFallbackProcessRow, out_row, out_row_size);
    }
}

void aura_format_initial_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
) {
    const bool ok = call_void_with_error("aura_format_initial_status", [&]() {
        const std::optional<int> sample_opt =
            sample_count_is_set != 0 ? std::optional<int>(sample_count) : std::nullopt;
        const std::string value = aura::render_native::format_initial_status(
            optional_string_from_nullable(db_path),
            sample_opt,
            optional_string_from_nullable(error)
        );
        write_c_string(value, out_status, out_status_size);
    });
    if (!ok) {
        write_c_chars(kFallbackInitialStatus, out_status, out_status_size);
    }
}

void aura_format_stream_status(
    const char* db_path,
    int sample_count_is_set,
    int sample_count,
    const char* error,
    char* out_status,
    size_t out_status_size
) {
    const bool ok = call_void_with_error("aura_format_stream_status", [&]() {
        const std::optional<int> sample_opt =
            sample_count_is_set != 0 ? std::optional<int>(sample_count) : std::nullopt;
        const std::string value = aura::render_native::format_stream_status(
            optional_string_from_nullable(db_path),
            sample_opt,
            optional_string_from_nullable(error)
        );
        write_c_string(value, out_status, out_status_size);
    });
    if (!ok) {
        write_c_chars(kFallbackStreamStatus, out_status, out_status_size);
    }
}

void aura_format_disk_rate(
    double bytes_per_second,
    char* out_rate,
    size_t out_rate_size
) {
    const bool ok = call_void_with_error("aura_format_disk_rate", [&]() {
        const std::string value = aura::render_native::format_disk_rate(bytes_per_second);
        write_c_string(value, out_rate, out_rate_size);
    });
    if (!ok) {
        write_c_chars("Disk 0.0 KB/s", out_rate, out_rate_size);
    }
}

void aura_format_network_rate(
    double bytes_per_second,
    char* out_rate,
    size_t out_rate_size
) {
    const bool ok = call_void_with_error("aura_format_network_rate", [&]() {
        const std::string value = aura::render_native::format_network_rate(bytes_per_second);
        write_c_string(value, out_rate, out_rate_size);
    });
    if (!ok) {
        write_c_chars("Net 0.0 KB/s", out_rate, out_rate_size);
    }
}

AuraQtRenderBackendCaps aura_qt_hooks_backend_caps(void) {
    const AuraQtRenderBackendCaps fallback_caps{0, 1, kDefaultTargetFps};
    return call_with_fallback<AuraQtRenderBackendCaps>(
        "aura_qt_hooks_backend_caps",
        fallback_caps,
        [&]() {
            const aura::render_native::QtRenderBackendCaps caps = aura::render_native::qt_backend_caps();
            return AuraQtRenderBackendCaps{
                caps.available ? 1 : 0,
                caps.supports_callbacks ? 1 : 0,
                caps.preferred_fps,
            };
        }
    );
}

AuraQtRenderHooks* aura_qt_hooks_create(const AuraQtRenderCallbacks* callbacks, void* user_data) {
    if (callbacks == nullptr) {
        set_last_error("aura_qt_hooks_create", "callbacks cannot be null");
        return nullptr;
    }

    aura::render_native::QtRenderCallbacks internal_callbacks = to_internal(*callbacks);
    if (!aura::render_native::qt_callbacks_complete(internal_callbacks)) {
        set_last_error("aura_qt_hooks_create", "callbacks are incomplete");
        return nullptr;
    }

    try {
        AuraQtRenderHooks* hooks = new AuraQtRenderHooks(std::move(internal_callbacks), user_data);
        clear_last_error();
        return hooks;
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_qt_hooks_create", ex);
        return nullptr;
    } catch (...) {
        set_last_error_unknown("aura_qt_hooks_create");
        return nullptr;
    }
}

void aura_qt_hooks_destroy(AuraQtRenderHooks* hooks) {
    (void)call_void_with_error("aura_qt_hooks_destroy", [&]() {
        delete hooks;
    });
}

int aura_qt_hooks_render_frame(AuraQtRenderHooks* hooks, AuraQtRenderFrameInput input) {
    if (hooks == nullptr) {
        set_last_error("aura_qt_hooks_render_frame", "invalid render hooks handle");
        return 0;
    }
    try {
        if (hooks->hooks.render_frame(to_internal(input))) {
            clear_last_error();
            return 1;
        }
        const std::string hook_error = hooks->hooks.last_error();
        if (hook_error.empty()) {
            set_last_error("aura_qt_hooks_render_frame", "render frame failed");
        } else {
            set_last_error("aura_qt_hooks_render_frame", hook_error);
        }
        return 0;
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_qt_hooks_render_frame", ex);
        return 0;
    } catch (...) {
        set_last_error_unknown("aura_qt_hooks_render_frame");
        return 0;
    }
}

const char* aura_qt_hooks_last_error(const AuraQtRenderHooks* hooks) {
    if (hooks == nullptr) {
        set_last_error("aura_qt_hooks_last_error", "invalid render hooks handle");
        return "invalid render hooks handle";
    }
    try {
        clear_last_error();
        return hooks->hooks.last_error().c_str();
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_qt_hooks_last_error", ex);
        return "callback invocation failed: unable to read hook error";
    } catch (...) {
        set_last_error_unknown("aura_qt_hooks_last_error");
        return "callback invocation failed: unable to read hook error";
    }
}

}  // extern "C"
