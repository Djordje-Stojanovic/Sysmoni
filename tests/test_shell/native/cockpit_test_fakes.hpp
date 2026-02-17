#pragma once

#include "aura_shell/cockpit_controller.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Fake bridge implementations for cockpit controller tests
// ---------------------------------------------------------------------------

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

    // -- Extended sensor fakes -----------------------------------------------
    std::optional<aura::shell::PerCoreCpuState> next_per_core;
    std::optional<aura::shell::GpuState> next_gpu;
    std::optional<aura::shell::DiskIoState> next_disk;
    std::optional<aura::shell::NetworkIoState> next_network;
    std::optional<aura::shell::ThermalState> next_thermal;

    std::optional<aura::shell::PerCoreCpuState> collect_per_core_cpu(std::string& error) override {
        error.clear();
        if (!backend_available) { error = "unavailable"; return std::nullopt; }
        return next_per_core;
    }
    std::optional<aura::shell::GpuState> collect_gpu(std::string& error) override {
        error.clear();
        if (!backend_available) { error = "unavailable"; return std::nullopt; }
        return next_gpu;
    }
    std::optional<aura::shell::DiskIoState> collect_disk_io(std::string& error) override {
        error.clear();
        if (!backend_available) { error = "unavailable"; return std::nullopt; }
        return next_disk;
    }
    std::optional<aura::shell::NetworkIoState> collect_network_io(std::string& error) override {
        error.clear();
        if (!backend_available) { error = "unavailable"; return std::nullopt; }
        return next_network;
    }
    std::optional<aura::shell::ThermalState> collect_thermal(std::string& error) override {
        error.clear();
        if (!backend_available) { error = "unavailable"; return std::nullopt; }
        return next_thermal;
    }
    std::vector<aura::shell::ProcessSample> collect_process_details(
        std::size_t max_results, std::uint8_t /*sort_column*/,
        bool /*sort_descending*/, std::string& error) override {
        return collect_top_processes(max_results, error);
    }
    bool terminate_process(std::uint32_t /*pid*/, std::string& error) override {
        error = "not implemented in fake";
        return false;
    }
};

class FakeRenderBridge final : public aura::shell::IRenderBridge {
public:
    bool backend_available = true;
    bool fail_compose = false;
    bool fail_style_tokens = false;
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

