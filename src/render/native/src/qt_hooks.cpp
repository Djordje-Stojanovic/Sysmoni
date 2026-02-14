#include "render_native/qt_hooks.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <utility>

#include "render_native/math.hpp"
#include "render_native/widgets.hpp"

namespace aura::render_native {

namespace {

constexpr int kDefaultTargetFps = 60;
constexpr int kDefaultMaxCatchupFrames = 4;
constexpr double kDefaultPulseHz = 0.5;

struct TrendState {
    bool initialized{false};
    double previous_cpu_percent{0.0};
    double previous_memory_percent{0.0};
};

thread_local TrendState g_trend_state{};

FrameDiscipline resolve_discipline(const QtRenderFrameInput& input) {
    return FrameDiscipline{
        input.target_fps > 0 ? input.target_fps : kDefaultTargetFps,
        input.max_catchup_frames > 0 ? input.max_catchup_frames : kDefaultMaxCatchupFrames,
    };
}

double resolve_pulse_hz(double value) {
    if (!std::isfinite(value) || value <= 0.0) {
        return kDefaultPulseHz;
    }
    return value;
}

double resolve_elapsed(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return value;
}

double compute_cpu_alpha(double cpu_percent) {
    const double ratio = sanitize_percent(cpu_percent) / 100.0;
    return clamp_unit(0.20 + (ratio * 0.75));
}

double compute_memory_alpha(double memory_percent) {
    const double ratio = sanitize_percent(memory_percent) / 100.0;
    return clamp_unit(0.20 + (ratio * 0.75));
}

double resolve_elapsed_positive(double value) {
    const double elapsed = resolve_elapsed(value);
    return elapsed > 1e-6 ? elapsed : (1.0 / static_cast<double>(kDefaultTargetFps));
}

double combined_load(double cpu_percent, double memory_percent) {
    return 0.5 * (sanitize_percent(cpu_percent) + sanitize_percent(memory_percent));
}

double compute_slope_per_second(
    const double cpu_percent,
    const double memory_percent,
    const double elapsed_since_last_frame
) {
    const double load = combined_load(cpu_percent, memory_percent);
    if (!g_trend_state.initialized) {
        g_trend_state.initialized = true;
        g_trend_state.previous_cpu_percent = sanitize_percent(cpu_percent);
        g_trend_state.previous_memory_percent = sanitize_percent(memory_percent);
        return 0.0;
    }

    const double previous_load =
        combined_load(g_trend_state.previous_cpu_percent, g_trend_state.previous_memory_percent);
    g_trend_state.previous_cpu_percent = sanitize_percent(cpu_percent);
    g_trend_state.previous_memory_percent = sanitize_percent(memory_percent);

    const double elapsed = resolve_elapsed_positive(elapsed_since_last_frame);
    return std::clamp((load - previous_load) / elapsed, -120.0, 120.0);
}

int compute_severity_level(const double cpu_percent, const double memory_percent, const double slope_per_second) {
    const double load = std::max(sanitize_percent(cpu_percent), sanitize_percent(memory_percent));
    if (load >= 92.0 || (load >= 85.0 && slope_per_second >= 8.0)) {
        return 3;
    }
    if (load >= 75.0 || (load >= 65.0 && slope_per_second >= 6.0)) {
        return 2;
    }
    if (load >= 50.0 || slope_per_second >= 4.0) {
        return 1;
    }
    return 0;
}

double compute_motion_scale(const int severity_level, const double slope_per_second) {
    constexpr double kSeverityScale[] = {1.00, 0.92, 0.80, 0.68};
    const int clamped_level = std::clamp(severity_level, 0, 3);
    const double slope_penalty = clamp_unit(std::max(0.0, slope_per_second) / 100.0) * 0.15;
    return std::clamp(kSeverityScale[clamped_level] - slope_penalty, 0.60, 1.00);
}

int compute_quality_hint(
    const int severity_level,
    const double cpu_percent,
    const double memory_percent
) {
    const double load = std::max(sanitize_percent(cpu_percent), sanitize_percent(memory_percent));
    return (severity_level >= 3 || (severity_level == 2 && load >= 82.0)) ? 1 : 0;
}

double compute_timeline_anomaly_alpha(
    const int severity_level,
    const double cpu_percent,
    const double memory_percent,
    const double slope_per_second
) {
    const double load = std::max(sanitize_percent(cpu_percent), sanitize_percent(memory_percent));
    const double load_score = clamp_unit((load - 55.0) / 45.0);
    const double slope_score = clamp_unit((slope_per_second - 2.0) / 20.0);
    const double severity_boost = static_cast<double>(std::clamp(severity_level, 0, 3)) * 0.06;
    return clamp_unit(0.05 + (load_score * 0.55) + (slope_score * 0.40) + severity_boost);
}

}  // namespace

bool qt_callbacks_complete(const QtRenderCallbacks& callbacks) {
    return callbacks.begin_frame != nullptr && callbacks.set_accent_rgba != nullptr &&
           callbacks.set_panel_frost != nullptr && callbacks.set_ring_style != nullptr &&
           callbacks.set_timeline_emphasis != nullptr && callbacks.commit_frame != nullptr;
}

QtRenderBackendCaps qt_backend_caps() {
    return QtRenderBackendCaps{
        widget_backend_available(),
        true,
        kDefaultTargetFps,
    };
}

QtRenderStyleTokens
compute_qt_style_tokens(double previous_phase, const QtRenderFrameInput& input) {
    const FrameDiscipline discipline = resolve_discipline(input);
    const double pulse_hz = resolve_pulse_hz(input.pulse_hz);
    const CockpitFrameState frame = compose_cockpit_frame(
        previous_phase,
        resolve_elapsed(input.elapsed_since_last_frame),
        sanitize_percent(input.cpu_percent),
        sanitize_percent(input.memory_percent),
        discipline,
        pulse_hz
    );

    const double accent = clamp_unit(frame.accent_intensity);

    QtRenderStyleTokens tokens{};
    tokens.phase = frame.phase;
    tokens.next_delay_seconds = sanitize_non_negative(frame.next_delay_seconds);
    tokens.accent_intensity = accent;
    tokens.accent_red = clamp_unit(0.12 + (accent * 0.65));
    tokens.accent_green = clamp_unit(0.30 + (accent * 0.50));
    tokens.accent_blue = clamp_unit(0.48 + (accent * 0.42));
    tokens.accent_alpha = clamp_unit(0.62 + (accent * 0.33));
    tokens.frost_intensity = clamp_unit(0.05 + (accent * 0.30));
    tokens.tint_strength = clamp_unit(0.10 + (accent * 0.50));
    tokens.ring_line_width = 1.0 + (accent * 6.0);
    tokens.ring_glow_strength = clamp_unit(0.20 + (accent * 0.75));
    tokens.cpu_alpha = compute_cpu_alpha(input.cpu_percent);
    tokens.memory_alpha = compute_memory_alpha(input.memory_percent);
    const double slope_per_second =
        compute_slope_per_second(input.cpu_percent, input.memory_percent, input.elapsed_since_last_frame);
    tokens.severity_level = compute_severity_level(input.cpu_percent, input.memory_percent, slope_per_second);
    tokens.motion_scale = compute_motion_scale(tokens.severity_level, slope_per_second);
    tokens.quality_hint = compute_quality_hint(tokens.severity_level, input.cpu_percent, input.memory_percent);
    tokens.timeline_anomaly_alpha = compute_timeline_anomaly_alpha(
        tokens.severity_level,
        input.cpu_percent,
        input.memory_percent,
        slope_per_second
    );
    return tokens;
}

QtRenderHooks::QtRenderHooks(QtRenderCallbacks callbacks, void* user_data)
    : callbacks_(callbacks), user_data_(user_data) {}

bool QtRenderHooks::render_frame(const QtRenderFrameInput& input) {
    if (!qt_callbacks_complete(callbacks_)) {
        set_error("render callbacks are incomplete.");
        return false;
    }

    try {
        const QtRenderStyleTokens tokens = compute_qt_style_tokens(phase_, input);
        phase_ = tokens.phase;

        callbacks_.begin_frame(user_data_);
        callbacks_.set_accent_rgba(
            user_data_,
            tokens.accent_red,
            tokens.accent_green,
            tokens.accent_blue,
            tokens.accent_alpha
        );
        callbacks_.set_panel_frost(user_data_, tokens.frost_intensity, tokens.tint_strength);
        callbacks_.set_ring_style(user_data_, tokens.ring_line_width, tokens.ring_glow_strength);
        callbacks_.set_timeline_emphasis(user_data_, tokens.cpu_alpha, tokens.memory_alpha);
        callbacks_.commit_frame(user_data_);

        set_error("");
        return true;
    } catch (const std::exception& ex) {
        set_error(std::string("callback invocation failed: ") + ex.what());
        return false;
    } catch (...) {
        set_error("callback invocation failed: unknown exception.");
        return false;
    }
}

const std::string& QtRenderHooks::last_error() const {
    return last_error_;
}

void QtRenderHooks::set_error(std::string message) {
    last_error_ = std::move(message);
}

}  // namespace aura::render_native
