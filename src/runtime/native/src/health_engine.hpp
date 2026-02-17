#pragma once

#include "platform_internal.hpp"

#include <cmath>
#include <mutex>
#include <vector>

namespace aura::platform {

// ---------------------------------------------------------------------------
// Health Score — composite 0-100 system health index
// ---------------------------------------------------------------------------

struct HealthScore {
    double overall = 0.0;       // 0-100 weighted composite
    double cpu_score = 0.0;     // 0-100 (100 = idle, 0 = maxed)
    double memory_score = 0.0;  // 0-100 (100 = free, 0 = full)
    double disk_score = 0.0;    // 0-100 (100 = quiet, 0 = saturated)
    double network_score = 0.0; // 0-100 (100 = quiet, 0 = saturated)
};

// Weights for composite score
struct HealthWeights {
    double cpu = 0.35;
    double memory = 0.35;
    double disk = 0.15;
    double network = 0.15;
};

HealthScore ComputeHealthScore(const Snapshot& snapshot);
HealthScore ComputeHealthScoreWeighted(const Snapshot& snapshot, const HealthWeights& weights);

// ---------------------------------------------------------------------------
// Trend Detection — linear regression on recent metric values
// ---------------------------------------------------------------------------

enum class TrendDirection {
    Stable = 0,
    Rising = 1,
    Falling = 2,
};

struct TrendResult {
    TrendDirection direction = TrendDirection::Stable;
    double slope = 0.0;        // rate of change per second
    double r_squared = 0.0;    // 0-1 goodness of fit (confidence)
    double intercept = 0.0;    // y-intercept of regression line
};

TrendResult DetectTrend(
    const std::vector<Snapshot>& snapshots,
    int metric_index,          // 0=cpu, 1=mem, 2=disk_r, 3=disk_w, 4=net_r, 5=net_s
    double sensitivity = 0.1   // minimum |slope| per second to qualify as rising/falling
);

// ---------------------------------------------------------------------------
// EMA Smoother — exponential moving average for jitter-free telemetry
// ---------------------------------------------------------------------------

class EmaSmoother {
public:
    explicit EmaSmoother(double alpha);
    ~EmaSmoother() = default;

    EmaSmoother(const EmaSmoother&) = delete;
    EmaSmoother& operator=(const EmaSmoother&) = delete;

    Snapshot Update(const Snapshot& raw);
    Snapshot Current() const;
    void Reset();
    double Alpha() const { return alpha_; }

private:
    double alpha_;
    bool initialized_ = false;
    Snapshot state_;
    mutable std::mutex mu_;
};

} // namespace aura::platform
