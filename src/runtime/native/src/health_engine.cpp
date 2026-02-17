#include "health_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aura::platform {

// ---------------------------------------------------------------------------
// Health Score — internal helpers
// ---------------------------------------------------------------------------

namespace {

// Clamp a value to [lo, hi].
inline double Clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Inverse-linear score: 100 at 0%, 0 at 100%.
inline double ScorePercent(double percent) {
    return Clamp(100.0 - percent, 0.0, 100.0);
}

// Score for I/O rate metrics (bytes-per-second).
// Returns 100 for rates below `low_bps`, 0 for rates above `high_bps`,
// linear interpolation between.
inline double ScoreRate(double bps, double low_bps, double high_bps) {
    if (!std::isfinite(bps) || bps <= low_bps) return 100.0;
    if (bps >= high_bps) return 0.0;
    return 100.0 * (1.0 - (bps - low_bps) / (high_bps - low_bps));
}

// Disk score: combination of read + write.
// Thresholds: <50 MB/s combined = 100, >500 MB/s combined = 0.
constexpr double kDiskLowBps = 50.0 * 1024.0 * 1024.0;   // 50 MB/s
constexpr double kDiskHighBps = 500.0 * 1024.0 * 1024.0;  // 500 MB/s

// Network score: combination of recv + sent.
// Thresholds: <10 MB/s combined = 100, >100 MB/s combined = 0.
constexpr double kNetLowBps = 10.0 * 1024.0 * 1024.0;     // 10 MB/s
constexpr double kNetHighBps = 100.0 * 1024.0 * 1024.0;    // 100 MB/s

// Get metric field value by index (mirrors stats_engine.cpp pattern).
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

// ---------------------------------------------------------------------------
// Health Score — public API
// ---------------------------------------------------------------------------

HealthScore ComputeHealthScore(const Snapshot& snapshot) {
    static constexpr HealthWeights kDefaultWeights{0.35, 0.35, 0.15, 0.15};
    return ComputeHealthScoreWeighted(snapshot, kDefaultWeights);
}

HealthScore ComputeHealthScoreWeighted(const Snapshot& snapshot, const HealthWeights& weights) {
    HealthScore score;

    // CPU: inverse of utilization percentage
    const double cpu = std::isfinite(snapshot.cpu_percent) ? snapshot.cpu_percent : 0.0;
    score.cpu_score = ScorePercent(Clamp(cpu, 0.0, 100.0));

    // Memory: inverse of utilization percentage
    const double mem = std::isfinite(snapshot.memory_percent) ? snapshot.memory_percent : 0.0;
    score.memory_score = ScorePercent(Clamp(mem, 0.0, 100.0));

    // Disk: combined read+write rate score
    const double dr = std::isfinite(snapshot.disk_read_bps) ? std::max(0.0, snapshot.disk_read_bps) : 0.0;
    const double dw = std::isfinite(snapshot.disk_write_bps) ? std::max(0.0, snapshot.disk_write_bps) : 0.0;
    score.disk_score = ScoreRate(dr + dw, kDiskLowBps, kDiskHighBps);

    // Network: combined recv+sent rate score
    const double nr = std::isfinite(snapshot.net_recv_bps) ? std::max(0.0, snapshot.net_recv_bps) : 0.0;
    const double ns = std::isfinite(snapshot.net_sent_bps) ? std::max(0.0, snapshot.net_sent_bps) : 0.0;
    score.network_score = ScoreRate(nr + ns, kNetLowBps, kNetHighBps);

    // Weighted composite
    const double w_cpu = std::isfinite(weights.cpu) ? std::max(0.0, weights.cpu) : 0.35;
    const double w_mem = std::isfinite(weights.memory) ? std::max(0.0, weights.memory) : 0.35;
    const double w_disk = std::isfinite(weights.disk) ? std::max(0.0, weights.disk) : 0.15;
    const double w_net = std::isfinite(weights.network) ? std::max(0.0, weights.network) : 0.15;
    const double w_total = w_cpu + w_mem + w_disk + w_net;

    if (w_total > 0.0) {
        score.overall = (score.cpu_score * w_cpu +
                        score.memory_score * w_mem +
                        score.disk_score * w_disk +
                        score.network_score * w_net) / w_total;
    } else {
        score.overall = 0.0;
    }

    score.overall = Clamp(score.overall, 0.0, 100.0);

    return score;
}

// ---------------------------------------------------------------------------
// Trend Detection — linear regression
// ---------------------------------------------------------------------------

TrendResult DetectTrend(
    const std::vector<Snapshot>& snapshots,
    int metric_index,
    double sensitivity
) {
    TrendResult result;

    if (snapshots.size() < 2) {
        return result; // Stable by default, not enough data
    }
    if (metric_index < 0 || metric_index > 5) {
        throw std::invalid_argument("metric_index must be 0-5");
    }
    if (!std::isfinite(sensitivity) || sensitivity < 0.0) {
        throw std::invalid_argument("sensitivity must be >= 0 and finite");
    }

    const int n = static_cast<int>(snapshots.size());

    // Collect (t, v) pairs using relative timestamps for numerical stability
    const double t0 = snapshots.front().timestamp;
    double sum_t = 0.0;
    double sum_v = 0.0;
    double sum_tt = 0.0;
    double sum_tv = 0.0;
    double sum_vv = 0.0;

    for (int i = 0; i < n; ++i) {
        const double t = snapshots[static_cast<std::size_t>(i)].timestamp - t0;
        const double v = GetField(snapshots[static_cast<std::size_t>(i)], metric_index);

        sum_t += t;
        sum_v += v;
        sum_tt += t * t;
        sum_tv += t * v;
        sum_vv += v * v;
    }

    const double dn = static_cast<double>(n);
    const double mean_t = sum_t / dn;
    const double mean_v = sum_v / dn;

    // Least-squares slope and intercept
    const double ss_tt = sum_tt - dn * mean_t * mean_t;
    const double ss_tv = sum_tv - dn * mean_t * mean_v;
    const double ss_vv = sum_vv - dn * mean_v * mean_v;

    // Guard against zero time-span (all same timestamp)
    if (std::fabs(ss_tt) < 1e-15) {
        return result; // Cannot determine trend
    }

    result.slope = ss_tv / ss_tt;
    result.intercept = mean_v - result.slope * mean_t;

    // R-squared: coefficient of determination
    if (std::fabs(ss_vv) < 1e-15) {
        // All values identical → perfect fit to constant (slope ≈ 0)
        result.r_squared = 1.0;
    } else {
        result.r_squared = (ss_tv * ss_tv) / (ss_tt * ss_vv);
        result.r_squared = Clamp(result.r_squared, 0.0, 1.0);
    }

    // Classify direction based on slope magnitude vs sensitivity
    if (result.slope > sensitivity) {
        result.direction = TrendDirection::Rising;
    } else if (result.slope < -sensitivity) {
        result.direction = TrendDirection::Falling;
    } else {
        result.direction = TrendDirection::Stable;
    }

    return result;
}

// ---------------------------------------------------------------------------
// EMA Smoother
// ---------------------------------------------------------------------------

EmaSmoother::EmaSmoother(double alpha) : alpha_(alpha) {
    if (!std::isfinite(alpha) || alpha <= 0.0 || alpha > 1.0) {
        throw std::invalid_argument("alpha must be in (0, 1]");
    }
}

Snapshot EmaSmoother::Update(const Snapshot& raw) {
    std::lock_guard<std::mutex> lock(mu_);

    if (!initialized_) {
        state_ = raw;
        initialized_ = true;
        return state_;
    }

    // EMA: smoothed = alpha * raw + (1 - alpha) * prev
    const double a = alpha_;
    const double b = 1.0 - a;

    state_.timestamp = raw.timestamp; // Always use latest timestamp
    state_.cpu_percent = a * raw.cpu_percent + b * state_.cpu_percent;
    state_.memory_percent = a * raw.memory_percent + b * state_.memory_percent;
    state_.disk_read_bps = a * raw.disk_read_bps + b * state_.disk_read_bps;
    state_.disk_write_bps = a * raw.disk_write_bps + b * state_.disk_write_bps;
    state_.net_recv_bps = a * raw.net_recv_bps + b * state_.net_recv_bps;
    state_.net_sent_bps = a * raw.net_sent_bps + b * state_.net_sent_bps;

    return state_;
}

Snapshot EmaSmoother::Current() const {
    std::lock_guard<std::mutex> lock(mu_);
    return state_;
}

void EmaSmoother::Reset() {
    std::lock_guard<std::mutex> lock(mu_);
    initialized_ = false;
    state_ = Snapshot{};
}

} // namespace aura::platform
