#include "test_platform_helpers.hpp"
#include "test_platform_tests.hpp"

#include <cmath>
#include <limits>
#include <vector>

// =========================================================================
// Category A: Health Score — Basic Correctness
// =========================================================================

void TestHealthScoreIdleSystem() {
    // Idle system: 0% CPU, 0% memory, no disk, no network → score = 100
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 0.0;
    snap.memory_percent = 0.0;
    snap.disk_read_bps = 0.0;
    snap.disk_write_bps = 0.0;
    snap.net_recv_bps = 0.0;
    snap.net_sent_bps = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "idle system: should succeed");

    ExpectNear(score.overall, 100.0, 1e-9, "idle system: overall = 100");
    ExpectNear(score.cpu_score, 100.0, 1e-9, "idle system: cpu_score = 100");
    ExpectNear(score.memory_score, 100.0, 1e-9, "idle system: memory_score = 100");
    ExpectNear(score.disk_score, 100.0, 1e-9, "idle system: disk_score = 100");
    ExpectNear(score.network_score, 100.0, 1e-9, "idle system: network_score = 100");
}

void TestHealthScoreFullLoad() {
    // 100% CPU, 100% memory → cpu=0, mem=0, disk=100, net=100
    // overall = (0*0.35 + 0*0.35 + 100*0.15 + 100*0.15) = 30
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 100.0;
    snap.memory_percent = 100.0;
    snap.disk_read_bps = 0.0;
    snap.disk_write_bps = 0.0;
    snap.net_recv_bps = 0.0;
    snap.net_sent_bps = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "full load: should succeed");

    ExpectNear(score.cpu_score, 0.0, 1e-9, "full load: cpu_score = 0");
    ExpectNear(score.memory_score, 0.0, 1e-9, "full load: memory_score = 0");
    ExpectNear(score.disk_score, 100.0, 1e-9, "full load: disk_score = 100 (no I/O)");
    ExpectNear(score.network_score, 100.0, 1e-9, "full load: net_score = 100 (no traffic)");
    ExpectNear(score.overall, 30.0, 1e-9, "full load: overall = 30");
}

void TestHealthScoreHalfLoad() {
    // 50% CPU, 50% memory → cpu=50, mem=50, disk=100, net=100
    // overall = (50*0.35 + 50*0.35 + 100*0.15 + 100*0.15) = 17.5 + 17.5 + 15 + 15 = 65
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;
    snap.memory_percent = 50.0;
    snap.disk_read_bps = 0.0;
    snap.disk_write_bps = 0.0;
    snap.net_recv_bps = 0.0;
    snap.net_sent_bps = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "half load: should succeed");

    ExpectNear(score.cpu_score, 50.0, 1e-9, "half load: cpu_score = 50");
    ExpectNear(score.memory_score, 50.0, 1e-9, "half load: memory_score = 50");
    ExpectNear(score.overall, 65.0, 1e-9, "half load: overall = 65");
}

void TestHealthScoreHighDiskIo() {
    // CPU/mem idle but disk saturated: 500 MB/s combined → disk_score = 0
    const double disk_bps = 500.0 * 1024.0 * 1024.0;
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 0.0;
    snap.memory_percent = 0.0;
    snap.disk_read_bps = disk_bps / 2.0;
    snap.disk_write_bps = disk_bps / 2.0;
    snap.net_recv_bps = 0.0;
    snap.net_sent_bps = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "high disk: should succeed");

    ExpectNear(score.disk_score, 0.0, 1e-9, "high disk: disk_score = 0");
    ExpectNear(score.cpu_score, 100.0, 1e-9, "high disk: cpu_score = 100");
    ExpectNear(score.network_score, 100.0, 1e-9, "high disk: net_score = 100");
    // overall = (100*0.35 + 100*0.35 + 0*0.15 + 100*0.15) = 35+35+0+15 = 85
    ExpectNear(score.overall, 85.0, 1e-9, "high disk: overall = 85");
}

