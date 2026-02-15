#include "render_test_helpers.h"
#include <limits>

void test_theme() {
    char out[16] = {};
    aura_blend_hex_color("#205b8e", "#3f8fd8", 1.0, out, sizeof(out));
    assert(std::strcmp(out, "#3f8fd8") == 0);
    assert(aura_quantize_accent_intensity(0.504) == 50);
}

void test_blend_at_ratio_zero_returns_start() {
    char out[16] = {};
    aura_blend_hex_color("#3b82f6", "#ef4444", 0.0, out, sizeof(out));
    assert(std::strcmp(out, "#3b82f6") == 0);
    assert_last_error_clear();
}

void test_blend_at_ratio_one_returns_end() {
    char out[16] = {};
    aura_blend_hex_color("#3b82f6", "#ef4444", 1.0, out, sizeof(out));
    assert(std::strcmp(out, "#ef4444") == 0);
    assert_last_error_clear();
}

void test_blend_symmetry_at_midpoint() {
    // Black (#000000) to white (#ffffff) at 0.5 should be #808080.
    char out[16] = {};
    aura_blend_hex_color("#000000", "#ffffff", 0.5, out, sizeof(out));
    assert(std::strcmp(out, "#808080") == 0);
    assert_last_error_clear();
}

void test_blend_ratio_clamped_above_one() {
    char out[16] = {};
    aura_blend_hex_color("#000000", "#ffffff", 2.5, out, sizeof(out));
    assert(std::strcmp(out, "#ffffff") == 0);
    assert_last_error_clear();
}

void test_blend_ratio_clamped_below_zero() {
    char out[16] = {};
    aura_blend_hex_color("#000000", "#ffffff", -1.0, out, sizeof(out));
    assert(std::strcmp(out, "#000000") == 0);
    assert_last_error_clear();
}

void test_blend_null_end_produces_fallback() {
    char out[16] = {};
    aura_blend_hex_color("#3b82f6", nullptr, 0.5, out, sizeof(out));
    assert(std::strcmp(out, "#000000") == 0);
    assert_last_error_contains("aura_blend_hex_color");
}

void test_blend_malformed_hex_produces_fallback() {
    char out[16] = {};
    aura_blend_hex_color("not_a_color", "#ffffff", 0.5, out, sizeof(out));
    assert(std::strcmp(out, "#000000") == 0);
    assert_last_error_contains("aura_blend_hex_color");
}

void test_quantize_accent_intensity_boundaries() {
    assert(aura_quantize_accent_intensity(0.0) == 0);
    assert(aura_quantize_accent_intensity(1.0) == 100);
    assert(aura_quantize_accent_intensity(0.5) == 50);

    // Over-range clamped to 100.
    assert(aura_quantize_accent_intensity(1.5) == 100);

    // Under-range clamped to 0.
    assert(aura_quantize_accent_intensity(-0.5) == 0);

    // NaN clamped to 0.
    assert(aura_quantize_accent_intensity(std::numeric_limits<double>::quiet_NaN()) == 0);

    // Infinity is not finite — clamp_unit returns 0.0, so quantize returns 0.
    assert(aura_quantize_accent_intensity(std::numeric_limits<double>::infinity()) == 0);

    // Rounding at .5 boundary.
    assert(aura_quantize_accent_intensity(0.504) == 50);
    assert(aura_quantize_accent_intensity(0.005) == 1);
    assert(aura_quantize_accent_intensity(0.994) == 99);
}

void test_phase_wraps_at_unity() {
    // With phase close to 1.0, advancing should wrap back to [0, 1)
    const AuraFrameDiscipline discipline{60, 4};

    // phase=0.99, delta=0.02, pulse_hz=1.0 -> 0.99 + 0.02*1.0 = 1.01 -> wraps to 0.01
    const double result = aura_advance_phase(0.99, 0.02, 1.0, discipline);
    assert(result >= 0.0 && result < 1.0);
    assert(std::fabs(result - 0.01) < 1e-9);
    assert_last_error_clear();

    // phase=0.999, delta very small, should remain near 1.0 but wrapped
    const double r2 = aura_advance_phase(0.999, 0.001, 1.0, discipline);
    assert(r2 >= 0.0 && r2 < 1.0);
    assert_last_error_clear();
}

void test_phase_zero_delta_no_advance() {
    const AuraFrameDiscipline discipline{60, 4};
    const double result = aura_advance_phase(0.3, 0.0, 0.5, discipline);
    assert(std::fabs(result - 0.3) < kFloatEpsilon);
    assert_last_error_clear();
}

void test_phase_negative_delta_no_advance() {
    const AuraFrameDiscipline discipline{60, 4};
    const double result = aura_advance_phase(0.3, -1.0, 0.5, discipline);
    // Negative delta is clamped to 0 by FrameDiscipline::clamp_delta_seconds
    assert(std::fabs(result - 0.3) < kFloatEpsilon);
    assert_last_error_clear();
}
