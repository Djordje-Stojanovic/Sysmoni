#include "cockpit_test_fakes.hpp"
#include "cockpit_test_registry.hpp"

#include "aura_shell/cockpit_controller.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// 1. Health score flows through tick() → state.health
// ---------------------------------------------------------------------------
bool test_health_score_flows_through_tick() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->next_health = {true, 82.0, 90.0, 75.0, 88.0, 95.0};

    aura::shell::CockpitController::Config config;
    config.analytics_refresh_ticks = 1;  // Every tick

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );
    const auto state = controller.tick(1.0, 1700000000.0);

    bool ok = true;
    ok &= expect_true(state.health.available, "health-flow: available");
    ok &= expect_true(std::abs(state.health.overall - 82.0) < 0.01, "health-flow: overall");
    ok &= expect_true(std::abs(state.health.cpu - 90.0) < 0.01, "health-flow: cpu");
    ok &= expect_true(std::abs(state.health.memory - 75.0) < 0.01, "health-flow: memory");
    ok &= expect_true(std::abs(state.health.disk - 88.0) < 0.01, "health-flow: disk");
    ok &= expect_true(std::abs(state.health.network - 95.0) < 0.01, "health-flow: network");
    return ok;
}

// ---------------------------------------------------------------------------
// 2. Health unavailable when no analytics bridge → defaults preserved
// ---------------------------------------------------------------------------
bool test_health_unavailable_preserves_defaults() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->health_enabled = false;

    aura::shell::CockpitController::Config config;
    config.analytics_refresh_ticks = 1;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );
    const auto state = controller.tick(1.0, 1700000000.0);

    bool ok = true;
    ok &= expect_true(!state.health.available, "health-unavail: not available");
    ok &= expect_true(std::abs(state.health.overall - 50.0) < 0.01, "health-unavail: default overall");
    return ok;
}

// ---------------------------------------------------------------------------
// 3. CPU trend rising with 10+ buffered samples
// ---------------------------------------------------------------------------
bool test_cpu_trend_rising_with_enough_samples() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->next_cpu_trend = {aura::shell::TrendDirection::Rising, 1.5, 0.9};

    aura::shell::CockpitController::Config config;
    config.analytics_refresh_ticks = 1;
    config.snapshot_buffer_capacity = 60;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );

    // Feed 10+ ticks to fill the buffer
    for (int i = 0; i < 11; ++i) {
        controller.tick(1.0, 1700000000.0 + i);
    }
    const auto state = controller.last_state();

    bool ok = true;
    ok &= expect_true(state.cpu_trend == aura::shell::TrendDirection::Rising, "trend-rising: cpu rising");
    return ok;
}

// ---------------------------------------------------------------------------
// 4. Trends need minimum 10 samples
// ---------------------------------------------------------------------------
bool test_trends_need_minimum_10_samples() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    auto* analytics_ptr = analytics.get();
    analytics->next_cpu_trend = {aura::shell::TrendDirection::Rising, 1.5, 0.9};

    aura::shell::CockpitController::Config config;
    config.analytics_refresh_ticks = 1;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );

    // Only 5 ticks — not enough for trends
    for (int i = 0; i < 5; ++i) {
        controller.tick(1.0, 1700000000.0 + i);
    }

    bool ok = true;
    ok &= expect_true(analytics_ptr->trend_call_count == 0, "trend-min: no trend calls with <10 samples");
    ok &= expect_true(controller.last_state().cpu_trend == aura::shell::TrendDirection::Stable, "trend-min: stable default");
    return ok;
}

