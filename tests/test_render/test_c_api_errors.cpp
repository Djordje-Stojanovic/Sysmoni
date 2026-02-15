#include "render_test_helpers.h"
#include <limits>

void test_c_api_error_surface_and_fallbacks() {
    aura_clear_error();
    assert_last_error_clear();

    const AuraFrameDiscipline invalid_discipline{0, 4};
    const double fallback_phase = aura_advance_phase(0.2, 0.01, 0.5, invalid_discipline);
    assert(std::isfinite(fallback_phase));
    assert(fallback_phase >= 0.0 && fallback_phase < 1.0);
    assert_last_error_contains("aura_advance_phase");

    const AuraFrameDiscipline valid_discipline{60, 4};
    (void)aura_advance_phase(0.2, 0.01, 0.5, valid_discipline);
    assert_last_error_clear();

    const double accent = aura_compute_accent_intensity(10.0, 20.0, 0.2, 0.8, 0.2, 0.2);
    assert(std::fabs(accent - 0.8) < kFloatEpsilon);
    assert_last_error_contains("aura_compute_accent_intensity");

    const AuraCockpitFrameState fallback_frame = aura_compose_cockpit_frame(
        std::numeric_limits<double>::quiet_NaN(),
        10.0,
        10.0,
        20.0,
        AuraFrameDiscipline{0, 0},
        -1.0
    );
    assert(std::isfinite(fallback_frame.phase));
    assert(std::isfinite(fallback_frame.accent_intensity));
    assert(std::isfinite(fallback_frame.next_delay_seconds));
    assert(std::fabs(fallback_frame.accent_intensity - 0.15) < kFloatEpsilon);
    assert(fallback_frame.next_delay_seconds > 0.0);
    assert_last_error_contains("aura_compose_cockpit_frame");

    char blended[16] = {};
    aura_blend_hex_color(nullptr, "#ffffff", 0.5, blended, sizeof(blended));
    assert(std::strcmp(blended, "#000000") == 0);
    assert_last_error_contains("aura_blend_hex_color");

    aura_blend_hex_color("#000000", "#ffffff", 0.5, blended, sizeof(blended));
    assert(std::strcmp(blended, "#808080") == 0);
    assert_last_error_clear();

    assert(aura_qt_hooks_create(nullptr, nullptr) == nullptr);
    assert_last_error_contains("aura_qt_hooks_create");
    aura_clear_error();
    assert_last_error_clear();
}

void test_widgets_backend() {
    const char* backend = aura_widget_backend_name();
    assert(backend != nullptr);
    assert(std::strlen(backend) > 0);
    const int available = aura_widget_backend_available();
    assert(available == 0 || available == 1);
}

void test_sanitize_edge_cases() {
    // NaN input -> 0.0
    assert(aura_sanitize_percent(std::numeric_limits<double>::quiet_NaN()) == 0.0);
    assert(aura_sanitize_non_negative(std::numeric_limits<double>::quiet_NaN()) == 0.0);

    // +Infinity -> not finite, sanitize returns 0.0 for both functions
    assert(aura_sanitize_percent(std::numeric_limits<double>::infinity()) == 0.0);
    // sanitize_non_negative: infinity is not finite, returns 0
    assert(aura_sanitize_non_negative(std::numeric_limits<double>::infinity()) == 0.0);

    // -Infinity -> 0
    assert(aura_sanitize_percent(-std::numeric_limits<double>::infinity()) == 0.0);
    assert(aura_sanitize_non_negative(-std::numeric_limits<double>::infinity()) == 0.0);

    // Exact boundaries
    assert(aura_sanitize_percent(0.0) == 0.0);
    assert(aura_sanitize_percent(100.0) == 100.0);
    assert(aura_sanitize_percent(50.0) == 50.0);
    assert(aura_sanitize_non_negative(0.0) == 0.0);
    assert(aura_sanitize_non_negative(9999.0) == 9999.0);

    // Negative input -> 0
    assert(aura_sanitize_percent(-0.001) == 0.0);
    assert(aura_sanitize_non_negative(-0.001) == 0.0);
}

void test_clear_error_idempotent() {
    // Set an error condition first
    (void)aura_advance_phase(0.0, 0.01, 0.5, AuraFrameDiscipline{0, 4});
    assert_last_error_contains("aura_advance_phase");

    // Clear once
    aura_clear_error();
    assert_last_error_clear();

    // Clear again — should still be empty
    aura_clear_error();
    assert_last_error_clear();

    // Clear with no prior error
    aura_clear_error();
    assert_last_error_clear();
}

void test_compose_cockpit_frame_accent_range() {
    const AuraFrameDiscipline discipline{60, 4};

    // Low load -> accent near floor (0.15)
    {
        const AuraCockpitFrameState frame =
            aura_compose_cockpit_frame(0.0, 0.016, 0.0, 0.0, discipline, 0.5);
        assert(std::isfinite(frame.accent_intensity));
        assert(frame.accent_intensity >= 0.15 && frame.accent_intensity <= 0.95);
    }

    // High load -> accent near ceiling (0.95)
    {
        const AuraCockpitFrameState frame =
            aura_compose_cockpit_frame(0.0, 0.016, 100.0, 100.0, discipline, 0.5);
        assert(std::isfinite(frame.accent_intensity));
        assert(frame.accent_intensity >= 0.15 && frame.accent_intensity <= 0.95);
        // High load must produce higher accent than low load
    }

    // next_delay_seconds should be > 0
    {
        const AuraCockpitFrameState frame =
            aura_compose_cockpit_frame(0.0, 0.016, 50.0, 50.0, discipline, 0.5);
        assert(frame.next_delay_seconds >= 0.0);
        assert(std::isfinite(frame.next_delay_seconds));
    }
}

void test_animation() {
    const AuraFrameDiscipline discipline{60, 4};
    const double phase = aura_advance_phase(0.2, 0.005, 0.5, discipline);
    assert(std::fabs(phase - 0.2025) < 1e-6);

    const AuraCockpitFrameState frame =
        aura_compose_cockpit_frame(0.2, 0.005, 35.0, 55.0, discipline, 0.5);
    assert(frame.accent_intensity >= 0.15);
    assert(frame.accent_intensity <= 0.95);
}