void TestHealthScoreHighNetwork() {
    // CPU/mem idle but network saturated: 100 MB/s combined → net_score = 0
    const double net_bps = 100.0 * 1024.0 * 1024.0;
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 0.0;
    snap.memory_percent = 0.0;
    snap.disk_read_bps = 0.0;
    snap.disk_write_bps = 0.0;
    snap.net_recv_bps = net_bps / 2.0;
    snap.net_sent_bps = net_bps / 2.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "high net: should succeed");

    ExpectNear(score.network_score, 0.0, 1e-9, "high net: net_score = 0");
    // overall = (100*0.35 + 100*0.35 + 100*0.15 + 0*0.15) = 85
    ExpectNear(score.overall, 85.0, 1e-9, "high net: overall = 85");
}

void TestHealthScoreEverythingMaxed() {
    // Everything maxed out → all scores = 0 → overall = 0
    const double disk_bps = 500.0 * 1024.0 * 1024.0;
    const double net_bps = 100.0 * 1024.0 * 1024.0;
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 100.0;
    snap.memory_percent = 100.0;
    snap.disk_read_bps = disk_bps;
    snap.disk_write_bps = disk_bps;
    snap.net_recv_bps = net_bps;
    snap.net_sent_bps = net_bps;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "everything maxed: should succeed");

    ExpectNear(score.cpu_score, 0.0, 1e-9, "everything maxed: cpu = 0");
    ExpectNear(score.memory_score, 0.0, 1e-9, "everything maxed: mem = 0");
    ExpectNear(score.disk_score, 0.0, 1e-9, "everything maxed: disk = 0");
    ExpectNear(score.network_score, 0.0, 1e-9, "everything maxed: net = 0");
    ExpectNear(score.overall, 0.0, 1e-9, "everything maxed: overall = 0");
}

void TestHealthScoreRangeAlwaysValid() {
    // Sweep CPU from 0..100 and verify all scores stay in [0, 100]
    for (int pct = 0; pct <= 100; pct += 5) {
        aura_snapshot_t snap{};
        snap.timestamp = 1000.0;
        snap.cpu_percent = static_cast<double>(pct);
        snap.memory_percent = static_cast<double>(100 - pct);

        aura_health_score_t score{};
        aura_error_t error{};
        const int rc = aura_health_score_compute(&snap, &score, &error);
        ExpectEq(rc, AURA_OK, "range sweep: should succeed");
        ExpectTrue(score.overall >= 0.0 && score.overall <= 100.0,
            "range sweep: overall in [0, 100]");
        ExpectTrue(score.cpu_score >= 0.0 && score.cpu_score <= 100.0,
            "range sweep: cpu in [0, 100]");
        ExpectTrue(score.memory_score >= 0.0 && score.memory_score <= 100.0,
            "range sweep: memory in [0, 100]");
    }
}

// =========================================================================
// Category B: Health Score — Custom Weights
// =========================================================================

void TestHealthScoreCustomWeightsCpuOnly() {
    // Weight all on CPU: 50% CPU → cpu_score = 50
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;
    snap.memory_percent = 100.0;

    aura_health_weights_t weights{};
    weights.cpu = 1.0;
    weights.memory = 0.0;
    weights.disk = 0.0;
    weights.network = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute_weighted(&snap, &weights, &score, &error);
    ExpectEq(rc, AURA_OK, "cpu only weight: should succeed");
    ExpectNear(score.overall, 50.0, 1e-9, "cpu only weight: overall = cpu_score = 50");
}

void TestHealthScoreCustomWeightsMemoryOnly() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 100.0;
    snap.memory_percent = 30.0;

    aura_health_weights_t weights{};
    weights.cpu = 0.0;
    weights.memory = 1.0;
    weights.disk = 0.0;
    weights.network = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute_weighted(&snap, &weights, &score, &error);
    ExpectEq(rc, AURA_OK, "memory only weight: should succeed");
    ExpectNear(score.overall, 70.0, 1e-9, "memory only weight: overall = 70");
}

