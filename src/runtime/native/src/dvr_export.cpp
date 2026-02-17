#include "platform_internal.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace aura::platform {

namespace {

std::string DoubleToString(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

void WriteMetricStatsJson(std::ostream& out, const std::string& name, const MetricStats& stats) {
    out << "\"" << name << "\":{"
        << "\"avg\":" << DoubleToString(stats.avg, 6)
        << ",\"min\":" << DoubleToString(stats.min, 6)
        << ",\"max\":" << DoubleToString(stats.max, 6)
        << ",\"p50\":" << DoubleToString(stats.p50, 6)
        << ",\"p95\":" << DoubleToString(stats.p95, 6)
        << ",\"p99\":" << DoubleToString(stats.p99, 6)
        << ",\"stddev\":" << DoubleToString(stats.stddev, 6)
        << "}";
}

void EnsureParentDirectory(const std::string& path) {
    const std::filesystem::path fs_path(path);
    const std::filesystem::path parent = fs_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

} // namespace

void ExportToJsonFile(
    const std::vector<Snapshot>& snapshots,
    const StatsResult* stats,
    const std::string& file_path
) {
    if (file_path.empty()) {
        throw std::runtime_error("file_path cannot be empty.");
    }

    EnsureParentDirectory(file_path);

    std::ofstream out(file_path, std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + file_path);
    }

    out << std::fixed;

    const int count = static_cast<int>(snapshots.size());

    // Header
    out << "{\"version\":1,\"format\":\"aura_dvr_export\",\"exported_at\":"
        << DoubleToString(NowUnixSeconds(), 6);

    out << ",\"count\":" << count;

    // Time range
    if (!snapshots.empty()) {
        double start_ts = snapshots.front().timestamp;
        double end_ts = snapshots.back().timestamp;
        for (const auto& s : snapshots) {
            if (s.timestamp < start_ts) start_ts = s.timestamp;
            if (s.timestamp > end_ts) end_ts = s.timestamp;
        }
        out << ",\"time_range\":{\"start\":" << DoubleToString(start_ts, 6)
            << ",\"end\":" << DoubleToString(end_ts, 6)
            << ",\"duration_seconds\":" << DoubleToString(end_ts - start_ts, 6)
            << "}";
    } else {
        out << ",\"time_range\":{\"start\":0.000000,\"end\":0.000000,\"duration_seconds\":0.000000}";
    }

    // Stats section (optional)
    if (stats != nullptr) {
        out << ",\"stats\":{";
        WriteMetricStatsJson(out, "cpu", stats->cpu);
        out << ",";
        WriteMetricStatsJson(out, "memory", stats->memory);
        out << ",";
        WriteMetricStatsJson(out, "disk_read", stats->disk_read);
        out << ",";
        WriteMetricStatsJson(out, "disk_write", stats->disk_write);
        out << ",";
        WriteMetricStatsJson(out, "net_recv", stats->net_recv);
        out << ",";
        WriteMetricStatsJson(out, "net_sent", stats->net_sent);
        out << "}";
    }

    // Snapshots array
    out << ",\"snapshots\":[";
    for (int i = 0; i < count; ++i) {
        const Snapshot& s = snapshots[static_cast<std::size_t>(i)];
        if (i > 0) out << ",";
        out << "{\"ts\":" << DoubleToString(s.timestamp, 6)
            << ",\"cpu\":" << DoubleToString(s.cpu_percent, 6)
            << ",\"mem\":" << DoubleToString(s.memory_percent, 6)
            << ",\"disk_r\":" << DoubleToString(s.disk_read_bps, 6)
            << ",\"disk_w\":" << DoubleToString(s.disk_write_bps, 6)
            << ",\"net_r\":" << DoubleToString(s.net_recv_bps, 6)
            << ",\"net_s\":" << DoubleToString(s.net_sent_bps, 6)
            << "}";
    }
    out << "]}";

    out.flush();
    if (out.fail()) {
        throw std::runtime_error("Failed to write JSON export file: " + file_path);
    }
}

void ExportToCsvFile(
    const std::vector<Snapshot>& snapshots,
    const std::string& file_path
) {
    if (file_path.empty()) {
        throw std::runtime_error("file_path cannot be empty.");
    }

    EnsureParentDirectory(file_path);

    std::ofstream out(file_path, std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + file_path);
    }

    // Header row
    out << "timestamp,cpu_percent,memory_percent,disk_read_bps,disk_write_bps,net_recv_bps,net_sent_bps\n";

    // Data rows
    for (const auto& s : snapshots) {
        out << DoubleToString(s.timestamp, 6) << ","
            << DoubleToString(s.cpu_percent, 6) << ","
            << DoubleToString(s.memory_percent, 6) << ","
            << DoubleToString(s.disk_read_bps, 6) << ","
            << DoubleToString(s.disk_write_bps, 6) << ","
            << DoubleToString(s.net_recv_bps, 6) << ","
            << DoubleToString(s.net_sent_bps, 6) << "\n";
    }

    out.flush();
    if (out.fail()) {
        throw std::runtime_error("Failed to write CSV export file: " + file_path);
    }
}

} // namespace aura::platform
