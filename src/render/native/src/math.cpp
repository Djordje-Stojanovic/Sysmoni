#include "render_native/math.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace aura::render_native {

double FrameDiscipline::frame_interval_seconds() const {
    if (target_fps <= 0) {
        throw std::invalid_argument("target_fps must be greater than 0.");
    }
    return 1.0 / static_cast<double>(target_fps);
}

double FrameDiscipline::max_delta_seconds() const {
    if (max_catchup_frames <= 0) {
        throw std::invalid_argument("max_catchup_frames must be greater than 0.");
    }
    return frame_interval_seconds() * static_cast<double>(max_catchup_frames);
}

double FrameDiscipline::clamp_delta_seconds(double delta_seconds) const {
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) {
        return 0.0;
    }
    return std::min(delta_seconds, max_delta_seconds());
}

double FrameDiscipline::next_frame_delay_seconds(double elapsed_since_last_frame) const {
    const double elapsed = clamp_delta_seconds(elapsed_since_last_frame);
    return std::max(0.0, frame_interval_seconds() - elapsed);
}

double sanitize_percent(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 100.0);
}

double sanitize_non_negative(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::max(0.0, value);
}

double clamp_unit(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

double advance_phase(
    double phase,
    double delta_seconds,
    double cycles_per_second,
    const FrameDiscipline& discipline
) {
    if (cycles_per_second <= 0.0) {
        throw std::invalid_argument("cycles_per_second must be greater than 0.");
    }

    double normalized_phase = std::fmod(phase, 1.0);
    if (!std::isfinite(normalized_phase)) {
        normalized_phase = 0.0;
    }
    if (normalized_phase < 0.0) {
        normalized_phase += 1.0;
    }

    const double clamped_delta = discipline.clamp_delta_seconds(delta_seconds);
    double next_phase = std::fmod(normalized_phase + (clamped_delta * cycles_per_second), 1.0);
    if (next_phase < 0.0) {
        next_phase += 1.0;
    }
    return next_phase;
}

double compute_accent_intensity(
    double cpu_percent,
    double memory_percent,
    double phase,
    double floor,
    double ceiling,
    double pulse_strength
) {
    if (floor < 0.0 || floor > 1.0) {
        throw std::invalid_argument("floor must be in the [0.0, 1.0] range.");
    }
    if (ceiling < 0.0 || ceiling > 1.0) {
        throw std::invalid_argument("ceiling must be in the [0.0, 1.0] range.");
    }
    if (ceiling < floor) {
        throw std::invalid_argument("ceiling must be greater than or equal to floor.");
    }

    const double load_ratio = std::max(
        sanitize_percent(cpu_percent) / 100.0,
        sanitize_percent(memory_percent) / 100.0
    );
    const double normalized_phase = std::isfinite(phase) ? std::fmod(phase, 1.0) : 0.0;
    const double pulse_ratio =
        (std::sin(normalized_phase * (2.0 * std::numbers::pi_v<double>)) + 1.0) * 0.5;
    const double pulse_weight = clamp_unit(pulse_strength);
    const double composite = clamp_unit(load_ratio + ((pulse_ratio - 0.5) * pulse_weight));
    return floor + ((ceiling - floor) * composite);
}

CockpitFrameState compose_cockpit_frame(
    double previous_phase,
    double elapsed_since_last_frame,
    double cpu_percent,
    double memory_percent,
    const FrameDiscipline& discipline,
    double pulse_hz
) {
    CockpitFrameState state{};
    state.phase = advance_phase(previous_phase, elapsed_since_last_frame, pulse_hz, discipline);
    state.accent_intensity = compute_accent_intensity(
        cpu_percent,
        memory_percent,
        state.phase,
        0.15,
        0.95,
        0.2
    );
    state.next_delay_seconds = discipline.next_frame_delay_seconds(elapsed_since_last_frame);
    return state;
}

}  // namespace aura::render_native
