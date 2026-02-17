#include "platform_internal.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace aura::platform {

namespace {

constexpr int kNumFields = 6;

inline double GetField(const Snapshot& s, int field_index) {
    switch (field_index) {
        case 0: return s.cpu_percent;
        case 1: return s.memory_percent;
        case 2: return s.disk_read_bps;
        case 3: return s.disk_write_bps;
        case 4: return s.net_recv_bps;
        case 5: return s.net_sent_bps;
        default: return 0.0;
    }
}

inline void SetMetricStats(StatsResult& result, int field_index, const MetricStats& stats) {
    switch (field_index) {
        case 0: result.cpu = stats; break;
        case 1: result.memory = stats; break;
        case 2: result.disk_read = stats; break;
        case 3: result.disk_write = stats; break;
        case 4: result.net_recv = stats; break;
        case 5: result.net_sent = stats; break;
        default: break;
    }
}

double ComputePercentile(const std::vector<double>& sorted_values, double percentile) {
    const int n = static_cast<int>(sorted_values.size());
    if (n == 1) {
        return sorted_values[0];
    }

    const double index = static_cast<double>(n - 1) * percentile;
    const int lower = static_cast<int>(std::floor(index));
    const int upper = static_cast<int>(std::ceil(index));
    const double fraction = index - static_cast<double>(lower);

    if (lower == upper) {
        return sorted_values[static_cast<std::size_t>(lower)];
    }

    return sorted_values[static_cast<std::size_t>(lower)] +
           (sorted_values[static_cast<std::size_t>(upper)] -
            sorted_values[static_cast<std::size_t>(lower)]) * fraction;
}

MetricStats ComputeMetricStats(const std::vector<double>& values) {
    MetricStats stats;
    const int n = static_cast<int>(values.size());

    // Compute avg, min, max in a single pass
    double sum = 0.0;
    stats.min = values[0];
    stats.max = values[0];
    for (int i = 0; i < n; ++i) {
        const double v = values[static_cast<std::size_t>(i)];
        sum += v;
        if (v < stats.min) stats.min = v;
        if (v > stats.max) stats.max = v;
    }
    stats.avg = sum / static_cast<double>(n);

    // Compute standard deviation (population stddev)
    double variance_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double diff = values[static_cast<std::size_t>(i)] - stats.avg;
        variance_sum += diff * diff;
    }
    stats.stddev = std::sqrt(variance_sum / static_cast<double>(n));

    // Sort for percentile computation
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    stats.p50 = ComputePercentile(sorted, 0.50);
    stats.p95 = ComputePercentile(sorted, 0.95);
    stats.p99 = ComputePercentile(sorted, 0.99);

    return stats;
}

} // namespace

StatsResult ComputeStats(const std::vector<Snapshot>& snapshots) {
    if (snapshots.empty()) {
        throw std::runtime_error("Cannot compute statistics on empty snapshot collection.");
    }

    const int n = static_cast<int>(snapshots.size());

    StatsResult result;
    result.count = n;
    result.start_timestamp = snapshots.front().timestamp;
    result.end_timestamp = snapshots.back().timestamp;
    result.duration_seconds = result.end_timestamp - result.start_timestamp;

    // Find actual min/max timestamps (in case snapshots are not time-sorted)
    for (int i = 0; i < n; ++i) {
        const double ts = snapshots[static_cast<std::size_t>(i)].timestamp;
        if (ts < result.start_timestamp) result.start_timestamp = ts;
        if (ts > result.end_timestamp) result.end_timestamp = ts;
    }
    result.duration_seconds = result.end_timestamp - result.start_timestamp;

    // Compute stats for each metric field
    for (int f = 0; f < kNumFields; ++f) {
        std::vector<double> values;
        values.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            values.push_back(GetField(snapshots[static_cast<std::size_t>(i)], f));
        }
        SetMetricStats(result, f, ComputeMetricStats(values));
    }

    return result;
}

} // namespace aura::platform