void TestHealthScoreCustomWeightsEqual() {
    // Equal weights: 25% each
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 40.0;
    snap.memory_percent = 60.0;

    aura_health_weights_t weights{};
    weights.cpu = 0.25;
    weights.memory = 0.25;
    weights.disk = 0.25;
    weights.network = 0.25;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute_weighted(&snap, &weights, &score, &error);
    ExpectEq(rc, AURA_OK, "equal weights: should succeed");
    // cpu_score=60, mem_score=40, disk_score=100, net_score=100
    // overall = (60 + 40 + 100 + 100) / 4 = 75
    ExpectNear(score.overall, 75.0, 1e-9, "equal weights: overall = 75");
}

void TestHealthScoreZeroWeightsReturnsZero() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;

    aura_health_weights_t weights{};
    weights.cpu = 0.0;
    weights.memory = 0.0;
    weights.disk = 0.0;
    weights.network = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute_weighted(&snap, &weights, &score, &error);
    ExpectEq(rc, AURA_OK, "zero weights: should succeed");
    ExpectNear(score.overall, 0.0, 1e-9, "zero weights: overall = 0");
}

// =========================================================================
// Category C: Health Score — Edge Cases & NaN Safety
// =========================================================================

void TestHealthScoreNaNFieldsSafe() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = std::numeric_limits<double>::quiet_NaN();
    snap.memory_percent = std::numeric_limits<double>::quiet_NaN();
    snap.disk_read_bps = std::numeric_limits<double>::quiet_NaN();
    snap.net_recv_bps = std::numeric_limits<double>::quiet_NaN();

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "NaN fields: should succeed (graceful degradation)");
    ExpectTrue(std::isfinite(score.overall), "NaN fields: overall is finite");
    ExpectTrue(score.overall >= 0.0 && score.overall <= 100.0, "NaN fields: overall in range");
}

void TestHealthScoreInfFieldsSafe() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = std::numeric_limits<double>::infinity();
    snap.memory_percent = -std::numeric_limits<double>::infinity();

    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, &score, &error);
    ExpectEq(rc, AURA_OK, "Inf fields: should succeed (graceful degradation)");
    ExpectTrue(std::isfinite(score.overall), "Inf fields: overall is finite");
}

void TestHealthScoreNegativeCpuClamped() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = -10.0;
    snap.memory_percent = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    aura_health_score_compute(&snap, &score, &error);
    ExpectNear(score.cpu_score, 100.0, 1e-9, "negative cpu: clamped to 0 → score = 100");
}

void TestHealthScoreOver100CpuClamped() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 150.0;
    snap.memory_percent = 0.0;

    aura_health_score_t score{};
    aura_error_t error{};
    aura_health_score_compute(&snap, &score, &error);
    ExpectNear(score.cpu_score, 0.0, 1e-9, "over 100 cpu: clamped to 100 → score = 0");
}

// =========================================================================
// Category D: Health Score — C ABI Safety
// =========================================================================

void TestHealthScoreAbiNullSnapshot() {
    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(nullptr, &score, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null snapshot: error code");
}

void TestHealthScoreAbiNullOutScore() {
    aura_snapshot_t snap{};
    aura_error_t error{};
    const int rc = aura_health_score_compute(&snap, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null out_score: error code");
}

void TestHealthScoreAbiNullError() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;
    aura_health_score_t score{};
    const int rc = aura_health_score_compute(&snap, &score, nullptr);
    ExpectEq(rc, AURA_OK, "null error (valid call): should succeed");
}

void TestHealthScoreWeightedAbiNullWeights() {
    aura_snapshot_t snap{};
    aura_health_score_t score{};
    aura_error_t error{};
    const int rc = aura_health_score_compute_weighted(&snap, nullptr, &score, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null weights: error code");
}

// =========================================================================
// Category E: Trend Detection — Basic Correctness
// =========================================================================

void TestTrendRisingCpu() {
    // CPU ramp: 0, 1, 2, ..., 9 over 10 seconds → slope = 1.0/sec
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);
        snaps.push_back(s);
    }

    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(snaps.data(), N, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "rising cpu: should succeed");
    ExpectEq(trend.direction, AURA_TREND_RISING, "rising cpu: direction = RISING");
    ExpectNear(trend.slope, 1.0, 1e-9, "rising cpu: slope = 1.0/sec");
    ExpectTrue(trend.r_squared > 0.99, "rising cpu: R² ≈ 1.0 (perfect linear)");
}

