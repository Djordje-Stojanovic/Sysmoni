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

struct AuraStyleSequencer {
    aura::render_native::FrameDiscipline discipline{};
    double pulse_hz{0.5};
    double rise_half_life_seconds{0.12};
    double fall_half_life_seconds{0.22};
    double phase{0.0};
    double smoothed_cpu_percent{0.0};
    double smoothed_memory_percent{0.0};
    bool has_smoothed_samples{false};
    std::string last_error{};
};

namespace {

constexpr int kDefaultTargetFps = 60;
constexpr int kDefaultMaxCatchupFrames = 4;
constexpr double kDefaultPulseHz = 0.5;
constexpr double kDefaultRiseHalfLifeSeconds = 0.12;
constexpr double kDefaultFallHalfLifeSeconds = 0.22;
constexpr double kDefaultAccentIntensity = 0.15;
constexpr char kFallbackHexColor[] = "#000000";
constexpr char kFallbackProcessRow[] = " 0. unavailable           CPU   0.0%  RAM     0.0 MB";
constexpr char kFallbackInitialStatus[] = "Collecting telemetry...";
constexpr char kFallbackStreamStatus[] = "Streaming telemetry";
constexpr char kFallbackLastError[] = "";
constexpr char kInvalidStyleSequencerHandle[] = "invalid style sequencer handle";
constexpr double kLn2 = 0.69314718055994530942;

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
    tokens.severity_level = 0;
    tokens.motion_scale = 1.0;
    tokens.quality_hint = 0;
    tokens.timeline_anomaly_alpha = 0.05;
    return tokens;
}

int resolve_target_fps(int target_fps) {
    return target_fps > 0 ? target_fps : kDefaultTargetFps;
}

int resolve_max_catchup_frames(int max_catchup_frames) {
    return max_catchup_frames > 0 ? max_catchup_frames : kDefaultMaxCatchupFrames;
}

double resolve_positive_finite(double value, double fallback) {
    if (!std::isfinite(value) || value <= 0.0) {
        return fallback;
    }
    return value;
}

AuraStyleSequencerConfig sanitize_style_sequencer_config(AuraStyleSequencerConfig config) {
    AuraStyleSequencerConfig out{};
    out.target_fps = resolve_target_fps(config.target_fps);
    out.max_catchup_frames = resolve_max_catchup_frames(config.max_catchup_frames);
    out.pulse_hz = resolve_positive_finite(config.pulse_hz, kDefaultPulseHz);
    out.rise_half_life_seconds = resolve_positive_finite(
        config.rise_half_life_seconds,
        kDefaultRiseHalfLifeSeconds
    );
    out.fall_half_life_seconds = resolve_positive_finite(
        config.fall_half_life_seconds,
        kDefaultFallHalfLifeSeconds
    );
    return out;
}

double resolve_elapsed_seconds(const AuraStyleSequencer& sequencer, double elapsed_since_last_frame) {
    return sequencer.discipline.clamp_delta_seconds(elapsed_since_last_frame);
}

double smoothing_alpha(double elapsed_seconds, double half_life_seconds) {
    const double clamped_elapsed = aura::render_native::sanitize_non_negative(elapsed_seconds);
    if (clamped_elapsed <= 0.0) {
        return 0.0;
    }

    const double safe_half_life =
        resolve_positive_finite(half_life_seconds, kDefaultRiseHalfLifeSeconds);
    const double alpha = 1.0 - std::exp((-kLn2 * clamped_elapsed) / safe_half_life);
    return aura::render_native::clamp_unit(alpha);
}

double apply_asymmetric_smoothing(
    double current_value,
    double target_value,
    double elapsed_seconds,
    double rise_half_life_seconds,
    double fall_half_life_seconds
) {
    const double rise_half_life =
        resolve_positive_finite(rise_half_life_seconds, kDefaultRiseHalfLifeSeconds);
    const double fall_half_life =
        resolve_positive_finite(fall_half_life_seconds, kDefaultFallHalfLifeSeconds);
    const double half_life = target_value >= current_value ? rise_half_life : fall_half_life;
    const double alpha = smoothing_alpha(elapsed_seconds, half_life);
    const double smoothed = current_value + ((target_value - current_value) * alpha);
    return aura::render_native::sanitize_percent(smoothed);
}

void set_style_sequencer_error(AuraStyleSequencer* sequencer, std::string message) {
    if (sequencer == nullptr) {
        return;
    }
    sequencer->last_error = std::move(message);
}

