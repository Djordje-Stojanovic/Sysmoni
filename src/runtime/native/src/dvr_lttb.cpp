#include "platform_internal.hpp"

#include <cmath>
#include <stdexcept>

namespace aura::platform {

std::vector<Snapshot> DownsampleLttb(const std::vector<Snapshot>& snapshots, int target) {
    if (target < 2) {
        throw std::runtime_error("target must be an integer >= 2.");
    }

    const int n = static_cast<int>(snapshots.size());
    if (n <= target) {
        return snapshots;
    }

    std::vector<Snapshot> selected;
    selected.reserve(static_cast<std::size_t>(target));

    selected.push_back(snapshots.front());
    if (target == 2) {
        selected.push_back(snapshots.back());
        return selected;
    }

    const double bucket_size = static_cast<double>(n - 2) / static_cast<double>(target - 2);

    double prev_x = snapshots.front().timestamp;
    double prev_y = snapshots.front().cpu_percent;

    for (int i = 0; i < target - 2; ++i) {
        const int bucket_start = static_cast<int>(1 + (i * bucket_size));
        int bucket_end = static_cast<int>(1 + ((i + 1) * bucket_size));
        bucket_end = std::min(bucket_end, n - 1);

        int next_start = static_cast<int>(1 + ((i + 1) * bucket_size));
        int next_end = static_cast<int>(1 + ((i + 2) * bucket_size));
        next_end = std::min(next_end, n - 1);
        if (i == target - 3) {
            next_start = n - 1;
            next_end = n - 1;
        }

        double avg_x = 0.0;
        double avg_y = 0.0;
        const int next_count = (next_end - next_start + 1);
        for (int j = next_start; j <= next_end; ++j) {
            avg_x += snapshots[static_cast<std::size_t>(j)].timestamp;
            avg_y += snapshots[static_cast<std::size_t>(j)].cpu_percent;
        }
        avg_x /= static_cast<double>(next_count);
        avg_y /= static_cast<double>(next_count);

        double best_area = -1.0;
        int best_idx = bucket_start;
        for (int j = bucket_start; j < bucket_end; ++j) {
            const Snapshot& candidate = snapshots[static_cast<std::size_t>(j)];
            const double area = std::abs(
                prev_x * (candidate.cpu_percent - avg_y) +
                candidate.timestamp * (avg_y - prev_y) +
                avg_x * (prev_y - candidate.cpu_percent)
            );
            if (area > best_area) {
                best_area = area;
                best_idx = j;
            }
        }

        const Snapshot& chosen = snapshots[static_cast<std::size_t>(best_idx)];
        selected.push_back(chosen);
        prev_x = chosen.timestamp;
        prev_y = chosen.cpu_percent;
    }

    selected.push_back(snapshots.back());
    return selected;
}

std::vector<Snapshot> QueryTimeline(
    TelemetryStore& store,
    const std::optional<double>& start,
    const std::optional<double>& end,
    int resolution
) {
    if (resolution < 2) {
        throw std::runtime_error("resolution must be an integer >= 2.");
    }

    const std::vector<Snapshot> snapshots = store.Between(start, end);
    if (snapshots.empty()) {
        return {};
    }
    return DownsampleLttb(snapshots, resolution);
}

} // namespace aura::platform
