#include "cockpit_test_fakes.hpp"
#include "cockpit_test_registry.hpp"

#include "aura_shell/cockpit_controller.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

bool test_happy_path_prefers_dvr() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController::Config config;
    config.max_process_rows = 5U;
    config.db_path = "C:/tmp/aura.db";

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000000.0);

    bool ok = true;
    ok &= expect_true(state.telemetry_available, "happy: telemetry available");
    ok &= expect_true(state.render_available, "happy: render available");
    ok &= expect_true(!state.degraded, "happy: state not degraded");
    ok &= expect_true(!state.cpu_line.empty(), "happy: cpu line populated");
    ok &= expect_true(state.process_rows.size() == 2U, "happy: process rows");
    ok &= expect_true(state.accent_intensity > 0.0, "happy: accent intensity");
    ok &= expect_true(state.style_tokens_available, "happy: style tokens available");
    ok &= expect_true(style_tokens_valid(state.style_tokens), "happy: style token ranges");
    ok &= expect_true(state.style_token_error.empty(), "happy: no style token error");
    ok &= expect_true(state.severity_level >= 0 && state.severity_level <= 3, "happy: severity range");
    ok &= expect_true(state.motion_scale >= 0.60 && state.motion_scale <= 1.0, "happy: motion scale range");
    ok &= expect_true(state.fps_target >= 1 && state.fps_target <= 120, "happy: fps target");
    ok &= expect_true(state.fps_recommended_delay_ms >= 16, "happy: delay floor");
    ok &= expect_true(contains(state.status_line, "render=ok"), "happy: render status");
    ok &= expect_true(state.timeline_source == aura::shell::TimelineSource::Dvr, "happy: dvr source");
    ok &= expect_true(state.timeline_points.size() >= 8U, "happy: dvr points");
    ok &= expect_true(contains(state.timeline_line, "timeline=dvr"), "happy: timeline line");
    return ok;
}

bool test_telemetry_missing() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    telemetry->backend_available = false;
    telemetry->next_snapshot = std::nullopt;
    telemetry->snapshot_error = "collector missing";
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {}
    );
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000001.0);

    bool ok = true;
    ok &= expect_true(!state.telemetry_available, "telemetry-missing: telemetry unavailable");
    ok &= expect_true(state.degraded, "telemetry-missing: degraded");
    ok &= expect_true(contains(state.status_line, "Telemetry degraded"), "telemetry-missing: status contains");
    ok &= expect_true(!state.cpu_line.empty(), "telemetry-missing: fallback cpu line");
    return ok;
}

bool test_render_missing() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    render->backend_available = false;
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {}
    );
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000002.0);

    bool ok = true;
    ok &= expect_true(state.telemetry_available, "render-missing: telemetry available");
    ok &= expect_true(!state.render_available, "render-missing: render unavailable");
    ok &= expect_true(state.degraded, "render-missing: degraded");
    ok &= expect_true(!state.style_tokens_available, "render-missing: style fallback");
    ok &= expect_true(style_tokens_valid(state.style_tokens), "render-missing: style fallback ranges");
    ok &= expect_true(!state.style_token_error.empty(), "render-missing: style fallback error");
    ok &= expect_true(state.severity_level == 0, "render-missing: default severity");
    ok &= expect_true(state.quality_hint == 0, "render-missing: default quality");
    ok &= expect_true(contains(state.status_line, "render=fallback"), "render-missing: fallback status");
    ok &= expect_true(!state.process_rows.empty(), "render-missing: process rows");
    return ok;
}