void TestTrendFallingMemory() {
    // Memory ramp down: 100, 90, 80, ..., 10 over 10 seconds
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.memory_percent = 100.0 - static_cast<double>(i) * 10.0;
        snaps.push_back(s);
    }

    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(snaps.data(), N, AURA_METRIC_MEMORY_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "falling memory: should succeed");
    ExpectEq(trend.direction, AURA_TREND_FALLING, "falling memory: direction = FALLING");
    ExpectNear(trend.slope, -10.0, 1e-9, "falling memory: slope = -10.0/sec");
    ExpectTrue(trend.r_squared > 0.99, "falling memory: R² ≈ 1.0");
}

void TestTrendStableConstant() {
    // Constant values → slope = 0, direction = STABLE
    const int N = 20;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        snaps.push_back(s);
    }

    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(snaps.data(), N, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "stable constant: should succeed");
    ExpectEq(trend.direction, AURA_TREND_STABLE, "stable constant: direction = STABLE");
    ExpectNear(trend.slope, 0.0, 1e-9, "stable constant: slope = 0");
}

void TestTrendSensitivityThreshold() {
    // Gentle slope of 0.05/sec, sensitivity = 0.1 → should be STABLE
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0 + static_cast<double>(i) * 0.05;
        snaps.push_back(s);
    }

    aura_trend_result_t trend{};
    aura_error_t error{};
    int rc = aura_trend_detect(snaps.data(), N, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "low sensitivity: should succeed");
    ExpectEq(trend.direction, AURA_TREND_STABLE, "low sensitivity: STABLE (slope < threshold)");

    // Same data with sensitivity = 0.01 → should be RISING
    rc = aura_trend_detect(snaps.data(), N, AURA_METRIC_CPU_PERCENT, 0.01, &trend, &error);
    ExpectEq(rc, AURA_OK, "high sensitivity: should succeed");
    ExpectEq(trend.direction, AURA_TREND_RISING, "high sensitivity: RISING (slope > threshold)");
}

void TestTrendSingleSnapshot() {
    // Single snapshot → not enough data → STABLE
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;

    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(&snap, 1, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "single snapshot: should succeed");
    ExpectEq(trend.direction, AURA_TREND_STABLE, "single snapshot: direction = STABLE");
    ExpectNear(trend.slope, 0.0, 1e-9, "single snapshot: slope = 0");
}

void TestTrendEmptyInput() {
    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(nullptr, 0, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "empty input: should succeed (stable by default)");
    ExpectEq(trend.direction, AURA_TREND_STABLE, "empty input: direction = STABLE");
}

void TestTrendAllMetricIndices() {
    // Verify all 6 metric indices work
    const int N = 5;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);
        s.memory_percent = static_cast<double>(i) * 2.0;
        s.disk_read_bps = static_cast<double>(i) * 1000.0;
        s.disk_write_bps = static_cast<double>(i) * 2000.0;
        s.net_recv_bps = static_cast<double>(i) * 3000.0;
        s.net_sent_bps = static_cast<double>(i) * 4000.0;
        snaps.push_back(s);
    }

    for (int metric = 0; metric <= 5; ++metric) {
        aura_trend_result_t trend{};
        aura_error_t error{};
        const int rc = aura_trend_detect(snaps.data(), N, metric, 0.01, &trend, &error);
        ExpectEq(rc, AURA_OK, "all metrics: metric " + std::to_string(metric) + " should succeed");
        ExpectEq(trend.direction, AURA_TREND_RISING, "all metrics: metric " + std::to_string(metric) + " RISING");
    }
}

void TestTrendRSquaredForNoisyData() {
    // Noisy data: alternating high/low → R² should be low
    const int N = 20;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = (i % 2 == 0) ? 80.0 : 20.0;
        snaps.push_back(s);
    }

    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(snaps.data(), N, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "noisy data: should succeed");
    ExpectTrue(trend.r_squared < 0.1, "noisy data: R² should be low (poor fit)");
}

