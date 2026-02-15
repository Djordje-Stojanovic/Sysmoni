#include "render_test_helpers.h"

void assert_style_tokens_ranges(const AuraRenderStyleTokens& tokens, int target_fps) {
    assert(std::isfinite(tokens.phase));
    assert(std::isfinite(tokens.next_delay_seconds));
    assert(std::isfinite(tokens.accent_intensity));
    assert(std::isfinite(tokens.accent_red));
    assert(std::isfinite(tokens.accent_green));
    assert(std::isfinite(tokens.accent_blue));
    assert(std::isfinite(tokens.accent_alpha));
    assert(std::isfinite(tokens.frost_intensity));
    assert(std::isfinite(tokens.tint_strength));
    assert(std::isfinite(tokens.ring_line_width));
    assert(std::isfinite(tokens.ring_glow_strength));
    assert(std::isfinite(tokens.cpu_alpha));
    assert(std::isfinite(tokens.memory_alpha));
    assert(std::isfinite(tokens.motion_scale));
    assert(std::isfinite(tokens.timeline_anomaly_alpha));

    assert(tokens.phase >= 0.0 && tokens.phase < 1.0);
    assert(tokens.next_delay_seconds >= 0.0);
    assert(tokens.accent_intensity >= 0.0 && tokens.accent_intensity <= 1.0);
    assert(tokens.accent_red >= 0.0 && tokens.accent_red <= 1.0);
    assert(tokens.accent_green >= 0.0 && tokens.accent_green <= 1.0);
    assert(tokens.accent_blue >= 0.0 && tokens.accent_blue <= 1.0);
    assert(tokens.accent_alpha >= 0.0 && tokens.accent_alpha <= 1.0);
    assert(tokens.frost_intensity >= 0.0 && tokens.frost_intensity <= 1.0);
    assert(tokens.tint_strength >= 0.0 && tokens.tint_strength <= 1.0);
    assert(tokens.ring_line_width > 0.0 && tokens.ring_line_width <= 7.0);
    assert(tokens.ring_glow_strength >= 0.0 && tokens.ring_glow_strength <= 1.0);
    assert(tokens.cpu_alpha >= 0.0 && tokens.cpu_alpha <= 1.0);
    assert(tokens.memory_alpha >= 0.0 && tokens.memory_alpha <= 1.0);
    assert(tokens.severity_level >= 0 && tokens.severity_level <= 3);
    assert(tokens.motion_scale >= 0.60 && tokens.motion_scale <= 1.0);
    assert(tokens.quality_hint == 0 || tokens.quality_hint == 1);
    assert(tokens.timeline_anomaly_alpha >= 0.0 && tokens.timeline_anomaly_alpha <= 1.0);

    const int safe_target_fps = target_fps > 0 ? target_fps : 60;
    assert(tokens.next_delay_seconds <= (1.0 / static_cast<double>(safe_target_fps)) + 1e-6);
}

void assert_style_tokens_close(
    const AuraRenderStyleTokens& actual,
    const AuraRenderStyleTokens& expected,
    double epsilon
) {
    assert(std::fabs(actual.phase - expected.phase) <= epsilon);
    assert(std::fabs(actual.next_delay_seconds - expected.next_delay_seconds) <= epsilon);
    assert(std::fabs(actual.accent_intensity - expected.accent_intensity) <= epsilon);
    assert(std::fabs(actual.accent_red - expected.accent_red) <= epsilon);
    assert(std::fabs(actual.accent_green - expected.accent_green) <= epsilon);
    assert(std::fabs(actual.accent_blue - expected.accent_blue) <= epsilon);
    assert(std::fabs(actual.accent_alpha - expected.accent_alpha) <= epsilon);
    assert(std::fabs(actual.frost_intensity - expected.frost_intensity) <= epsilon);
    assert(std::fabs(actual.tint_strength - expected.tint_strength) <= epsilon);
    assert(std::fabs(actual.ring_line_width - expected.ring_line_width) <= epsilon);
    assert(std::fabs(actual.ring_glow_strength - expected.ring_glow_strength) <= epsilon);
    assert(std::fabs(actual.cpu_alpha - expected.cpu_alpha) <= epsilon);
    assert(std::fabs(actual.memory_alpha - expected.memory_alpha) <= epsilon);
}