bool test_style_tokens_fallback_when_style_call_fails() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    render->fail_style_tokens = true;
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {}
    );
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000002.5);

    bool ok = true;
    ok &= expect_true(state.telemetry_available, "style-fallback: telemetry available");
    ok &= expect_true(state.render_available, "style-fallback: render still available");
    ok &= expect_true(state.degraded, "style-fallback: degraded");
    ok &= expect_true(!state.style_tokens_available, "style-fallback: style fallback active");
    ok &= expect_true(style_tokens_valid(state.style_tokens), "style-fallback: style fallback ranges");
    ok &= expect_true(contains(state.style_token_error, "style tokens failed"), "style-fallback: error");
    ok &= expect_true(contains(state.status_line, "warning=style tokens failed"), "style-fallback: status warning");
    return ok;
}

bool test_bounds_sanitized() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    telemetry->next_snapshot = aura::shell::TelemetrySnapshot{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
    };
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {}
    );
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000003.0);

    bool ok = true;
    ok &= expect_true(std::isfinite(state.cpu_percent), "bounds: cpu finite");
    ok &= expect_true(std::isfinite(state.memory_percent), "bounds: memory finite");
    ok &= expect_true(state.cpu_percent >= 0.0 && state.cpu_percent <= 100.0, "bounds: cpu range");
    ok &= expect_true(state.memory_percent >= 0.0 && state.memory_percent <= 100.0, "bounds: memory range");
    return ok;
}

bool test_last_good_reused_on_telemetry_failure_preserves_timeline() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto* telemetry_ptr = telemetry.get();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController::Config config;
    config.db_path = "C:/tmp/aura.db";

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );
    const aura::shell::CockpitUiState initial = controller.tick(1.0, 1700000004.0);

    telemetry_ptr->next_snapshot = std::nullopt;
    telemetry_ptr->snapshot_error = "transient timeout";
    const aura::shell::CockpitUiState degraded = controller.tick(1.0, 1700000005.0);

    bool ok = true;
    ok &= expect_true(initial.telemetry_available, "reuse: initial telemetry available");
    ok &= expect_true(degraded.degraded, "reuse: degraded after failure");
    ok &= expect_true(!degraded.telemetry_available, "reuse: telemetry unavailable");
    ok &= expect_true(degraded.cpu_line == initial.cpu_line, "reuse: cpu line preserved");
    ok &= expect_true(contains(degraded.status_line, "Telemetry degraded"), "reuse: status contains");
    ok &= expect_true(degraded.timeline_source == initial.timeline_source, "reuse: timeline source preserved");
    ok &= expect_true(degraded.timeline_points.size() == initial.timeline_points.size(), "reuse: timeline points preserved");
    return ok;
}

int main() {
    int failures = 0;

    // --- Core tests (defined here) ---
    if (!test_happy_path_prefers_dvr()) {
        ++failures;
    }
    if (!test_telemetry_missing()) {
        ++failures;
    }
    if (!test_render_missing()) {
        ++failures;
    }
    if (!test_bounds_sanitized()) {
        ++failures;
    }
    if (!test_style_tokens_fallback_when_style_call_fails()) {
        ++failures;
    }
    if (!test_last_good_reused_on_telemetry_failure_preserves_timeline()) {
        ++failures;
    }

    // --- Sensor tests (defined in cockpit_controller_sensor_tests.cpp) ---
    if (!test_falls_back_to_live_when_dvr_unavailable()) {
        ++failures;
    }
    if (!test_live_ring_respects_capacity()) {
        ++failures;
    }
    if (!test_anomaly_count_detects_spikes()) {
        ++failures;
    }
    if (!test_per_core_cpu_flows_through()) {
        ++failures;
    }
    if (!test_gpu_unavailable_default()) {
        ++failures;
    }
    if (!test_disk_network_rate_flows_through()) {
        ++failures;
    }
    if (!test_tiered_polling_gpu_every_2_ticks()) {
        ++failures;
    }
    if (!test_graceful_degradation_new_sensors()) {
        ++failures;
    }
    if (!test_thermal_every_5_ticks()) {
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All cockpit controller tests passed." << '\n';
        return 0;
    }
    std::cerr << failures << " cockpit controller tests failed." << '\n';
    return 1;
}
