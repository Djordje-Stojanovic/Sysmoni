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

struct CockpitUiState {
    double timestamp{0.0};
    double cpu_percent{0.0};
    double memory_percent{0.0};
    double accent_intensity{0.0};
    bool telemetry_available{false};
    bool render_available{false};
    bool degraded{false};
    std::string cpu_line;
    std::string memory_line;
    std::string timestamp_line;
    std::vector<std::string> process_rows;
    std::string status_line;
};

}  // namespace aura::shell