// ---------------------------------------------------------------------------
// 5. Smoothing toggle affects displayed values
// ---------------------------------------------------------------------------
bool test_smoothing_toggle_affects_values() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    telemetry->next_snapshot = aura::shell::TelemetrySnapshot{50.0, 60.0};
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->smoother_enabled = true;

    aura::shell::CockpitController::Config config;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );

    // Without smoothing
    auto state_off = controller.tick(1.0, 1700000000.0);
    const double cpu_raw = state_off.cpu_percent;

    // Enable smoothing
    controller.set_smoothing_enabled(true);
    auto state_on = controller.tick(1.0, 1700000001.0);

    bool ok = true;
    ok &= expect_true(state_on.smoothing_active, "smoothing-toggle: active flag");
    // Smoothed value should differ from raw (fake applies 0.7x + 15)
    ok &= expect_true(std::abs(state_on.cpu_percent - cpu_raw) > 0.01, "smoothing-toggle: value differs");
    return ok;
}

// ---------------------------------------------------------------------------
// 6. Smoothing off by default
// ---------------------------------------------------------------------------
bool test_smoothing_off_by_default() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {},
        nullptr,
        std::move(analytics)
    );

    bool ok = true;
    ok &= expect_true(!controller.smoothing_enabled(), "smoothing-default: disabled");
    const auto state = controller.tick(1.0, 1700000000.0);
    ok &= expect_true(!state.smoothing_active, "smoothing-default: state inactive");
    return ok;
}

// ---------------------------------------------------------------------------
// 7. Alert evaluation populates active_alerts
// ---------------------------------------------------------------------------
bool test_alert_evaluation_populates_active_alerts() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->alerts_enabled = true;
    analytics->next_alerts = {
        {1, 2, 92.0, 95.0, 5.5, false},
        {2, 1, 87.0, 88.0, 2.0, false},
    };

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {},
        nullptr,
        std::move(analytics)
    );
    const auto state = controller.tick(1.0, 1700000000.0);

    bool ok = true;
    ok &= expect_true(state.active_alerts.size() == 2U, "alerts-populate: 2 alerts");
    ok &= expect_true(state.active_alerts[0].rule_id == 1, "alerts-populate: first rule_id");
    ok &= expect_true(state.active_alerts[0].state == 2, "alerts-populate: first state=triggered");
    ok &= expect_true(std::abs(state.active_alerts[0].peak_value - 95.0) < 0.01, "alerts-populate: peak");
    return ok;
}

// ---------------------------------------------------------------------------
// 8. Alert acknowledge
// ---------------------------------------------------------------------------
bool test_alert_acknowledge() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->alerts_enabled = true;
    auto* analytics_ptr = analytics.get();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {},
        nullptr,
        std::move(analytics)
    );

    std::string error;
    const bool ack_ok = controller.acknowledge_alert(1, error);

    bool ok = true;
    ok &= expect_true(ack_ok, "alert-ack: success");
    ok &= expect_true(analytics_ptr->last_acknowledged_rule_id == 1, "alert-ack: correct rule_id");
    return ok;
}

// ---------------------------------------------------------------------------
// 9. Analytics graceful degradation — rest of state unaffected
// ---------------------------------------------------------------------------
bool test_analytics_graceful_degradation() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    analytics->backend_available = false;  // Analytics fully unavailable

    aura::shell::CockpitController::Config config;
    config.db_path = "C:/tmp/aura.db";

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );
    const auto state = controller.tick(1.0, 1700000000.0);

    bool ok = true;
    // Core telemetry still works
    ok &= expect_true(state.telemetry_available, "graceful-degrade: telemetry ok");
    ok &= expect_true(state.render_available, "graceful-degrade: render ok");
    // Analytics defaults
    ok &= expect_true(!state.health.available, "graceful-degrade: health unavailable");
    ok &= expect_true(state.cpu_trend == aura::shell::TrendDirection::Stable, "graceful-degrade: cpu stable");
    ok &= expect_true(state.active_alerts.empty(), "graceful-degrade: no alerts");
    ok &= expect_true(!state.smoothing_active, "graceful-degrade: no smoothing");
    return ok;
}

