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
