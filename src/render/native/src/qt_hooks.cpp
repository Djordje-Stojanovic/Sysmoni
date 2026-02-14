#include "render_native/qt_hooks.hpp"

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

QtRenderHooks::QtRenderHooks(QtRenderCallbacks callbacks, void* user_data)
    : callbacks_(callbacks), user_data_(user_data) {}

bool QtRenderHooks::render_frame(const QtRenderFrameInput& input) {
    if (!qt_callbacks_complete(callbacks_)) {
        set_error("render callbacks are incomplete.");
        return false;
    }

    try {
        const FrameDiscipline discipline = resolve_discipline(input);
        const double pulse_hz = resolve_pulse_hz(input.pulse_hz);
        const CockpitFrameState frame = compose_cockpit_frame(
            phase_,
            resolve_elapsed(input.elapsed_since_last_frame),
            sanitize_percent(input.cpu_percent),
            sanitize_percent(input.memory_percent),
            discipline,
            pulse_hz
        );

        phase_ = frame.phase;
        const double accent = clamp_unit(frame.accent_intensity);
        const double accent_red = clamp_unit(0.12 + (accent * 0.65));
        const double accent_green = clamp_unit(0.30 + (accent * 0.50));
        const double accent_blue = clamp_unit(0.48 + (accent * 0.42));
        const double accent_alpha = clamp_unit(0.62 + (accent * 0.33));

        const double frost_intensity = clamp_unit(0.05 + (accent * 0.30));
        const double tint_strength = clamp_unit(0.10 + (accent * 0.50));
        const double ring_line_width = 1.0 + (accent * 6.0);
        const double ring_glow_strength = clamp_unit(0.20 + (accent * 0.75));
        const double cpu_alpha = compute_cpu_alpha(input.cpu_percent);
        const double memory_alpha = compute_memory_alpha(input.memory_percent);

        callbacks_.begin_frame(user_data_);
        callbacks_.set_accent_rgba(
            user_data_,
            accent_red,
            accent_green,
            accent_blue,
            accent_alpha
        );
        callbacks_.set_panel_frost(user_data_, frost_intensity, tint_strength);
        callbacks_.set_ring_style(user_data_, ring_line_width, ring_glow_strength);
        callbacks_.set_timeline_emphasis(user_data_, cpu_alpha, memory_alpha);
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
