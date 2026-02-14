#include "aura_shell/cockpit_controller.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

class FakeTelemetryBridge final : public aura::shell::ITelemetryBridge {
public:
    bool backend_available = true;
    std::optional<aura::shell::TelemetrySnapshot> next_snapshot{
        aura::shell::TelemetrySnapshot{35.0, 48.0}
    };
    std::vector<aura::shell::ProcessSample> next_processes{
        aura::shell::ProcessSample{1234U, "aura", 22.1, 32ULL * 1024ULL * 1024ULL},
        aura::shell::ProcessSample{2048U, "explorer", 7.5, 110ULL * 1024ULL * 1024ULL},
    };
    std::string snapshot_error;
    std::string process_error;

    bool available() const override {
        return backend_available;
    }

    std::optional<aura::shell::TelemetrySnapshot> collect_snapshot(std::string& error) override {
        if (!backend_available || !next_snapshot.has_value()) {
            error = snapshot_error.empty() ? "telemetry unavailable" : snapshot_error;
            return std::nullopt;
        }
        error.clear();
        return next_snapshot;
    }

    std::vector<aura::shell::ProcessSample> collect_top_processes(
        const std::size_t max_samples,
        std::string& error
    ) override {
        if (!backend_available) {
            error = process_error.empty() ? "telemetry unavailable" : process_error;
            return {};
        }
        error = process_error;
        std::vector<aura::shell::ProcessSample> output = next_processes;
        if (output.size() > max_samples) {
            output.resize(max_samples);
        }
        return output;
    }
};

class FakeRenderBridge final : public aura::shell::IRenderBridge {
public:
    bool backend_available = true;
    bool fail_compose = false;
    bool fail_lines = false;
    bool fail_rows = false;
    bool fail_status = false;

    bool available() const override {
        return backend_available;
    }

    double sanitize_percent(const double value) const override {
        if (!std::isfinite(value)) {
            return 0.0;
        }
        return std::clamp(value, 0.0, 100.0);
    }

    std::optional<aura::shell::FrameState> compose_frame(
        const double previous_phase,
        const double elapsed_since_last_frame,
        const double cpu_percent,
        const double memory_percent,
        std::string& error
    ) const override {
        if (!backend_available || fail_compose) {
            error = "compose failed";
            return std::nullopt;
        }
        error.clear();
        aura::shell::FrameState frame;
        frame.phase = std::fmod(previous_phase + std::max(0.0, elapsed_since_last_frame), 1.0);
        frame.accent_intensity = std::clamp((cpu_percent + memory_percent) / 200.0, 0.0, 1.0);
        frame.next_delay_seconds = 1.0 / 60.0;
        return frame;
    }

    std::optional<aura::shell::SnapshotLines> format_snapshot_lines(
        const double timestamp,
        const double cpu_percent,
        const double memory_percent,
        std::string& error
    ) const override {
        if (!backend_available || fail_lines) {
            error = "snapshot formatting failed";
            return std::nullopt;
        }
        error.clear();
        aura::shell::SnapshotLines lines;
        lines.cpu = "cpu_line_" + std::to_string(static_cast<int>(cpu_percent));
        lines.memory = "mem_line_" + std::to_string(static_cast<int>(memory_percent));
        lines.timestamp = "ts_line_" + std::to_string(static_cast<int>(timestamp));
        return lines;
    }

    std::optional<std::string> format_process_row(
        const int rank,
        const std::string& name,
        const double cpu_percent,
        const double memory_rss_bytes,
        const int /*max_chars*/,
        std::string& error
    ) const override {
        if (!backend_available || fail_rows) {
            error = "row formatting failed";
            return std::nullopt;
        }
        error.clear();
        return "#" + std::to_string(rank) + " " + name + " cpu=" + std::to_string(static_cast<int>(cpu_percent)) +
               " mem=" + std::to_string(static_cast<int>(memory_rss_bytes));
    }

    std::optional<std::string> format_stream_status(
        const std::optional<std::string>& db_path,
        const std::optional<int>& /*sample_count*/,
        const std::optional<std::string>& stream_error,
        std::string& error
    ) const override {
        if (!backend_available || fail_status) {
            error = "status formatting failed";
            return std::nullopt;
        }
        error.clear();
        std::string value = "db=" + (db_path.has_value() ? *db_path : std::string("<none>")) + " render=ok";
        if (stream_error.has_value() && !stream_error->empty()) {
            value += " warning=" + *stream_error;
        }
        return value;
    }
};

