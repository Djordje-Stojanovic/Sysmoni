#include "render_test_helpers.h"
#include <limits>

void test_style_tokens_nominal_ranges() {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = 0.2;
    input.cpu_percent = 35.0;
    input.memory_percent = 55.0;
    input.elapsed_since_last_frame = 0.008;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
    assert_style_tokens_ranges(tokens, input.target_fps);
}

void test_style_tokens_sanitization_and_defaults() {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = std::numeric_limits<double>::quiet_NaN();
    input.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    input.memory_percent = std::numeric_limits<double>::infinity();
    input.elapsed_since_last_frame = 10.0;
    input.pulse_hz = -2.0;
    input.target_fps = 0;
    input.max_catchup_frames = -4;

    const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
    assert_style_tokens_ranges(tokens, 60);
}

void test_style_tokens_phase_progression() {
    AuraRenderStyleTokensInput input{};
    input.previous_phase = 0.2;
    input.cpu_percent = 45.0;
    input.memory_percent = 60.0;
    input.elapsed_since_last_frame = 0.01;
    input.pulse_hz = 0.5;
    input.target_fps = 60;
    input.max_catchup_frames = 4;

    const AuraRenderStyleTokens first = aura_compute_style_tokens(input);
    assert(std::fabs(first.phase - 0.205) < 1e-6);

    input.previous_phase = first.phase;
    const AuraRenderStyleTokens second = aura_compute_style_tokens(input);
    assert(std::fabs(second.phase - 0.21) < 1e-6);
    assert(second.phase > first.phase);
}

void test_style_tokens_adaptive_fields_progress_with_load() {
    AuraRenderStyleTokensInput low_input{};
    low_input.previous_phase = 0.0;
    low_input.cpu_percent = 18.0;
    low_input.memory_percent = 24.0;
    low_input.elapsed_since_last_frame = 1.0 / 60.0;
    low_input.pulse_hz = 0.5;
    low_input.target_fps = 60;
    low_input.max_catchup_frames = 4;
    const AuraRenderStyleTokens low = aura_compute_style_tokens(low_input);
    assert_style_tokens_ranges(low, low_input.target_fps);

    AuraRenderStyleTokensInput high_input{};
    high_input.previous_phase = low.phase;
    high_input.cpu_percent = 96.0;
    high_input.memory_percent = 94.0;
    high_input.elapsed_since_last_frame = 1.0 / 60.0;
    high_input.pulse_hz = 0.5;
    high_input.target_fps = 60;
    high_input.max_catchup_frames = 4;
    const AuraRenderStyleTokens high = aura_compute_style_tokens(high_input);
    assert_style_tokens_ranges(high, high_input.target_fps);

    assert(high.severity_level >= low.severity_level);
    assert(high.motion_scale <= low.motion_scale + 1e-9);
    assert(high.timeline_anomaly_alpha >= low.timeline_anomaly_alpha - 1e-9);
    assert(high.quality_hint >= low.quality_hint);
}