void TestTrendTwoSnapshots() {
    // Exactly 2 snapshots: should work, R² = 1.0 (perfect fit to a line)
    aura_snapshot_t snaps[2]{};
    snaps[0].timestamp = 1000.0;
    snaps[0].cpu_percent = 10.0;
    snaps[1].timestamp = 1001.0;
    snaps[1].cpu_percent = 20.0;

    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(snaps, 2, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_OK, "two snapshots: should succeed");
    ExpectEq(trend.direction, AURA_TREND_RISING, "two snapshots: RISING");
    ExpectNear(trend.slope, 10.0, 1e-9, "two snapshots: slope = 10.0/sec");
    ExpectNear(trend.r_squared, 1.0, 1e-9, "two snapshots: R² = 1.0");
}

// =========================================================================
// Category F: Trend Detection — C ABI Safety
// =========================================================================

void TestTrendAbiNullSnapshotsWithPositiveCount() {
    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(nullptr, 5, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null snapshots: error code");
}

void TestTrendAbiNullOutTrend() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    aura_error_t error{};
    const int rc = aura_trend_detect(&snap, 1, AURA_METRIC_CPU_PERCENT, 0.1, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null out_trend: error code");
}

void TestTrendAbiInvalidMetric() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(&snap, 1, 99, 0.1, &trend, &error);
    // metric_index 99 is out of range → should still not crash; handled as invalid_argument
    // Since count=1, DetectTrend returns STABLE without checking metric.
    // With count >= 2 it would throw.
    ExpectEq(rc, AURA_OK, "invalid metric with 1 snapshot: returns stable (no regression)");

    // Now with 2 snapshots → should get AURA_ERR_INVALID_ARGUMENT
    aura_snapshot_t snaps[2]{};
    snaps[0].timestamp = 1000.0;
    snaps[1].timestamp = 1001.0;
    const int rc2 = aura_trend_detect(snaps, 2, 99, 0.1, &trend, &error);
    ExpectEq(rc2, AURA_ERR_INVALID_ARGUMENT, "invalid metric with 2 snapshots: error code");
}

void TestTrendAbiNegativeCount() {
    aura_trend_result_t trend{};
    aura_error_t error{};
    const int rc = aura_trend_detect(nullptr, -1, AURA_METRIC_CPU_PERCENT, 0.1, &trend, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "negative count: error code");
}

void TestTrendAbiNullError() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    aura_trend_result_t trend{};
    const int rc = aura_trend_detect(&snap, 1, AURA_METRIC_CPU_PERCENT, 0.1, &trend, nullptr);
    ExpectEq(rc, AURA_OK, "null error (valid call): should succeed");
}

// =========================================================================
// Category G: EMA Smoother — Basic Correctness
// =========================================================================

void TestSmootherFirstUpdatePassthrough() {
    // First update should return the raw value unmodified
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    int rc = aura_smoother_create(0.3, &smoother, &error);
    ExpectEq(rc, AURA_OK, "smoother create: should succeed");

    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 75.0;
    raw.memory_percent = 60.0;
    raw.disk_read_bps = 5000.0;

    aura_snapshot_t smoothed{};
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectEq(rc, AURA_OK, "first update: should succeed");
    ExpectNear(smoothed.cpu_percent, 75.0, 1e-9, "first update: cpu passthrough");
    ExpectNear(smoothed.memory_percent, 60.0, 1e-9, "first update: memory passthrough");
    ExpectNear(smoothed.disk_read_bps, 5000.0, 1e-9, "first update: disk passthrough");

    aura_smoother_destroy(smoother);
}

void TestSmootherConvergence() {
    // Alpha = 0.5: after step from 0 to 100, should converge toward 100
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    int rc = aura_smoother_create(0.5, &smoother, &error);
    ExpectEq(rc, AURA_OK, "convergence: create");

    // First update: sets baseline to 0
    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 0.0;
    aura_snapshot_t smoothed{};
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 0.0, 1e-9, "convergence: first = 0");

    // Step to 100: smoothed = 0.5 * 100 + 0.5 * 0 = 50
    raw.timestamp = 1001.0;
    raw.cpu_percent = 100.0;
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 50.0, 1e-9, "convergence: second = 50");

    // Third: smoothed = 0.5 * 100 + 0.5 * 50 = 75
    raw.timestamp = 1002.0;
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 75.0, 1e-9, "convergence: third = 75");

    // Fourth: smoothed = 0.5 * 100 + 0.5 * 75 = 87.5
    raw.timestamp = 1003.0;
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 87.5, 1e-9, "convergence: fourth = 87.5");

    aura_smoother_destroy(smoother);
}

