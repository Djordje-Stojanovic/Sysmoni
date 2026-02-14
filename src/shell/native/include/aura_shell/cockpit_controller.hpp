#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "aura_shell/cockpit_types.hpp"
#include "aura_shell/render_bridge.hpp"
#include "aura_shell/telemetry_bridge.hpp"

namespace aura::shell {

class CockpitController {
public:
    struct Config {
        double poll_interval_seconds{1.0};
        std::size_t max_process_rows{5};
        std::optional<std::string> db_path;
    };

    CockpitController(
        std::unique_ptr<ITelemetryBridge> telemetry_bridge,
        std::unique_ptr<IRenderBridge> render_bridge,
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
    std::string fallback_status_line(const std::optional<std::string>& error) const;
    CockpitUiState degraded_from_last_state(double timestamp, const std::string& reason) const;

    std::unique_ptr<ITelemetryBridge> telemetry_bridge_;
    std::unique_ptr<IRenderBridge> render_bridge_;
    Config config_;
    double frame_phase_{0.0};
    bool has_last_good_state_{false};
    CockpitUiState last_state_{};
};

}  // namespace aura::shell