    std::optional<aura::shell::RenderStyleTokens> compute_style_tokens(
        const double previous_phase,
        const double elapsed_since_last_frame,
        const double cpu_percent,
        const double memory_percent,
        std::string& error
    ) const override {
        if (!backend_available || fail_style_tokens) {
            error = "style tokens failed";
            return std::nullopt;
        }
        error.clear();
        aura::shell::RenderStyleTokens tokens;
        tokens.phase = std::fmod(previous_phase + std::max(0.0, elapsed_since_last_frame), 1.0);
        tokens.next_delay_seconds = 1.0 / 60.0;
        tokens.accent_intensity = std::clamp((cpu_percent + memory_percent) / 200.0, 0.0, 1.0);
        tokens.accent_red = std::clamp(0.2 + tokens.accent_intensity * 0.4, 0.0, 1.0);
        tokens.accent_green = std::clamp(0.4 + tokens.accent_intensity * 0.3, 0.0, 1.0);
        tokens.accent_blue = std::clamp(0.7 + tokens.accent_intensity * 0.2, 0.0, 1.0);
        tokens.accent_alpha = std::clamp(0.2 + tokens.accent_intensity * 0.4, 0.0, 1.0);
        tokens.frost_intensity = std::clamp(0.2 + tokens.accent_intensity * 0.7, 0.0, 1.0);
        tokens.tint_strength = std::clamp(0.3 + tokens.accent_intensity * 0.5, 0.0, 1.0);
        tokens.ring_line_width = std::clamp(1.0 + tokens.accent_intensity * 6.0, 1.0, 7.0);
        tokens.ring_glow_strength = tokens.accent_intensity;
        tokens.cpu_alpha = std::clamp(0.3 + sanitize_percent(cpu_percent) / 100.0 * 0.7, 0.0, 1.0);
        tokens.memory_alpha = std::clamp(0.3 + sanitize_percent(memory_percent) / 100.0 * 0.7, 0.0, 1.0);
        tokens.severity_level = tokens.accent_intensity > 0.8 ? 3 : (tokens.accent_intensity > 0.6 ? 2 : 1);
        tokens.motion_scale = tokens.severity_level >= 2 ? 0.75 : 0.95;
        tokens.quality_hint = tokens.severity_level >= 2 ? 1 : 0;
        tokens.timeline_anomaly_alpha = std::clamp(tokens.accent_intensity, 0.05, 1.0);
        return tokens;
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

    std::string last_error_text() const override {
        if (!backend_available) {
            return "render unavailable";
        }
        if (fail_style_tokens) {
            return "style tokens failed";
        }
        return {};
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

class FakePersistenceBridge final : public aura::shell::IPersistenceBridge {
public:
    bool backend_available = false;
    bool store_opened = false;
    void* fake_store_handle = nullptr;

    bool available() const override { return backend_available; }

    bool open_store(const std::string& /*db_path*/, double /*retention_seconds*/, std::string& error) override {
        if (!backend_available) { error = "unavailable"; return false; }
        store_opened = true;
        return true;
    }

    bool append_snapshot(double /*ts*/, double /*cpu*/, double /*mem*/, double /*disk_r*/, double /*disk_w*/, std::string& error) override {
        if (!backend_available) { error = "unavailable"; return false; }
        return true;
    }

    void close_store() override { store_opened = false; }

    void* store_handle() const override { return fake_store_handle; }
};

class FakeAnalyticsBridge final : public aura::shell::IAnalyticsBridge {
public:
    bool backend_available = true;
    bool health_enabled = true;
    bool trend_enabled = true;
    bool smoother_enabled = false;
    bool alerts_enabled = false;
    bool dvr_stats_enabled = false;

    aura::shell::HealthScoreState next_health{true, 75.0, 80.0, 70.0, 85.0, 90.0};
    aura::shell::TrendResult next_cpu_trend{aura::shell::TrendDirection::Rising, 0.5, 0.8};
    aura::shell::TrendResult next_mem_trend{aura::shell::TrendDirection::Stable, 0.0, 0.1};
    aura::shell::AnalyticsSnapshot next_smoothed{};
    std::vector<aura::shell::ActiveAlert> next_alerts;
    aura::shell::DvrStatsResult next_dvr_stats;

    int health_call_count = 0;
    int trend_call_count = 0;
    int smooth_call_count = 0;
    int evaluate_call_count = 0;
    int acknowledge_call_count = 0;
    int last_acknowledged_rule_id = -1;

    bool available() const override { return backend_available; }
    bool smoother_available() const override { return backend_available && smoother_enabled; }
    bool alerts_available() const override { return backend_available && alerts_enabled; }
    bool health_available() const override { return backend_available && health_enabled; }
    bool trend_available() const override { return backend_available && trend_enabled; }
    bool dvr_stats_available() const override { return backend_available && dvr_stats_enabled; }

    std::optional<aura::shell::HealthScoreState> compute_health_score(
        const aura::shell::AnalyticsSnapshot& /*snapshot*/, std::string& error) override {
        ++health_call_count;
        if (!health_available()) { error = "health unavailable"; return std::nullopt; }
        return next_health;
    }

    std::optional<aura::shell::TrendResult> detect_trend(
        const std::vector<aura::shell::AnalyticsSnapshot>& /*snapshots*/,
        int metric, double /*sensitivity*/, std::string& error) override {
        ++trend_call_count;
        if (!trend_available()) { error = "trend unavailable"; return std::nullopt; }
        if (metric == 0) return next_cpu_trend;
        if (metric == 1) return next_mem_trend;
        error = "unknown metric";
        return std::nullopt;
    }

    std::optional<aura::shell::AnalyticsSnapshot> smooth(
        const aura::shell::AnalyticsSnapshot& snapshot, std::string& error) override {
        ++smooth_call_count;
        if (!smoother_available()) { error = "smoother unavailable"; return std::nullopt; }
        next_smoothed = snapshot;
        next_smoothed.cpu_percent = snapshot.cpu_percent * 0.5 + 10.0;
        next_smoothed.memory_percent = snapshot.memory_percent * 0.5 + 10.0;
        return next_smoothed;
    }

    bool reset_smoother(std::string& error) override {
        if (!smoother_available()) { error = "smoother unavailable"; return false; }
        return true;
    }

    bool evaluate_alerts(const aura::shell::AnalyticsSnapshot& /*snapshot*/, std::string& error) override {
        ++evaluate_call_count;
        if (!alerts_available()) { error = "alerts unavailable"; return false; }
        return true;
    }

    std::vector<aura::shell::ActiveAlert> get_active_alerts(std::string& error) override {
        if (!alerts_available()) { error = "alerts unavailable"; return {}; }
        return next_alerts;
    }

    bool acknowledge_alert(int rule_id, std::string& error) override {
        ++acknowledge_call_count;
        if (!alerts_available()) { error = "alerts unavailable"; return false; }
        last_acknowledged_rule_id = rule_id;
        return true;
    }

    std::optional<aura::shell::DvrStatsResult> compute_dvr_stats(
        void* /*store_handle*/, double /*start*/, double /*end*/, std::string& error) override {
        if (!dvr_stats_available()) { error = "dvr stats unavailable"; return std::nullopt; }
        return next_dvr_stats;
    }

    bool export_json(void* /*store_handle*/, double /*start*/, double /*end*/,
        bool /*include_stats*/, const std::string& /*path*/, std::string& error) override {
        if (!available()) { error = "unavailable"; return false; }
        return true;
    }

    bool export_csv(void* /*store_handle*/, double /*start*/, double /*end*/,
        const std::string& /*path*/, std::string& error) override {
        if (!available()) { error = "unavailable"; return false; }
        return true;
    }
};

// ---------------------------------------------------------------------------
// Assertion helpers
// ---------------------------------------------------------------------------

inline bool expect_true(const bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}

inline bool contains(const std::string& text, const std::string& pattern) {
    return text.find(pattern) != std::string::npos;
}

inline bool style_tokens_valid(const aura::shell::RenderStyleTokens& tokens) {
    return std::isfinite(tokens.phase) &&
           std::isfinite(tokens.next_delay_seconds) &&
           std::isfinite(tokens.accent_intensity) &&
           std::isfinite(tokens.accent_red) &&
           std::isfinite(tokens.accent_green) &&
           std::isfinite(tokens.accent_blue) &&
           std::isfinite(tokens.accent_alpha) &&
           std::isfinite(tokens.frost_intensity) &&
           std::isfinite(tokens.tint_strength) &&
           std::isfinite(tokens.ring_line_width) &&
           std::isfinite(tokens.ring_glow_strength) &&
           std::isfinite(tokens.cpu_alpha) &&
           std::isfinite(tokens.memory_alpha) &&
           std::isfinite(tokens.motion_scale) &&
           std::isfinite(tokens.timeline_anomaly_alpha) &&
           tokens.phase >= 0.0 &&
           tokens.phase < 1.0 &&
           tokens.next_delay_seconds >= 0.0 &&
           tokens.accent_intensity >= 0.0 &&
           tokens.accent_intensity <= 1.0 &&
           tokens.accent_red >= 0.0 &&
           tokens.accent_red <= 1.0 &&
           tokens.accent_green >= 0.0 &&
           tokens.accent_green <= 1.0 &&
           tokens.accent_blue >= 0.0 &&
           tokens.accent_blue <= 1.0 &&
           tokens.accent_alpha >= 0.0 &&
           tokens.accent_alpha <= 1.0 &&
           tokens.frost_intensity >= 0.0 &&
           tokens.frost_intensity <= 1.0 &&
           tokens.tint_strength >= 0.0 &&
           tokens.tint_strength <= 1.0 &&
           tokens.ring_line_width >= 1.0 &&
           tokens.ring_line_width <= 7.0 &&
           tokens.ring_glow_strength >= 0.0 &&
           tokens.ring_glow_strength <= 1.0 &&
           tokens.cpu_alpha >= 0.0 &&
           tokens.cpu_alpha <= 1.0 &&
           tokens.memory_alpha >= 0.0 &&
           tokens.memory_alpha <= 1.0 &&
           tokens.severity_level >= 0 &&
           tokens.severity_level <= 3 &&
           tokens.motion_scale >= 0.60 &&
           tokens.motion_scale <= 1.0 &&
           (tokens.quality_hint == 0 || tokens.quality_hint == 1) &&
           tokens.timeline_anomaly_alpha >= 0.0 &&
           tokens.timeline_anomaly_alpha <= 1.0;
}
