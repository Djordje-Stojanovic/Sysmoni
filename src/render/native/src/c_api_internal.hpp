#pragma once

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

// ---------------------------------------------------------------------------
// Opaque struct definitions (visible to all C API translation units)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Thread-local error buffer (C++17 inline thread_local -- same TLS slot
// across all translation units that include this header)
// ---------------------------------------------------------------------------

inline thread_local char g_last_error[512] = "";

// ---------------------------------------------------------------------------
// Internal helpers -- aura::render::detail namespace
// All functions are inline so they can live in a header included by
// multiple .cpp files without ODR violations.
// ---------------------------------------------------------------------------

namespace aura::render::detail {

// ---- Constants ------------------------------------------------------------

inline constexpr int kDefaultTargetFps = 60;
inline constexpr int kDefaultMaxCatchupFrames = 4;
inline constexpr double kDefaultPulseHz = 0.5;
inline constexpr double kDefaultRiseHalfLifeSeconds = 0.12;
inline constexpr double kDefaultFallHalfLifeSeconds = 0.22;
inline constexpr double kDefaultAccentIntensity = 0.15;
inline constexpr char kFallbackHexColor[] = "#000000";
inline constexpr char kFallbackProcessRow[] = " 0. unavailable           CPU   0.0%  RAM     0.0 MB";
inline constexpr char kFallbackInitialStatus[] = "Collecting telemetry...";
inline constexpr char kFallbackStreamStatus[] = "Streaming telemetry";
inline constexpr char kFallbackLastError[] = "";
inline constexpr char kInvalidStyleSequencerHandle[] = "invalid style sequencer handle";
inline constexpr double kLn2 = 0.69314718055994530942;

// ---- String helpers -------------------------------------------------------

inline void write_c_chars(const char* value, char* out, size_t out_size) {
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

inline void write_c_string(const std::string& value, char* out, size_t out_size) {
    write_c_chars(value.c_str(), out, out_size);
}

// ---- Error management -----------------------------------------------------

inline void clear_last_error() {
    g_last_error[0] = '\0';
}

inline void set_last_error(const char* api_name, const char* message) {
    const char* safe_api = api_name == nullptr ? "aura_render" : api_name;
    const char* safe_message = message == nullptr ? "unknown error" : message;
    const int written =
        std::snprintf(g_last_error, sizeof(g_last_error), "%s: %s", safe_api, safe_message);
    if (written < 0) {
        write_c_chars("aura_render: unknown error", g_last_error, sizeof(g_last_error));
    }
}

inline void set_last_error(const char* api_name, const std::string& message) {
    set_last_error(api_name, message.c_str());
}

inline void set_last_error_exception(const char* api_name, const std::exception& ex) {
    set_last_error(api_name, ex.what());
}

inline void set_last_error_unknown(const char* api_name) {
    set_last_error(api_name, "unknown exception");
}

// ---- Call wrappers --------------------------------------------------------

template <typename T, typename Func>
inline T call_with_fallback(const char* api_name, T fallback, Func&& func) {
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
inline bool call_void_with_error(const char* api_name, Func&& func) {
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

// ---- Phase / normalization helpers ----------------------------------------

inline double normalize_phase(double phase) {
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

// ---- Fallback value generators --------------------------------------------

inline double fallback_next_delay_seconds() {
    return 1.0 / static_cast<double>(kDefaultTargetFps);
}

inline double fallback_accent_floor(double floor) {
    if (!std::isfinite(floor)) {
        return kDefaultAccentIntensity;
    }
    return aura::render_native::clamp_unit(floor);
}

inline AuraCockpitFrameState fallback_cockpit_frame(double previous_phase) {
    return AuraCockpitFrameState{
        normalize_phase(previous_phase),
        kDefaultAccentIntensity,
        fallback_next_delay_seconds(),
    };
}

inline AuraRenderStyleTokens fallback_style_tokens(double previous_phase) {
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

// ---- Config sanitization --------------------------------------------------

inline int resolve_target_fps(int target_fps) {
    return target_fps > 0 ? target_fps : kDefaultTargetFps;
}

inline int resolve_max_catchup_frames(int max_catchup_frames) {
    return max_catchup_frames > 0 ? max_catchup_frames : kDefaultMaxCatchupFrames;
}

inline double resolve_positive_finite(double value, double fallback) {
    if (!std::isfinite(value) || value <= 0.0) {
        return fallback;
    }
    return value;
}

inline AuraStyleSequencerConfig sanitize_style_sequencer_config(AuraStyleSequencerConfig config) {
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

// ---- Smoothing functions --------------------------------------------------

inline double resolve_elapsed_seconds(const AuraStyleSequencer& sequencer, double elapsed_since_last_frame) {
    return sequencer.discipline.clamp_delta_seconds(elapsed_since_last_frame);
}

inline double smoothing_alpha(double elapsed_seconds, double half_life_seconds) {
    const double clamped_elapsed = aura::render_native::sanitize_non_negative(elapsed_seconds);
    if (clamped_elapsed <= 0.0) {
        return 0.0;
    }

    const double safe_half_life =
        resolve_positive_finite(half_life_seconds, kDefaultRiseHalfLifeSeconds);
    const double alpha = 1.0 - std::exp((-kLn2 * clamped_elapsed) / safe_half_life);
    return aura::render_native::clamp_unit(alpha);
}

inline double apply_asymmetric_smoothing(
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

// ---- Style sequencer error helpers ----------------------------------------

inline void set_style_sequencer_error(AuraStyleSequencer* sequencer, std::string message) {
    if (sequencer == nullptr) {
        return;
    }
    sequencer->last_error = std::move(message);
}

inline void clear_style_sequencer_error(AuraStyleSequencer* sequencer) {
    if (sequencer == nullptr) {
        return;
    }
    sequencer->last_error.clear();
}

// ---- Formatting fallback helpers ------------------------------------------

inline AuraSnapshotLines fallback_snapshot_lines() {
    AuraSnapshotLines out{};
    write_c_chars("CPU 0.0%", out.cpu, sizeof(out.cpu));
    write_c_chars("Memory 0.0%", out.memory, sizeof(out.memory));
    write_c_chars("Updated 00:00:00 UTC", out.timestamp, sizeof(out.timestamp));
    return out;
}

inline std::optional<std::string> optional_string_from_nullable(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

// ---- Bridge conversion helpers --------------------------------------------

inline aura::render_native::FrameDiscipline to_internal(AuraFrameDiscipline discipline) {
    return aura::render_native::FrameDiscipline{
        discipline.target_fps,
        discipline.max_catchup_frames,
    };
}

inline aura::render_native::QtRenderCallbacks to_internal(AuraQtRenderCallbacks callbacks) {
    return aura::render_native::QtRenderCallbacks{
        callbacks.begin_frame,
        callbacks.set_accent_rgba,
        callbacks.set_panel_frost,
        callbacks.set_ring_style,
        callbacks.set_timeline_emphasis,
        callbacks.commit_frame,
    };
}

inline aura::render_native::QtRenderFrameInput to_internal(AuraQtRenderFrameInput input) {
    return aura::render_native::QtRenderFrameInput{
        input.cpu_percent,
        input.memory_percent,
        input.elapsed_since_last_frame,
        input.pulse_hz,
        input.target_fps,
        input.max_catchup_frames,
    };
}

inline aura::render_native::QtRenderFrameInput to_internal(AuraRenderStyleTokensInput input) {
    return aura::render_native::QtRenderFrameInput{
        input.cpu_percent,
        input.memory_percent,
        input.elapsed_since_last_frame,
        input.pulse_hz,
        input.target_fps,
        input.max_catchup_frames,
    };
}

inline AuraRenderStyleTokens to_external(const aura::render_native::QtRenderStyleTokens& input) {
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

}  // namespace aura::render::detail
