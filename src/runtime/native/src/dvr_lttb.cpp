#include "platform_internal.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aura::platform {

namespace {

constexpr int kNumFields = 6;
constexpr double kRangeEpsilon = 1e-9;

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

} // namespace

std::vector<Snapshot> DownsampleLttb(const std::vector<Snapshot>& snapshots, int target) {
    if (target < 2) {
        throw std::runtime_error("target must be an integer >= 2.");
    }

    const int n = static_cast<int>(snapshots.size());
    if (n <= target) {
        return snapshots;
    }

    // Pre-pass: compute min/max range for each numeric field.
    double field_min[kNumFields];
    double field_max[kNumFields];
    for (int f = 0; f < kNumFields; ++f) {
        field_min[f] = GetField(snapshots.front(), f);
        field_max[f] = field_min[f];
    }
    for (int i = 1; i < n; ++i) {
        for (int f = 0; f < kNumFields; ++f) {
            const double v = GetField(snapshots[static_cast<std::size_t>(i)], f);
            if (v < field_min[f]) field_min[f] = v;
            if (v > field_max[f]) field_max[f] = v;
        }
    }

    // Determine active fields (those with non-trivial range).
    int active[kNumFields];
    double range[kNumFields] = {};
    int num_active = 0;
    for (int f = 0; f < kNumFields; ++f) {
        range[f] = field_max[f] - field_min[f];
        if (range[f] >= kRangeEpsilon) {
            active[num_active++] = f;
        }
    }

    // Fallback: if all fields are constant, use cpu_percent with unit range.
    if (num_active == 0) {
        active[0] = 0;
        range[0] = 1.0;
        num_active = 1;
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
    double prev_y[kNumFields];
    for (int f = 0; f < kNumFields; ++f) {
        prev_y[f] = GetField(snapshots.front(), f);
    }

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

        // Compute next-bucket averages for all active fields.
        double avg_x = 0.0;
        double avg_y[kNumFields] = {};
        const int next_count = (next_end - next_start + 1);
        for (int j = next_start; j <= next_end; ++j) {
            const Snapshot& s = snapshots[static_cast<std::size_t>(j)];
            avg_x += s.timestamp;
            for (int af = 0; af < num_active; ++af) {
                avg_y[active[af]] += GetField(s, active[af]);
            }
        }
        avg_x /= static_cast<double>(next_count);
        for (int af = 0; af < num_active; ++af) {
            avg_y[active[af]] /= static_cast<double>(next_count);
        }

        // Find candidate with maximum normalized area across all active fields.
        double best_area = -1.0;
        int best_idx = bucket_start;
        for (int j = bucket_start; j < bucket_end; ++j) {
            const Snapshot& candidate = snapshots[static_cast<std::size_t>(j)];
            double max_normalized = -1.0;
            for (int af = 0; af < num_active; ++af) {
                const int f = active[af];
                const double cy = GetField(candidate, f);
                const double area = std::abs(
                    prev_x * (cy - avg_y[f]) +
                    candidate.timestamp * (avg_y[f] - prev_y[f]) +
                    avg_x * (prev_y[f] - cy)
                );
                const double normalized = area / range[f];
                if (normalized > max_normalized) {
                    max_normalized = normalized;
                }
            }
            if (max_normalized > best_area) {
                best_area = max_normalized;
                best_idx = j;
            }
        }

        const Snapshot& chosen = snapshots[static_cast<std::size_t>(best_idx)];
        selected.push_back(chosen);
        prev_x = chosen.timestamp;
        for (int f = 0; f < kNumFields; ++f) {
            prev_y[f] = GetField(chosen, f);
        }
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