void clear_style_sequencer_error(AuraStyleSequencer* sequencer) {
    if (sequencer == nullptr) {
        return;
    }
    sequencer->last_error.clear();
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
        input.severity_level,
        input.motion_scale,
        input.quality_hint,
        input.timeline_anomaly_alpha,
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

AuraStyleSequencer* aura_style_sequencer_create(AuraStyleSequencerConfig config) {
    try {
        const AuraStyleSequencerConfig sanitized = sanitize_style_sequencer_config(config);
        auto* sequencer = new AuraStyleSequencer{};
        sequencer->discipline = aura::render_native::FrameDiscipline{
            sanitized.target_fps,
            sanitized.max_catchup_frames,
        };
        sequencer->pulse_hz = sanitized.pulse_hz;
        sequencer->rise_half_life_seconds = sanitized.rise_half_life_seconds;
        sequencer->fall_half_life_seconds = sanitized.fall_half_life_seconds;
        sequencer->phase = 0.0;
        sequencer->smoothed_cpu_percent = 0.0;
        sequencer->smoothed_memory_percent = 0.0;
        sequencer->has_smoothed_samples = false;
        clear_style_sequencer_error(sequencer);
        clear_last_error();
        return sequencer;
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_style_sequencer_create", ex);
        return nullptr;
    } catch (...) {
        set_last_error_unknown("aura_style_sequencer_create");
        return nullptr;
    }
}

void aura_style_sequencer_destroy(AuraStyleSequencer* sequencer) {
    (void)call_void_with_error("aura_style_sequencer_destroy", [&]() {
        delete sequencer;
    });
}

void aura_style_sequencer_reset(AuraStyleSequencer* sequencer, double phase_seed) {
    if (sequencer == nullptr) {
        set_last_error("aura_style_sequencer_reset", kInvalidStyleSequencerHandle);
        return;
    }

    const bool ok = call_void_with_error("aura_style_sequencer_reset", [&]() {
        sequencer->phase = normalize_phase(phase_seed);
        sequencer->smoothed_cpu_percent = 0.0;
        sequencer->smoothed_memory_percent = 0.0;
        sequencer->has_smoothed_samples = false;
        clear_style_sequencer_error(sequencer);
    });
    if (!ok) {
        set_style_sequencer_error(sequencer, "reset failed");
    }
}

AuraRenderStyleTokens aura_style_sequencer_tick(
    AuraStyleSequencer* sequencer,
    AuraStyleSequencerInput input
) {
    if (sequencer == nullptr) {
        set_last_error("aura_style_sequencer_tick", kInvalidStyleSequencerHandle);
        return fallback_style_tokens(0.0);
    }

    try {
        const double cpu_percent = aura::render_native::sanitize_percent(input.cpu_percent);
        const double memory_percent = aura::render_native::sanitize_percent(input.memory_percent);
        const double elapsed_seconds = resolve_elapsed_seconds(*sequencer, input.elapsed_since_last_frame);

        if (!sequencer->has_smoothed_samples) {
            sequencer->smoothed_cpu_percent = cpu_percent;
            sequencer->smoothed_memory_percent = memory_percent;
            sequencer->has_smoothed_samples = true;
        } else {
            sequencer->smoothed_cpu_percent = apply_asymmetric_smoothing(
                sequencer->smoothed_cpu_percent,
                cpu_percent,
                elapsed_seconds,
                sequencer->rise_half_life_seconds,
                sequencer->fall_half_life_seconds
            );
            sequencer->smoothed_memory_percent = apply_asymmetric_smoothing(
                sequencer->smoothed_memory_percent,
                memory_percent,
                elapsed_seconds,
                sequencer->rise_half_life_seconds,
                sequencer->fall_half_life_seconds
            );
        }

        aura::render_native::QtRenderFrameInput frame_input{};
        frame_input.cpu_percent = sequencer->smoothed_cpu_percent;
        frame_input.memory_percent = sequencer->smoothed_memory_percent;
        frame_input.elapsed_since_last_frame = elapsed_seconds;
        frame_input.pulse_hz = sequencer->pulse_hz;
        frame_input.target_fps = sequencer->discipline.target_fps;
        frame_input.max_catchup_frames = sequencer->discipline.max_catchup_frames;

        const aura::render_native::QtRenderStyleTokens tokens =
            aura::render_native::compute_qt_style_tokens(sequencer->phase, frame_input);
        sequencer->phase = tokens.phase;
        clear_style_sequencer_error(sequencer);
        clear_last_error();
        return to_external(tokens);
    } catch (const std::exception& ex) {
        set_style_sequencer_error(sequencer, std::string(ex.what()));
        set_last_error_exception("aura_style_sequencer_tick", ex);
        return fallback_style_tokens(sequencer->phase);
    } catch (...) {
        set_style_sequencer_error(sequencer, "unknown exception");
        set_last_error_unknown("aura_style_sequencer_tick");
        return fallback_style_tokens(sequencer->phase);
    }
}

const char* aura_style_sequencer_last_error(const AuraStyleSequencer* sequencer) {
    if (sequencer == nullptr) {
        set_last_error("aura_style_sequencer_last_error", kInvalidStyleSequencerHandle);
        return kInvalidStyleSequencerHandle;
    }

    try {
        clear_last_error();
        return sequencer->last_error.c_str();
    } catch (const std::exception& ex) {
        set_last_error_exception("aura_style_sequencer_last_error", ex);
        return "unable to read style sequencer error";
    } catch (...) {
        set_last_error_unknown("aura_style_sequencer_last_error");
        return "unable to read style sequencer error";
    }
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