void test_style_tokens_boundary_accent_values() {
    // cpu=0, memory=0 → minimum accent, accent_intensity close to floor (0.15)
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 0.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert_style_tokens_ranges(tokens, 60);
        // accent_intensity is >= 0.15 (floor) at minimum load
        assert(tokens.accent_intensity >= 0.15);
    }

    // cpu=100, memory=100 → maximum accent, accent_intensity close to ceiling (0.95)
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 100.0;
        input.memory_percent = 100.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert_style_tokens_ranges(tokens, 60);
        assert(tokens.accent_intensity <= 0.95);
        assert(tokens.accent_intensity >= 0.15);
    }

    // Verify accent_intensity=0 boundary in style token math:
    // ring_line_width = 1 + accent*6, so at accent=0 → 1.0
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 0.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        // ring_line_width = 1 + (accent_intensity * 6), so it must be >= 1 and <= 7
        assert(tokens.ring_line_width >= 1.0);
        assert(tokens.ring_line_width <= 7.0);
        // Verify derived formula: ring_line_width == 1 + accent_intensity * 6
        const double expected_ring = 1.0 + (tokens.accent_intensity * 6.0);
        assert(std::fabs(tokens.ring_line_width - expected_ring) < kFloatEpsilon);
    }

    // Verify accent derivation consistency across all derived tokens
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.5;
        input.cpu_percent = 50.0;
        input.memory_percent = 50.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert_style_tokens_ranges(tokens, 60);

        const double a = tokens.accent_intensity;
        // accent_red = clamp(0.12 + accent*0.65)
        assert(std::fabs(tokens.accent_red - (0.12 + (a * 0.65))) < kFloatEpsilon);
        // accent_green = clamp(0.30 + accent*0.50)
        assert(std::fabs(tokens.accent_green - (0.30 + (a * 0.50))) < kFloatEpsilon);
        // accent_blue = clamp(0.48 + accent*0.42)
        assert(std::fabs(tokens.accent_blue - (0.48 + (a * 0.42))) < kFloatEpsilon);
        // accent_alpha = clamp(0.62 + accent*0.33)
        assert(std::fabs(tokens.accent_alpha - (0.62 + (a * 0.33))) < kFloatEpsilon);
        // frost_intensity = clamp(0.05 + accent*0.30)
        assert(std::fabs(tokens.frost_intensity - (0.05 + (a * 0.30))) < kFloatEpsilon);
        // tint_strength = clamp(0.10 + accent*0.50)
        assert(std::fabs(tokens.tint_strength - (0.10 + (a * 0.50))) < kFloatEpsilon);
        // ring_line_width = 1 + accent*6
        assert(std::fabs(tokens.ring_line_width - (1.0 + (a * 6.0))) < kFloatEpsilon);
        // ring_glow_strength = clamp(0.20 + accent*0.75)
        assert(std::fabs(tokens.ring_glow_strength - (0.20 + (a * 0.75))) < kFloatEpsilon);
    }
}

void test_style_tokens_cpu_memory_alpha_derivation() {
    // At cpu=0, memory=0: cpu_alpha = 0.20, memory_alpha = 0.20
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 0.0;
        input.memory_percent = 0.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert(std::fabs(tokens.cpu_alpha - 0.20) < kFloatEpsilon);
        assert(std::fabs(tokens.memory_alpha - 0.20) < kFloatEpsilon);
    }

    // At cpu=100, memory=100: cpu_alpha = 0.95, memory_alpha = 0.95
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 100.0;
        input.memory_percent = 100.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        assert(std::fabs(tokens.cpu_alpha - 0.95) < kFloatEpsilon);
        assert(std::fabs(tokens.memory_alpha - 0.95) < kFloatEpsilon);
    }

    // At cpu=50, memory=20: values computed independently
    {
        AuraRenderStyleTokensInput input{};
        input.previous_phase = 0.0;
        input.cpu_percent = 50.0;
        input.memory_percent = 20.0;
        input.elapsed_since_last_frame = 0.016;
        input.pulse_hz = 0.5;
        input.target_fps = 60;
        input.max_catchup_frames = 4;
        const AuraRenderStyleTokens tokens = aura_compute_style_tokens(input);
        // cpu_alpha = 0.20 + (50/100)*0.75 = 0.20 + 0.375 = 0.575
        assert(std::fabs(tokens.cpu_alpha - 0.575) < kFloatEpsilon);
        // memory_alpha = 0.20 + (20/100)*0.75 = 0.20 + 0.15 = 0.35
        assert(std::fabs(tokens.memory_alpha - 0.35) < kFloatEpsilon);
    }
}

void test_metrics() {
    assert(aura_sanitize_percent(-1.0) == 0.0);
    assert(aura_sanitize_percent(42.5) == 42.5);
    assert(aura_sanitize_percent(120.0) == 100.0);
    assert(aura_sanitize_non_negative(-1.0) == 0.0);
    assert(aura_sanitize_non_negative(12.0) == 12.0);
}
