#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
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
};

enum class TimelineSource : std::uint8_t {
    None = 0,
    Live = 1,
    Dvr = 2,
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
};

}  // namespace aura::shell
