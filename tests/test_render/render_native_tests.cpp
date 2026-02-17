#include <cstdio>

// --- test_style_tokens.cpp ---
void test_style_tokens_nominal_ranges();
void test_style_tokens_sanitization_and_defaults();
void test_style_tokens_phase_progression();
void test_style_tokens_adaptive_fields_progress_with_load();
void test_style_tokens_boundary_accent_values();
void test_style_tokens_cpu_memory_alpha_derivation();
void test_metrics();

// --- test_style_sequencer.cpp ---
void test_style_sequencer_lifecycle_and_null_safety();
void test_style_sequencer_deterministic_reset_progression();
void test_style_sequencer_asymmetric_smoothing();
void test_style_sequencer_frame_spike_clamping();
void test_style_sequencer_sanitization();
void test_style_sequencer_stateless_parity_with_tiny_half_life();
void test_style_sequencer_phase_monotonically_advances();
void test_style_sequencer_error_clears_on_success();

// --- test_theme_blend.cpp ---
void test_theme();
void test_blend_at_ratio_zero_returns_start();
void test_blend_at_ratio_one_returns_end();
void test_blend_symmetry_at_midpoint();
void test_blend_ratio_clamped_above_one();
void test_blend_ratio_clamped_below_zero();
void test_blend_null_end_produces_fallback();
void test_blend_malformed_hex_produces_fallback();
void test_quantize_accent_intensity_boundaries();
void test_phase_wraps_at_unity();
void test_phase_zero_delta_no_advance();
void test_phase_negative_delta_no_advance();

// --- test_formatting.cpp ---
void test_formatting_and_status();
void test_format_disk_rate();
void test_format_network_rate();
void test_format_process_row_empty_name();
void test_format_process_row_null_name();
void test_format_process_row_zero_values();
void test_format_process_row_huge_memory();
void test_format_process_row_clamped_cpu();
void test_format_process_row_nan_cpu();
void test_format_snapshot_lines_nan_inf();
void test_format_disk_rate_zero();
void test_format_disk_rate_nan();
void test_format_disk_rate_infinity();
void test_format_disk_rate_exact_gb();
void test_format_network_rate_nan();
void test_format_network_rate_negative();
void test_format_network_rate_exact_mb();
void test_format_initial_status_no_db_no_samples();
void test_format_initial_status_with_error();
void test_format_stream_status_null_db();

// --- test_qt_hooks.cpp ---
void test_qt_hooks_caps_and_lifecycle();
void test_qt_hooks_callback_order_and_ranges();
void test_qt_hooks_sanitization_and_clamping();
void test_style_tokens_match_qt_hook_outputs();
void test_qt_hooks_error_surface();
void test_qt_hooks_multi_frame_accumulation();
void test_qt_hooks_rejects_partial_callbacks();
void test_qt_hooks_cpu_alpha_tracks_load();

// --- test_c_api_errors.cpp ---
void test_c_api_error_surface_and_fallbacks();
void test_widgets_backend();
void test_sanitize_edge_cases();
void test_clear_error_idempotent();
void test_compose_cockpit_frame_accent_range();
void test_animation();

// --- test_severity_pipeline.cpp ---
void test_severity_level_load_boundaries();
void test_severity_level_slope_compounds();
void test_severity_memory_drives_load();
void test_motion_scale_per_severity();
void test_motion_scale_slope_penalty();
void test_quality_hint_thresholds();
void test_timeline_anomaly_alpha_range();
void test_timeline_anomaly_alpha_monotonic_with_load();
void test_severity_pipeline_all_nan();

// --- test_gauge_color.cpp ---
void test_gauge_color_flat_blue_segment();
void test_gauge_color_blue_to_cyan_blend();
void test_gauge_color_cyan_to_amber_blend();
void test_gauge_color_amber_to_red_blend();
void test_gauge_color_boundary_continuity();
void test_gauge_color_nan_inf_negative();
void test_gauge_color_hex_consistency();
void test_luminance_known_values();
void test_luminance_monotonically_increases();
void test_contrast_ratio_black_vs_white();
void test_contrast_ratio_identical_colors();
void test_contrast_ratio_symmetric();
void test_contrast_ratio_range();
void test_palette_accessibility();
void test_parse_hex_color_cases();

