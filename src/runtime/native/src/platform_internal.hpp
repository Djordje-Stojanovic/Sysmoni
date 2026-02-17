#pragma once

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aura::platform {

constexpr double kDefaultRetentionSeconds = 24.0 * 60.0 * 60.0;

struct Snapshot {
    double timestamp = 0.0;
    double cpu_percent = 0.0;
    double memory_percent = 0.0;
    double disk_read_bps = 0.0;
    double disk_write_bps = 0.0;
    double net_recv_bps = 0.0;
    double net_sent_bps = 0.0;
};

enum class DbSource {
    Cli = 0,
    Env = 1,
    Config = 2,
    Auto = 3,
    Disabled = 4,
};

struct RuntimeConfig {
    bool persistence_enabled = true;
    double retention_seconds = kDefaultRetentionSeconds;
    DbSource db_source = DbSource::Auto;
    std::string db_path;
};

struct ConfigRequest {
    std::optional<std::string> cli_db_path;
    bool no_persist = false;
    std::optional<double> cli_retention_seconds;
    std::optional<std::string> config_path_override;
};

struct Error {
    int code = 0;
    std::string message;
};

class TelemetryStore {
  public:
    virtual ~TelemetryStore() = default;

    virtual void Append(const Snapshot& snapshot) = 0;
    virtual int Count() = 0;
    virtual std::vector<Snapshot> Latest(int limit) = 0;
    virtual std::vector<Snapshot> Between(
        const std::optional<double>& start_timestamp,
        const std::optional<double>& end_timestamp
    ) = 0;
};

RuntimeConfig ResolveRuntimeConfig(const ConfigRequest& request);
std::unique_ptr<TelemetryStore> OpenStore(const std::string& db_path, double retention_seconds);

std::vector<Snapshot> DownsampleLttb(const std::vector<Snapshot>& snapshots, int target);
std::vector<Snapshot> QueryTimeline(
    TelemetryStore& store,
    const std::optional<double>& start,
    const std::optional<double>& end,
    int resolution
);

Snapshot CollectSystemSnapshot();
double NowUnixSeconds();

void ValidateSnapshot(const Snapshot& snapshot);
void ValidatePositiveFinite(double value, const char* field_name);
void ValidateFinite(double value, const char* field_name);

// ---------------------------------------------------------------------------
// DVR Statistics & Export Engine
// ---------------------------------------------------------------------------

struct MetricStats {
    double avg = 0.0;
    double min = 0.0;
    double max = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
};

struct StatsResult {
    int count = 0;
    double start_timestamp = 0.0;
    double end_timestamp = 0.0;
    double duration_seconds = 0.0;
    MetricStats cpu;
    MetricStats memory;
    MetricStats disk_read;
    MetricStats disk_write;
    MetricStats net_recv;
    MetricStats net_sent;
};

StatsResult ComputeStats(const std::vector<Snapshot>& snapshots);

void ExportToJsonFile(
    const std::vector<Snapshot>& snapshots,
    const StatsResult* stats,
    const std::string& file_path
);

void ExportToCsvFile(
    const std::vector<Snapshot>& snapshots,
    const std::string& file_path
);

} // namespace aura::platform