void TestSmootherAlphaOne() {
    // Alpha = 1.0: no smoothing, always returns raw value
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    int rc = aura_smoother_create(1.0, &smoother, &error);
    ExpectEq(rc, AURA_OK, "alpha=1: create");

    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 10.0;
    aura_snapshot_t smoothed{};
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 10.0, 1e-9, "alpha=1: first = 10");

    raw.timestamp = 1001.0;
    raw.cpu_percent = 90.0;
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 90.0, 1e-9, "alpha=1: second = 90 (no smoothing)");

    aura_smoother_destroy(smoother);
}

void TestSmootherSmallAlpha() {
    // Alpha = 0.1: heavy smoothing, slow convergence
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    int rc = aura_smoother_create(0.1, &smoother, &error);
    ExpectEq(rc, AURA_OK, "alpha=0.1: create");

    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 0.0;
    aura_snapshot_t smoothed{};
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 0.0, 1e-9, "alpha=0.1: first = 0");

    // Step to 100: smoothed = 0.1 * 100 + 0.9 * 0 = 10
    raw.timestamp = 1001.0;
    raw.cpu_percent = 100.0;
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 10.0, 1e-9, "alpha=0.1: second = 10 (slow)");

    aura_smoother_destroy(smoother);
}

void TestSmootherReset() {
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    int rc = aura_smoother_create(0.5, &smoother, &error);

    // Feed some data
    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 50.0;
    aura_snapshot_t smoothed{};
    aura_smoother_update(smoother, &raw, &smoothed, &error);

    raw.timestamp = 1001.0;
    raw.cpu_percent = 100.0;
    aura_smoother_update(smoother, &raw, &smoothed, &error);
    // smoothed cpu = 75 at this point

    // Reset
    rc = aura_smoother_reset(smoother, &error);
    ExpectEq(rc, AURA_OK, "reset: should succeed");

    // Next update should behave as first (passthrough)
    raw.timestamp = 1002.0;
    raw.cpu_percent = 30.0;
    rc = aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.cpu_percent, 30.0, 1e-9, "after reset: passthrough");

    aura_smoother_destroy(smoother);
}

void TestSmootherAllFieldsSmoothed() {
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    aura_smoother_create(0.5, &smoother, &error);

    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 0.0;
    raw.memory_percent = 0.0;
    raw.disk_read_bps = 0.0;
    raw.disk_write_bps = 0.0;
    raw.net_recv_bps = 0.0;
    raw.net_sent_bps = 0.0;
    aura_snapshot_t smoothed{};
    aura_smoother_update(smoother, &raw, &smoothed, &error);

    // Step all to 100
    raw.timestamp = 1001.0;
    raw.cpu_percent = 100.0;
    raw.memory_percent = 100.0;
    raw.disk_read_bps = 100.0;
    raw.disk_write_bps = 100.0;
    raw.net_recv_bps = 100.0;
    raw.net_sent_bps = 100.0;
    aura_smoother_update(smoother, &raw, &smoothed, &error);

    ExpectNear(smoothed.cpu_percent, 50.0, 1e-9, "all fields: cpu smoothed");
    ExpectNear(smoothed.memory_percent, 50.0, 1e-9, "all fields: memory smoothed");
    ExpectNear(smoothed.disk_read_bps, 50.0, 1e-9, "all fields: disk_read smoothed");
    ExpectNear(smoothed.disk_write_bps, 50.0, 1e-9, "all fields: disk_write smoothed");
    ExpectNear(smoothed.net_recv_bps, 50.0, 1e-9, "all fields: net_recv smoothed");
    ExpectNear(smoothed.net_sent_bps, 50.0, 1e-9, "all fields: net_sent smoothed");

    aura_smoother_destroy(smoother);
}

