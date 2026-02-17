#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace aura::shell {

struct TelemetrySnapshot {
    double cpu_percent{0.0};
    double memory_percent{0.0};
};

struct ProcessSample {
    std::uint32_t pid{0};
    std::string name;
    double cpu_percent{0.0};
    std::uint64_t memory_rss_bytes{0};
};

struct SnapshotLines {
    std::string cpu;
    std::string memory;
    std::string timestamp;
};

struct FrameState {
    double phase{0.0};
    double accent_intensity{0.0};
    double next_delay_seconds{0.0};
};

struct RenderStyleTokens {
    double phase{0.0};
    double next_delay_seconds{1.0 / 60.0};
    double accent_intensity{0.0};
    double accent_red{0.20};
    double accent_green{0.45};
    double accent_blue{0.75};
    double accent_alpha{0.20};
    double frost_intensity{0.25};
    double tint_strength{0.35};
    double ring_line_width{2.0};
    double ring_glow_strength{0.25};
    double cpu_alpha{0.70};
    double memory_alpha{0.70};
    int severity_level{0};
    double motion_scale{1.0};
    int quality_hint{0};
    double timeline_anomaly_alpha{0.05};
};

struct TimelinePoint {
    double timestamp{0.0};
    double cpu_percent{0.0};
    double memory_percent{0.0};
    double gpu_percent{0.0};
};

enum class TimelineSource : std::uint8_t {
    None = 0,
    Live = 1,
    Dvr = 2,
};

struct PerCoreCpuState {
    std::vector<double> core_percents;
    std::uint32_t core_count{0};
};

struct GpuState {
    bool available{false};
    double gpu_percent{0.0};
    double vram_percent{0.0};
    std::uint64_t vram_used_bytes{0};
    std::uint64_t vram_total_bytes{0};
};

struct DiskIoState {
    double read_bytes_per_sec{0.0};
    double write_bytes_per_sec{0.0};
};

struct NetworkIoState {
    double recv_bytes_per_sec{0.0};
    double sent_bytes_per_sec{0.0};
};

struct ThermalSensorReading {
    std::string label;
    double current_celsius{0.0};
    double high_celsius{0.0};
    double critical_celsius{0.0};
    bool has_high{false};
    bool has_critical{false};
};

struct ThermalState {
    bool available{false};
    std::vector<ThermalSensorReading> sensors;
    double hottest_celsius{0.0};
};

enum class TrendDirection : int { Stable = 0, Rising = 1, Falling = 2 };

struct HealthScoreState {
    bool available{false};
    double overall{50.0};
    double cpu{50.0};
    double memory{50.0};
    double disk{50.0};
    double network{50.0};
};

struct ActiveAlert {
    int rule_id{0};
    int state{0};       // 0=idle, 1=pending, 2=triggered, 3=cooldown
    double last_value{0.0};
    double peak_value{0.0};
    double duration{0.0};
    bool acknowledged{false};
};

struct MetricStats {
    double avg{0.0};
    double min_val{0.0};
    double max_val{0.0};
    double p50{0.0};
    double p95{0.0};
    double p99{0.0};
    double stddev{0.0};
};

struct DvrStatsResult {
    int count{0};
    double duration_seconds{0.0};
    MetricStats cpu;
    MetricStats memory;
    MetricStats disk_read;
    MetricStats disk_write;
    MetricStats net_recv;
    MetricStats net_sent;
};

struct AnalyticsSnapshot {
    double timestamp{0.0};
    double cpu_percent{0.0};
    double memory_percent{0.0};
    double disk_read_bps{0.0};
    double disk_write_bps{0.0};
    double net_recv_bps{0.0};
    double net_sent_bps{0.0};
};

struct CockpitUiState {
    double timestamp{0.0};
    double cpu_percent{0.0};
    double memory_percent{0.0};
    double accent_intensity{0.0};
    int severity_level{0};
    double motion_scale{1.0};
    int quality_hint{0};
    int timeline_anomaly_count{0};
    int fps_target{60};
    int fps_recommended_delay_ms{16};
    bool telemetry_available{false};
    bool render_available{false};
    bool degraded{false};
    std::string cpu_line;
    std::string memory_line;
    std::string timestamp_line;
    std::vector<std::string> process_rows;
    std::vector<TimelinePoint> timeline_points;
    TimelineSource timeline_source{TimelineSource::None};
    std::string timeline_line;
    std::string status_line;
    RenderStyleTokens style_tokens;
    bool style_tokens_available{false};
    std::string style_token_error;

    PerCoreCpuState per_core_cpu;
    GpuState gpu;
    DiskIoState disk_io;
    NetworkIoState network_io;
    ThermalState thermal;

    HealthScoreState health;
    TrendDirection cpu_trend{TrendDirection::Stable};
    TrendDirection memory_trend{TrendDirection::Stable};
    bool smoothing_active{false};
    std::vector<ActiveAlert> active_alerts;
};

}  // namespace aura::shell