class FakeTimelineBridge final : public aura::shell::ITimelineBridge {
public:
    bool backend_available = true;
    bool fail_query = false;
    std::string query_error;
    std::vector<aura::shell::TimelinePoint> next_points{
        {1699999950.0, 20.0, 35.0},
        {1699999955.0, 21.0, 35.5},
        {1699999960.0, 22.0, 36.0},
        {1699999965.0, 23.0, 36.5},
        {1699999970.0, 24.0, 37.0},
        {1699999975.0, 25.0, 37.5},
        {1699999980.0, 26.0, 38.0},
        {1699999985.0, 27.0, 38.5},
        {1699999990.0, 28.0, 39.0},
        {1699999995.0, 29.0, 39.5},
    };
    int query_count = 0;

    bool available() const override {
        return backend_available;
    }

    std::vector<aura::shell::TimelinePoint> query_recent(
        const std::string& db_path,
        const double /*end_timestamp*/,
        const double /*window_seconds*/,
        const int resolution,
        std::string& error
    ) override {
        ++query_count;
        if (!backend_available || fail_query) {
            error = query_error.empty() ? "timeline unavailable" : query_error;
            return {};
        }
        if (db_path.empty()) {
            error = "db_path empty";
            return {};
        }
        error.clear();
        std::vector<aura::shell::TimelinePoint> output = next_points;
        if (resolution > 0 && output.size() > static_cast<std::size_t>(resolution)) {
            output.resize(static_cast<std::size_t>(resolution));
        }
        return output;
    }
};

bool expect_true(const bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}

bool contains(const std::string& text, const std::string& pattern) {
    return text.find(pattern) != std::string::npos;
}

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
    ok &= expect_true(contains(state.status_line, "render=fallback"), "render-missing: fallback status");
    ok &= expect_true(!state.process_rows.empty(), "render-missing: process rows");
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

bool test_falls_back_to_live_when_dvr_unavailable() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();
    timeline->backend_available = false;

    aura::shell::CockpitController::Config config;
    config.db_path = "C:/tmp/aura.db";

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );
    static_cast<void>(controller.tick(1.0, 1700000010.0));
    const aura::shell::CockpitUiState state = controller.tick(1.0, 1700000011.0);

    bool ok = true;
    ok &= expect_true(state.timeline_source == aura::shell::TimelineSource::Live, "fallback-live: source");
    ok &= expect_true(state.timeline_points.size() >= 2U, "fallback-live: points");
    ok &= expect_true(contains(state.timeline_line, "timeline=live"), "fallback-live: line");
    return ok;
}

bool test_live_ring_respects_capacity() {
    auto telemetry = std::make_unique<FakeTelemetryBridge>();
    auto* telemetry_ptr = telemetry.get();
    auto render = std::make_unique<FakeRenderBridge>();
    auto timeline = std::make_unique<FakeTimelineBridge>();

    aura::shell::CockpitController::Config config;
    config.prefer_dvr_timeline = false;
    config.timeline_live_capacity = 3U;
    config.timeline_window_seconds = 1000.0;

    aura::shell::CockpitController controller(
        std::move(telemetry),
        std::move(render),
        std::move(timeline),
        config
    );

    aura::shell::CockpitUiState state;
    for (int i = 0; i < 5; ++i) {
        telemetry_ptr->next_snapshot = aura::shell::TelemetrySnapshot{
            10.0 + static_cast<double>(i),
            20.0 + static_cast<double>(i),
        };
        state = controller.tick(1.0, 1700000100.0 + static_cast<double>(i));
    }

    bool ok = true;
    ok &= expect_true(state.timeline_source == aura::shell::TimelineSource::Live, "capacity: source");
    ok &= expect_true(state.timeline_points.size() == 3U, "capacity: size");
    ok &= expect_true(std::fabs(state.timeline_points.front().timestamp - 1700000102.0) < 0.0001, "capacity: front timestamp");
    return ok;
}

}  // namespace

int main() {
    int failures = 0;

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
    if (!test_last_good_reused_on_telemetry_failure_preserves_timeline()) {
        ++failures;
    }
    if (!test_falls_back_to_live_when_dvr_unavailable()) {
        ++failures;
    }
    if (!test_live_ring_respects_capacity()) {
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All cockpit controller tests passed." << '\n';
        return 0;
    }
    std::cerr << failures << " cockpit controller tests failed." << '\n';
    return 1;
}