// ---------------------------------------------------------------------------
// 10. Tiered polling — health/trends update every N ticks
// ---------------------------------------------------------------------------
bool test_tiered_analytics_every_n_ticks() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    auto* analytics_ptr = analytics.get();

    aura::shell::CockpitController::Config config;
    config.analytics_refresh_ticks = 3;
    config.snapshot_buffer_capacity = 60;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );

    // Tick 1: analytics fires (first tick)
    controller.tick(1.0, 1700000000.0);
    const int after_1 = analytics_ptr->health_call_count;

    // Tick 2,3: no analytics
    controller.tick(1.0, 1700000001.0);
    controller.tick(1.0, 1700000002.0);
    const int after_3 = analytics_ptr->health_call_count;

    // Tick 4: analytics fires again
    controller.tick(1.0, 1700000003.0);
    const int after_4 = analytics_ptr->health_call_count;

    bool ok = true;
    ok &= expect_true(after_1 == 1, "tiered: health called on tick 1");
    ok &= expect_true(after_3 == 1, "tiered: health not called on tick 2-3");
    ok &= expect_true(after_4 == 2, "tiered: health called on tick 4");
    return ok;
}

// ---------------------------------------------------------------------------
// 11. Snapshot buffer ring capped at capacity
// ---------------------------------------------------------------------------
bool test_snapshot_buffer_ring_capacity() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    auto analytics = std::make_unique<FakeAnalyticsBridge>();
    auto* analytics_ptr = analytics.get();

    aura::shell::CockpitController::Config config;
    config.snapshot_buffer_capacity = 5;
    config.analytics_refresh_ticks = 1;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config,
        nullptr,
        std::move(analytics)
    );

    // Feed 20 ticks — buffer should cap at 5
    for (int i = 0; i < 20; ++i) {
        controller.tick(1.0, 1700000000.0 + i);
    }

    // After 10+ ticks, trends should start being called (buffer hits 10 samples
    // at tick 10, but our capacity is only 5 so we never reach 10)
    bool ok = true;
    ok &= expect_true(analytics_ptr->trend_call_count == 0, "ring-cap: no trend calls with capacity 5 < 10");
    return ok;
}

// ---------------------------------------------------------------------------
// 12. No analytics bridge → constructor and tick() work fine
// ---------------------------------------------------------------------------
bool test_no_analytics_bridge_works_fine() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        {},
        nullptr,
        nullptr  // no analytics bridge
    );
    const auto state = controller.tick(1.0, 1700000000.0);

    bool ok = true;
    ok &= expect_true(state.telemetry_available, "no-analytics: telemetry works");
    ok &= expect_true(!state.health.available, "no-analytics: health defaults");
    ok &= expect_true(state.cpu_trend == aura::shell::TrendDirection::Stable, "no-analytics: stable");
    ok &= expect_true(state.active_alerts.empty(), "no-analytics: no alerts");
    return ok;
}

int main() {
    int failures = 0;

    if (!test_health_score_flows_through_tick()) ++failures;
    if (!test_health_unavailable_preserves_defaults()) ++failures;
    if (!test_cpu_trend_rising_with_enough_samples()) ++failures;
    if (!test_trends_need_minimum_10_samples()) ++failures;
    if (!test_smoothing_toggle_affects_values()) ++failures;
    if (!test_smoothing_off_by_default()) ++failures;
    if (!test_alert_evaluation_populates_active_alerts()) ++failures;
    if (!test_alert_acknowledge()) ++failures;
    if (!test_analytics_graceful_degradation()) ++failures;
    if (!test_tiered_analytics_every_n_ticks()) ++failures;
    if (!test_snapshot_buffer_ring_capacity()) ++failures;
    if (!test_no_analytics_bridge_works_fine()) ++failures;

    if (failures == 0) {
        std::cout << "All analytics bridge tests passed." << '\n';
        return 0;
    }
    std::cerr << failures << " analytics bridge tests failed." << '\n';
    return 1;
}
