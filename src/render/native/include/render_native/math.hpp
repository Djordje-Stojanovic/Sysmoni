#pragma once

namespace aura::render_native {

struct FrameDiscipline {
    int target_fps{60};
    int max_catchup_frames{4};

    [[nodiscard]] double frame_interval_seconds() const;
    [[nodiscard]] double max_delta_seconds() const;
    [[nodiscard]] double clamp_delta_seconds(double delta_seconds) const;
    [[nodiscard]] double next_frame_delay_seconds(double elapsed_since_last_frame) const;
};

struct CockpitFrameState {
    double phase{0.0};
    double accent_intensity{0.15};
    double next_delay_seconds{0.0};
};

double sanitize_percent(double value);
double sanitize_non_negative(double value);
double clamp_unit(double value);

double advance_phase(
    double phase,
    double delta_seconds,
    double cycles_per_second,
    const FrameDiscipline& discipline
);

double compute_accent_intensity(
    double cpu_percent,
    double memory_percent,
    double phase,
    double floor,
    double ceiling,
    double pulse_strength
);

CockpitFrameState compose_cockpit_frame(
    double previous_phase,
    double elapsed_since_last_frame,
    double cpu_percent,
    double memory_percent,
    const FrameDiscipline& discipline,
    double pulse_hz
);

}  // namespace aura::render_native