int main() {
    // --- Severity pipeline tests ---
    test_severity_level_load_boundaries();
    test_severity_level_slope_compounds();
    test_severity_memory_drives_load();
    test_motion_scale_per_severity();
    test_motion_scale_slope_penalty();
    test_quality_hint_thresholds();
    test_timeline_anomaly_alpha_range();
    test_timeline_anomaly_alpha_monotonic_with_load();
    test_severity_pipeline_all_nan();

    // --- Gauge color and accessibility tests ---
    test_gauge_color_flat_blue_segment();
    test_gauge_color_blue_to_cyan_blend();
    test_gauge_color_cyan_to_amber_blend();
    test_gauge_color_amber_to_red_blend();
    test_gauge_color_boundary_continuity();
    test_gauge_color_nan_inf_negative();
    test_gauge_color_hex_consistency();
    test_luminance_known_values();
    test_luminance_monotonically_increases();
    test_contrast_ratio_black_vs_white();
    test_contrast_ratio_identical_colors();
    test_contrast_ratio_symmetric();
    test_contrast_ratio_range();
    test_palette_accessibility();
    test_parse_hex_color_cases();

    // --- Existing tests (unchanged) ---
    test_metrics();
    test_animation();
    test_style_tokens_nominal_ranges();
    test_style_tokens_sanitization_and_defaults();
    test_style_tokens_phase_progression();
    test_style_tokens_adaptive_fields_progress_with_load();
    test_style_sequencer_lifecycle_and_null_safety();
    test_style_sequencer_deterministic_reset_progression();
    test_style_sequencer_asymmetric_smoothing();
    test_style_sequencer_frame_spike_clamping();
    test_style_sequencer_sanitization();
    test_style_sequencer_stateless_parity_with_tiny_half_life();
    test_theme();
    test_c_api_error_surface_and_fallbacks();
    test_formatting_and_status();
    test_format_disk_rate();
    test_format_network_rate();
    test_widgets_backend();
    test_qt_hooks_caps_and_lifecycle();
    test_qt_hooks_callback_order_and_ranges();
    test_qt_hooks_sanitization_and_clamping();
    test_style_tokens_match_qt_hook_outputs();
    test_qt_hooks_error_surface();

    // --- Theme / blend tests ---
    test_blend_at_ratio_zero_returns_start();
    test_blend_at_ratio_one_returns_end();
    test_blend_symmetry_at_midpoint();
    test_blend_ratio_clamped_above_one();
    test_blend_ratio_clamped_below_zero();
    test_blend_null_end_produces_fallback();
    test_blend_malformed_hex_produces_fallback();
    test_quantize_accent_intensity_boundaries();

    // --- Style token boundary and derivation tests ---
    test_style_tokens_boundary_accent_values();
    test_style_tokens_cpu_memory_alpha_derivation();

    // --- Phase advance edge cases ---
    test_phase_wraps_at_unity();
    test_phase_zero_delta_no_advance();
    test_phase_negative_delta_no_advance();

    // --- Formatting edge cases ---
    test_format_process_row_empty_name();
    test_format_process_row_null_name();
    test_format_process_row_zero_values();
    test_format_process_row_huge_memory();
    test_format_process_row_clamped_cpu();
    test_format_process_row_nan_cpu();
    test_format_snapshot_lines_nan_inf();
    test_format_disk_rate_zero();
    test_format_disk_rate_nan();
    test_format_disk_rate_infinity();
    test_format_disk_rate_exact_gb();
    test_format_network_rate_nan();
    test_format_network_rate_negative();
    test_format_network_rate_exact_mb();

    // --- Status formatting edge cases ---
    test_format_initial_status_no_db_no_samples();
    test_format_initial_status_with_error();
    test_format_stream_status_null_db();

    // --- Style sequencer behavioral tests ---
    test_style_sequencer_phase_monotonically_advances();
    test_style_sequencer_error_clears_on_success();

    // --- Qt hooks multi-frame and partial callback tests ---
    test_qt_hooks_multi_frame_accumulation();
    test_qt_hooks_rejects_partial_callbacks();

    // --- sanitize edge cases ---
    test_sanitize_edge_cases();

    // --- Error API tests ---
    test_clear_error_idempotent();

    // --- cockpit frame accent range ---
    test_compose_cockpit_frame_accent_range();

    // --- cpu/memory alpha load tracking ---
    test_qt_hooks_cpu_alpha_tracks_load();

    return 0;
}
