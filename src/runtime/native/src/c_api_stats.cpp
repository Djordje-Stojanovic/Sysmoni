#include "aura_platform.h"
#include "c_api_helpers.hpp"
#include "platform_internal.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <vector>

using aura::platform::MetricStats;
using aura::platform::Snapshot;
using aura::platform::StatsResult;
using aura::platform::TelemetryStore;

namespace {

struct AuraStore {
    explicit AuraStore(std::unique_ptr<TelemetryStore> store_in)
        : store(std::move(store_in)) {}

    std::unique_ptr<TelemetryStore> store;
};

void CopyMetricStats(const MetricStats& src, aura_metric_stats_t& dst) {
    dst.avg = src.avg;
    dst.min = src.min;
    dst.max = src.max;
    dst.p50 = src.p50;
    dst.p95 = src.p95;
    dst.p99 = src.p99;
    dst.stddev = src.stddev;
}

void CopyStatsResult(const StatsResult& src, aura_stats_result_t& dst) {
    dst.count = src.count;
    dst.start_timestamp = src.start_timestamp;
    dst.end_timestamp = src.end_timestamp;
    dst.duration_seconds = src.duration_seconds;
    CopyMetricStats(src.cpu, dst.cpu);
    CopyMetricStats(src.memory, dst.memory);
    CopyMetricStats(src.disk_read, dst.disk_read);
    CopyMetricStats(src.disk_write, dst.disk_write);
    CopyMetricStats(src.net_recv, dst.net_recv);
    CopyMetricStats(src.net_sent, dst.net_sent);
}

} // namespace

extern "C" {

AURA_PLATFORM_EXPORT int aura_stats_compute(
    const aura_snapshot_t* snapshots,
    const int count,
    aura_stats_result_t* out_stats,
    aura_error_t* out_error
) {
    if (count <= 0) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "count must be greater than 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (snapshots == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "snapshots must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (out_stats == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_stats must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        std::vector<Snapshot> internal;
        internal.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            internal.push_back(ToInternalSnapshot(snapshots[i]));
        }

        const StatsResult result = aura::platform::ComputeStats(internal);
        CopyStatsResult(result, *out_stats);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_dvr_compute_stats(
    aura_store_t* store,
    const int has_start,
    const double start_timestamp,
    const int has_end,
    const double end_timestamp,
    aura_stats_result_t* out_stats,
    aura_error_t* out_error
) {
    if (store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (out_stats == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_stats must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        const std::optional<double> start = has_start != 0
            ? std::optional<double>(start_timestamp) : std::nullopt;
        const std::optional<double> end = has_end != 0
            ? std::optional<double>(end_timestamp) : std::nullopt;

        const std::vector<Snapshot> snapshots = typed_store->store->Between(start, end);
        if (snapshots.empty()) {
            SetError(out_error, AURA_ERR_RUNTIME, "No snapshots found in the specified time range.");
            return AURA_ERR_RUNTIME;
        }

        const StatsResult result = aura::platform::ComputeStats(snapshots);
        CopyStatsResult(result, *out_stats);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_dvr_export_json(
    aura_store_t* store,
    const int has_start,
    const double start_timestamp,
    const int has_end,
    const double end_timestamp,
    const int include_stats,
    const char* file_path,
    aura_error_t* out_error
) {
    if (store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (file_path == nullptr || file_path[0] == '\0') {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "file_path must not be null or empty.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        const std::optional<double> start = has_start != 0
            ? std::optional<double>(start_timestamp) : std::nullopt;
        const std::optional<double> end = has_end != 0
            ? std::optional<double>(end_timestamp) : std::nullopt;

        const std::vector<Snapshot> snapshots = typed_store->store->Between(start, end);

        const aura::platform::StatsResult* stats_ptr = nullptr;
        aura::platform::StatsResult stats;
        if (include_stats != 0 && !snapshots.empty()) {
            stats = aura::platform::ComputeStats(snapshots);
            stats_ptr = &stats;
        }

        aura::platform::ExportToJsonFile(snapshots, stats_ptr, std::string(file_path));
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_dvr_export_csv(
    aura_store_t* store,
    const int has_start,
    const double start_timestamp,
    const int has_end,
    const double end_timestamp,
    const char* file_path,
    aura_error_t* out_error
) {
    if (store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (file_path == nullptr || file_path[0] == '\0') {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "file_path must not be null or empty.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        const std::optional<double> start = has_start != 0
            ? std::optional<double>(start_timestamp) : std::nullopt;
        const std::optional<double> end = has_end != 0
            ? std::optional<double>(end_timestamp) : std::nullopt;

        const std::vector<Snapshot> snapshots = typed_store->store->Between(start, end);
        aura::platform::ExportToCsvFile(snapshots, std::string(file_path));
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

} // extern "C"
