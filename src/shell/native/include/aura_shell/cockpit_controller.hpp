#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aura_shell/cockpit_types.hpp"
#include "aura_shell/render_bridge.hpp"
#include "aura_shell/telemetry_bridge.hpp"
#include "aura_shell/timeline_bridge.hpp"

namespace aura::shell {

class CockpitController {
public:
    struct Config {
        double poll_interval_seconds{1.0};
        std::size_t max_process_rows{5};
        std::optional<std::string> db_path;
        std::size_t timeline_live_capacity{120};
        double timeline_window_seconds{300.0};
        int timeline_resolution{64};
        std::size_t timeline_refresh_ticks{5};
        bool prefer_dvr_timeline{true};
    };

    CockpitController(
        std::unique_ptr<ITelemetryBridge> telemetry_bridge,
        std::unique_ptr<IRenderBridge> render_bridge,
        std::unique_ptr<ITimelineBridge> timeline_bridge,
        Config config = {}
    );

    CockpitUiState tick(
        double elapsed_since_last_frame,
        std::optional<double> timestamp_override = std::nullopt
    );

    const CockpitUiState& last_state() const;

private:
    static double now_seconds();
    static double clamp_percent(double value);
    static SnapshotLines fallback_snapshot_lines(double timestamp, double cpu_percent, double memory_percent);
    static std::string fallback_process_row(
        int rank,
        const ProcessSample& process,
        std::size_t max_chars
    );
    static std::string timeline_source_to_string(TimelineSource source);
    static std::string fallback_timeline_line(
        TimelineSource source,
        std::size_t point_count,
        int anomaly_count,
        double cpu_percent,
        double memory_percent
    );
    void append_live_timeline_point(double timestamp, double cpu_percent, double memory_percent);
    std::vector<TimelinePoint> copy_live_timeline_window(double now_timestamp) const;
    void populate_timeline_state(CockpitUiState& state, std::optional<std::string>& stream_error);
    std::string fallback_status_line(const std::optional<std::string>& error) const;
    CockpitUiState degraded_from_last_state(double timestamp, const std::string& reason) const;

    std::unique_ptr<ITelemetryBridge> telemetry_bridge_;
    std::unique_ptr<IRenderBridge> render_bridge_;
    std::unique_ptr<ITimelineBridge> timeline_bridge_;
    Config config_;
    double frame_phase_{0.0};
    std::size_t ticks_since_timeline_query_{0U};
    bool has_dvr_timeline_cache_{false};
    std::vector<TimelinePoint> live_timeline_points_;
    std::vector<TimelinePoint> dvr_timeline_cache_;
    bool has_last_good_state_{false};
    CockpitUiState last_state_{};
};

}  // namespace aura::shell
