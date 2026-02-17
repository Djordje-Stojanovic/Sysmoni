#pragma once

// Forward declarations for all 15 cockpit controller test functions.
// Tests defined in cockpit_controller_tests.cpp (6 tests):
bool test_happy_path_prefers_dvr();
bool test_telemetry_missing();
bool test_render_missing();
bool test_style_tokens_fallback_when_style_call_fails();
bool test_bounds_sanitized();
bool test_last_good_reused_on_telemetry_failure_preserves_timeline();

// Tests defined in cockpit_controller_sensor_tests.cpp (9 tests):
bool test_falls_back_to_live_when_dvr_unavailable();
bool test_live_ring_respects_capacity();
bool test_anomaly_count_detects_spikes();
bool test_per_core_cpu_flows_through();
bool test_gpu_unavailable_default();
bool test_disk_network_rate_flows_through();
bool test_tiered_polling_gpu_every_2_ticks();
bool test_graceful_degradation_new_sensors();
bool test_thermal_every_5_ticks();

// Tests defined in analytics_bridge_tests.cpp (12 tests):
bool test_health_score_flows_through_tick();
bool test_health_unavailable_preserves_defaults();
bool test_cpu_trend_rising_with_enough_samples();
bool test_trends_need_minimum_10_samples();
bool test_smoothing_toggle_affects_values();
bool test_smoothing_off_by_default();
bool test_alert_evaluation_populates_active_alerts();
bool test_alert_acknowledge();
bool test_analytics_graceful_degradation();
bool test_tiered_analytics_every_n_ticks();
bool test_snapshot_buffer_ring_capacity();
bool test_no_analytics_bridge_works_fine();