void TestSmootherTimestampUsesLatest() {
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    aura_smoother_create(0.5, &smoother, &error);

    aura_snapshot_t raw{};
    raw.timestamp = 1000.0;
    raw.cpu_percent = 50.0;
    aura_snapshot_t smoothed{};
    aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.timestamp, 1000.0, 1e-9, "timestamp: first");

    raw.timestamp = 1005.0;
    raw.cpu_percent = 60.0;
    aura_smoother_update(smoother, &raw, &smoothed, &error);
    ExpectNear(smoothed.timestamp, 1005.0, 1e-9, "timestamp: uses latest");

    aura_smoother_destroy(smoother);
}

// =========================================================================
// Category H: EMA Smoother — C ABI Safety
// =========================================================================

void TestSmootherCreateInvalidAlpha() {
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};

    // alpha = 0 → invalid
    int rc = aura_smoother_create(0.0, &smoother, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "alpha=0: error code");
    ExpectTrue(smoother == nullptr, "alpha=0: smoother stays null");

    // alpha < 0 → invalid
    rc = aura_smoother_create(-0.5, &smoother, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "alpha<0: error code");

    // alpha > 1 → invalid
    rc = aura_smoother_create(1.5, &smoother, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "alpha>1: error code");

    // alpha = NaN → invalid
    rc = aura_smoother_create(std::numeric_limits<double>::quiet_NaN(), &smoother, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "alpha=NaN: error code");
}

void TestSmootherCreateNullOut() {
    aura_error_t error{};
    const int rc = aura_smoother_create(0.5, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null out_smoother: error code");
}

void TestSmootherUpdateNullSmoother() {
    aura_snapshot_t raw{};
    aura_snapshot_t smoothed{};
    aura_error_t error{};
    const int rc = aura_smoother_update(nullptr, &raw, &smoothed, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null smoother: error code");
}

void TestSmootherUpdateNullSnapshot() {
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    aura_smoother_create(0.5, &smoother, &error);

    aura_snapshot_t smoothed{};
    const int rc = aura_smoother_update(smoother, nullptr, &smoothed, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null raw_snapshot: error code");

    aura_smoother_destroy(smoother);
}

void TestSmootherUpdateNullOutput() {
    aura_smoother_t* smoother = nullptr;
    aura_error_t error{};
    aura_smoother_create(0.5, &smoother, &error);

    aura_snapshot_t raw{};
    const int rc = aura_smoother_update(smoother, &raw, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null out_smoothed: error code");

    aura_smoother_destroy(smoother);
}

void TestSmootherDestroyNull() {
    const int rc = aura_smoother_destroy(nullptr);
    ExpectEq(rc, AURA_OK, "destroy null: should succeed (no-op)");
}

void TestSmootherResetNull() {
    aura_error_t error{};
    const int rc = aura_smoother_reset(nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "reset null: error code");
}

// =========================================================================
// Category I: Disk I/O Score Interpolation
// =========================================================================

void TestHealthScoreDiskMidpoint() {
    // 275 MB/s combined (midpoint of 50-500) → disk_score ≈ 50
    const double mid_bps = (50.0 + 500.0) / 2.0 * 1024.0 * 1024.0;
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.disk_read_bps = mid_bps;

    aura_health_score_t score{};
    aura_error_t error{};
    aura_health_score_compute(&snap, &score, &error);
    ExpectNear(score.disk_score, 50.0, 0.1, "disk midpoint: score ≈ 50");
}

void TestHealthScoreNetworkMidpoint() {
    // 55 MB/s combined (midpoint of 10-100) → net_score ≈ 50
    const double mid_bps = (10.0 + 100.0) / 2.0 * 1024.0 * 1024.0;
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.net_recv_bps = mid_bps;

    aura_health_score_t score{};
    aura_error_t error{};
    aura_health_score_compute(&snap, &score, &error);
    ExpectNear(score.network_score, 50.0, 0.1, "net midpoint: score ≈ 50");
}
