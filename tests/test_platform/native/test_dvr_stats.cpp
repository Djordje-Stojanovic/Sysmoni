#include "test_platform_helpers.hpp"
#include "test_platform_tests.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Category A: Stats Engine — Basic Correctness
// ---------------------------------------------------------------------------

void TestStatsEmptyInput() {
    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(nullptr, 0, &stats, &error);
    ExpectTrue(rc != AURA_OK, "empty input: should return error");
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "empty input: error code");
}

void TestStatsSingleSnapshot() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 42.0;
    snap.memory_percent = 55.0;
    snap.disk_read_bps = 1000.0;
    snap.disk_write_bps = 2000.0;
    snap.net_recv_bps = 3000.0;
    snap.net_sent_bps = 4000.0;

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(&snap, 1, &stats, &error);
    ExpectEq(rc, AURA_OK, "single snapshot: should succeed");
    ExpectEq(stats.count, 1, "single snapshot: count");

    // For a single value: avg = min = max = p50 = p95 = p99 = value, stddev = 0
    ExpectNear(stats.cpu.avg, 42.0, 1e-9, "single snapshot: cpu avg");
    ExpectNear(stats.cpu.min, 42.0, 1e-9, "single snapshot: cpu min");
    ExpectNear(stats.cpu.max, 42.0, 1e-9, "single snapshot: cpu max");
    ExpectNear(stats.cpu.p50, 42.0, 1e-9, "single snapshot: cpu p50");
    ExpectNear(stats.cpu.p95, 42.0, 1e-9, "single snapshot: cpu p95");
    ExpectNear(stats.cpu.p99, 42.0, 1e-9, "single snapshot: cpu p99");
    ExpectNear(stats.cpu.stddev, 0.0, 1e-9, "single snapshot: cpu stddev");

    ExpectNear(stats.memory.avg, 55.0, 1e-9, "single snapshot: memory avg");
    ExpectNear(stats.disk_read.avg, 1000.0, 1e-9, "single snapshot: disk_read avg");
    ExpectNear(stats.disk_write.avg, 2000.0, 1e-9, "single snapshot: disk_write avg");
    ExpectNear(stats.net_recv.avg, 3000.0, 1e-9, "single snapshot: net_recv avg");
    ExpectNear(stats.net_sent.avg, 4000.0, 1e-9, "single snapshot: net_sent avg");
}

void TestStatsTwoSnapshots() {
    aura_snapshot_t snaps[2]{};
    snaps[0].timestamp = 1000.0;
    snaps[0].cpu_percent = 20.0;
    snaps[0].memory_percent = 40.0;

    snaps[1].timestamp = 1001.0;
    snaps[1].cpu_percent = 80.0;
    snaps[1].memory_percent = 60.0;

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps, 2, &stats, &error);
    ExpectEq(rc, AURA_OK, "two snapshots: should succeed");
    ExpectEq(stats.count, 2, "two snapshots: count");

    ExpectNear(stats.cpu.avg, 50.0, 1e-9, "two snapshots: cpu avg = (20+80)/2");
    ExpectNear(stats.cpu.min, 20.0, 1e-9, "two snapshots: cpu min");
    ExpectNear(stats.cpu.max, 80.0, 1e-9, "two snapshots: cpu max");
    ExpectNear(stats.memory.avg, 50.0, 1e-9, "two snapshots: memory avg = (40+60)/2");

    // stddev for {20, 80}: mean=50, variance=((30^2 + 30^2)/2)=900, stddev=30
    ExpectNear(stats.cpu.stddev, 30.0, 1e-9, "two snapshots: cpu stddev");
}

void TestStatsKnownLinearDistribution() {
    // Linear ramp 0..99 for CPU
    const int N = 100;
    std::vector<aura_snapshot_t> snaps;
    snaps.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "linear distribution: should succeed");
    ExpectEq(stats.count, N, "linear distribution: count");

    ExpectNear(stats.cpu.avg, 49.5, 1e-9, "linear distribution: cpu avg");
    ExpectNear(stats.cpu.min, 0.0, 1e-9, "linear distribution: cpu min");
    ExpectNear(stats.cpu.max, 99.0, 1e-9, "linear distribution: cpu max");

    // P50 of 0..99: index = 99 * 0.50 = 49.5, interpolated between 49 and 50 = 49.5
    ExpectNear(stats.cpu.p50, 49.5, 0.1, "linear distribution: cpu p50");
    // P95 of 0..99: index = 99 * 0.95 = 94.05
    ExpectNear(stats.cpu.p95, 94.05, 0.1, "linear distribution: cpu p95");
    // P99 of 0..99: index = 99 * 0.99 = 98.01
    ExpectNear(stats.cpu.p99, 98.01, 0.1, "linear distribution: cpu p99");
}

void TestStatsConstantValues() {
    const int N = 20;
    std::vector<aura_snapshot_t> snaps;
    snaps.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 42.0;
        s.memory_percent = 55.0;
        s.disk_read_bps = 1000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "constant values: should succeed");

    ExpectNear(stats.cpu.avg, 42.0, 1e-9, "constant values: cpu avg");
    ExpectNear(stats.cpu.min, 42.0, 1e-9, "constant values: cpu min");
    ExpectNear(stats.cpu.max, 42.0, 1e-9, "constant values: cpu max");
    ExpectNear(stats.cpu.p50, 42.0, 1e-9, "constant values: cpu p50");
    ExpectNear(stats.cpu.p95, 42.0, 1e-9, "constant values: cpu p95");
    ExpectNear(stats.cpu.p99, 42.0, 1e-9, "constant values: cpu p99");
    ExpectNear(stats.cpu.stddev, 0.0, 1e-9, "constant values: cpu stddev");
}

void TestStatsTimestampRange() {
    aura_snapshot_t snaps[3]{};
    snaps[0].timestamp = 1000.0;
    snaps[0].cpu_percent = 10.0;
    snaps[0].memory_percent = 20.0;

    snaps[1].timestamp = 1005.0;
    snaps[1].cpu_percent = 20.0;
    snaps[1].memory_percent = 30.0;

    snaps[2].timestamp = 1010.0;
    snaps[2].cpu_percent = 30.0;
    snaps[2].memory_percent = 40.0;

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps, 3, &stats, &error);
    ExpectEq(rc, AURA_OK, "timestamp range: should succeed");

    ExpectNear(stats.start_timestamp, 1000.0, 1e-9, "timestamp range: start");
    ExpectNear(stats.end_timestamp, 1010.0, 1e-9, "timestamp range: end");
    ExpectNear(stats.duration_seconds, 10.0, 1e-9, "timestamp range: duration");
}

void TestStatsCountField() {
    const int N = 7;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "count field: should succeed");
    ExpectEq(stats.count, N, "count field: matches input count");
}

void TestStatsAllMetricsPopulated() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 10.0;
    snap.memory_percent = 20.0;
    snap.disk_read_bps = 30000.0;
    snap.disk_write_bps = 40000.0;
    snap.net_recv_bps = 50000.0;
    snap.net_sent_bps = 60000.0;

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(&snap, 1, &stats, &error);
    ExpectEq(rc, AURA_OK, "all metrics: should succeed");

    ExpectNear(stats.cpu.avg, 10.0, 1e-9, "all metrics: cpu populated");
    ExpectNear(stats.memory.avg, 20.0, 1e-9, "all metrics: memory populated");
    ExpectNear(stats.disk_read.avg, 30000.0, 1e-9, "all metrics: disk_read populated");
    ExpectNear(stats.disk_write.avg, 40000.0, 1e-9, "all metrics: disk_write populated");
    ExpectNear(stats.net_recv.avg, 50000.0, 1e-9, "all metrics: net_recv populated");
    ExpectNear(stats.net_sent.avg, 60000.0, 1e-9, "all metrics: net_sent populated");
}

// ---------------------------------------------------------------------------
// Category B: Stats Engine — Percentile Correctness
// ---------------------------------------------------------------------------

void TestStatsP50EvenCount() {
    // 4 values: {10, 20, 30, 40}. P50 index = 3*0.5 = 1.5 → interpolate(20, 30, 0.5) = 25
    aura_snapshot_t snaps[4]{};
    double values[] = {10.0, 20.0, 30.0, 40.0};
    for (int i = 0; i < 4; ++i) {
        snaps[i].timestamp = 1000.0 + static_cast<double>(i);
        snaps[i].cpu_percent = values[i];
        snaps[i].memory_percent = 50.0;
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps, 4, &stats, &error);
    ExpectEq(rc, AURA_OK, "p50 even count: should succeed");
    ExpectNear(stats.cpu.p50, 25.0, 1e-9, "p50 even count: median interpolated");
}

void TestStatsP50OddCount() {
    // 5 values: {10, 20, 30, 40, 50}. P50 index = 4*0.5 = 2.0 → exact value[2] = 30
    aura_snapshot_t snaps[5]{};
    double values[] = {10.0, 20.0, 30.0, 40.0, 50.0};
    for (int i = 0; i < 5; ++i) {
        snaps[i].timestamp = 1000.0 + static_cast<double>(i);
        snaps[i].cpu_percent = values[i];
        snaps[i].memory_percent = 50.0;
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps, 5, &stats, &error);
    ExpectEq(rc, AURA_OK, "p50 odd count: should succeed");
    ExpectNear(stats.cpu.p50, 30.0, 1e-9, "p50 odd count: median is exact middle");
}

void TestStatsP95LargeInput() {
    const int N = 1000;
    std::vector<aura_snapshot_t> snaps;
    snaps.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i) / 10.0; // 0.0 to 99.9
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "p95 large: should succeed");

    // P95 of 0..99.9: index = 999 * 0.95 = 949.05, value ≈ 94.905
    ExpectTrue(stats.cpu.p95 > 90.0, "p95 large: P95 should be > 90");
    ExpectTrue(stats.cpu.p95 < 100.0, "p95 large: P95 should be < 100");
}

void TestStatsP99LargeInput() {
    const int N = 1000;
    std::vector<aura_snapshot_t> snaps;
    snaps.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i) / 10.0;
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "p99 large: should succeed");

    // P99 of 0..99.9: index = 999 * 0.99 = 989.01, value ≈ 98.901
    ExpectTrue(stats.cpu.p99 > 95.0, "p99 large: P99 should be > 95");
    ExpectTrue(stats.cpu.p99 < 100.0, "p99 large: P99 should be < 100");
}

void TestStatsPercentilesOrdered() {
    const int N = 50;
    std::vector<aura_snapshot_t> snaps;
    snaps.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 7 % 101);
        s.memory_percent = static_cast<double>(i * 13 % 97);
        s.disk_read_bps = static_cast<double>(i) * 1000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "percentiles ordered: should succeed");

    // cpu
    ExpectTrue(stats.cpu.p50 <= stats.cpu.p95, "percentiles ordered: cpu p50 <= p95");
    ExpectTrue(stats.cpu.p95 <= stats.cpu.p99, "percentiles ordered: cpu p95 <= p99");
    // memory
    ExpectTrue(stats.memory.p50 <= stats.memory.p95, "percentiles ordered: memory p50 <= p95");
    ExpectTrue(stats.memory.p95 <= stats.memory.p99, "percentiles ordered: memory p95 <= p99");
    // disk_read
    ExpectTrue(stats.disk_read.p50 <= stats.disk_read.p95, "percentiles ordered: disk_read p50 <= p95");
    ExpectTrue(stats.disk_read.p95 <= stats.disk_read.p99, "percentiles ordered: disk_read p95 <= p99");
}

void TestStatsPercentilesWithinMinMax() {
    const int N = 30;
    std::vector<aura_snapshot_t> snaps;
    snaps.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 11 % 101);
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "within min/max: should succeed");

    ExpectTrue(stats.cpu.min <= stats.cpu.p50, "within min/max: min <= p50");
    ExpectTrue(stats.cpu.p50 <= stats.cpu.p95, "within min/max: p50 <= p95");
    ExpectTrue(stats.cpu.p95 <= stats.cpu.p99, "within min/max: p95 <= p99");
    ExpectTrue(stats.cpu.p99 <= stats.cpu.max, "within min/max: p99 <= max");
}

void TestStatsPercentilesThreeValues() {
    // 3 values: {10, 50, 90}
    // P50: index = 2 * 0.5 = 1.0 → exact value[1] = 50
    // P95: index = 2 * 0.95 = 1.9 → interpolate(50, 90, 0.9) = 50 + 40*0.9 = 86
    // P99: index = 2 * 0.99 = 1.98 → interpolate(50, 90, 0.98) = 50 + 40*0.98 = 89.2
    aura_snapshot_t snaps[3]{};
    snaps[0] = {1000.0, 10.0, 50.0};
    snaps[1] = {1001.0, 50.0, 50.0};
    snaps[2] = {1002.0, 90.0, 50.0};

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps, 3, &stats, &error);
    ExpectEq(rc, AURA_OK, "three values: should succeed");
    ExpectNear(stats.cpu.p50, 50.0, 1e-9, "three values: p50");
    ExpectNear(stats.cpu.p95, 86.0, 0.1, "three values: p95");
    ExpectNear(stats.cpu.p99, 89.2, 0.1, "three values: p99");
}

// ---------------------------------------------------------------------------
// Category C: Stats Engine — Standard Deviation
// ---------------------------------------------------------------------------

void TestStatsStddevZeroForConstant() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "stddev zero: should succeed");
    ExpectNear(stats.cpu.stddev, 0.0, 1e-9, "stddev zero: constant values → 0");
}

void TestStatsStddevKnownValues() {
    // Population stddev of {2, 4, 4, 4, 5, 5, 7, 9}
    // Mean = 40/8 = 5.0
    // Variance = ((9+1+1+1+0+0+4+16)/8) = 32/8 = 4.0
    // Stddev = 2.0
    double values[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    const int N = 8;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = values[i];
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "stddev known: should succeed");
    ExpectNear(stats.cpu.stddev, 2.0, 1e-9, "stddev known: {2,4,4,4,5,5,7,9} → 2.0");
    ExpectNear(stats.cpu.avg, 5.0, 1e-9, "stddev known: avg = 5.0");
}

void TestStatsStddevSingleValue() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 42.0;
    snap.memory_percent = 50.0;

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(&snap, 1, &stats, &error);
    ExpectEq(rc, AURA_OK, "stddev single: should succeed");
    ExpectNear(stats.cpu.stddev, 0.0, 1e-9, "stddev single: n=1 → 0");
}

void TestStatsStddevLargeSpread() {
    // Alternating 0 and 100: mean=50, stddev=50
    const int N = 100;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = (i % 2 == 0) ? 0.0 : 100.0;
        s.memory_percent = 50.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(snaps.data(), N, &stats, &error);
    ExpectEq(rc, AURA_OK, "stddev large spread: should succeed");
    ExpectNear(stats.cpu.avg, 50.0, 1e-9, "stddev large spread: avg = 50");
    ExpectNear(stats.cpu.stddev, 50.0, 1e-9, "stddev large spread: stddev = 50");
}

// ---------------------------------------------------------------------------
// Category D: Stats Engine — Per-Metric Isolation
// ---------------------------------------------------------------------------

void TestStatsCpuMetricIsolated() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 10); // 0, 10, 20, ..., 90
        s.memory_percent = 50.0;
        s.disk_read_bps = 1000.0;
        s.disk_write_bps = 2000.0;
        s.net_recv_bps = 3000.0;
        s.net_sent_bps = 4000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    aura_stats_compute(snaps.data(), N, &stats, &error);

    ExpectTrue(stats.cpu.stddev > 0.0, "cpu isolated: cpu has nonzero stddev");
    ExpectNear(stats.memory.stddev, 0.0, 1e-9, "cpu isolated: memory stddev = 0");
    ExpectNear(stats.disk_read.stddev, 0.0, 1e-9, "cpu isolated: disk_read stddev = 0");
    ExpectNear(stats.disk_write.stddev, 0.0, 1e-9, "cpu isolated: disk_write stddev = 0");
    ExpectNear(stats.net_recv.stddev, 0.0, 1e-9, "cpu isolated: net_recv stddev = 0");
    ExpectNear(stats.net_sent.stddev, 0.0, 1e-9, "cpu isolated: net_sent stddev = 0");
}

void TestStatsMemoryMetricIsolated() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = static_cast<double>(i * 10);
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    aura_stats_compute(snaps.data(), N, &stats, &error);

    ExpectNear(stats.cpu.stddev, 0.0, 1e-9, "memory isolated: cpu stddev = 0");
    ExpectTrue(stats.memory.stddev > 0.0, "memory isolated: memory has nonzero stddev");
}

void TestStatsDiskReadMetricIsolated() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 50.0;
        s.disk_read_bps = static_cast<double>(i) * 100000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    aura_stats_compute(snaps.data(), N, &stats, &error);

    ExpectNear(stats.cpu.stddev, 0.0, 1e-9, "disk_read isolated: cpu stddev = 0");
    ExpectTrue(stats.disk_read.stddev > 0.0, "disk_read isolated: disk_read has nonzero stddev");
    ExpectNear(stats.disk_write.stddev, 0.0, 1e-9, "disk_read isolated: disk_write stddev = 0");
}

void TestStatsDiskWriteMetricIsolated() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 50.0;
        s.disk_write_bps = static_cast<double>(i) * 200000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    aura_stats_compute(snaps.data(), N, &stats, &error);

    ExpectNear(stats.disk_read.stddev, 0.0, 1e-9, "disk_write isolated: disk_read stddev = 0");
    ExpectTrue(stats.disk_write.stddev > 0.0, "disk_write isolated: disk_write has nonzero stddev");
}

void TestStatsNetRecvMetricIsolated() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 50.0;
        s.net_recv_bps = static_cast<double>(i) * 500000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    aura_stats_compute(snaps.data(), N, &stats, &error);

    ExpectNear(stats.net_sent.stddev, 0.0, 1e-9, "net_recv isolated: net_sent stddev = 0");
    ExpectTrue(stats.net_recv.stddev > 0.0, "net_recv isolated: net_recv has nonzero stddev");
}

void TestStatsNetSentMetricIsolated() {
    const int N = 10;
    std::vector<aura_snapshot_t> snaps;
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 50.0;
        s.net_sent_bps = static_cast<double>(i) * 300000.0;
        snaps.push_back(s);
    }

    aura_stats_result_t stats{};
    aura_error_t error{};
    aura_stats_compute(snaps.data(), N, &stats, &error);

    ExpectNear(stats.net_recv.stddev, 0.0, 1e-9, "net_sent isolated: net_recv stddev = 0");
    ExpectTrue(stats.net_sent.stddev > 0.0, "net_sent isolated: net_sent has nonzero stddev");
}

// ---------------------------------------------------------------------------
// Category E: Stats Engine — C ABI Safety
// ---------------------------------------------------------------------------

void TestStatsAbiNullSnapshots() {
    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(nullptr, 5, &stats, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null snapshots: error code");
    ExpectTrue(error.message[0] != '\0', "null snapshots: error message populated");
}

void TestStatsAbiNullOutStats() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;
    snap.memory_percent = 50.0;

    aura_error_t error{};
    const int rc = aura_stats_compute(&snap, 1, nullptr, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null out_stats: error code");
}

void TestStatsAbiNullError() {
    aura_snapshot_t snap{};
    snap.timestamp = 1000.0;
    snap.cpu_percent = 50.0;
    snap.memory_percent = 50.0;

    aura_stats_result_t stats{};
    const int rc = aura_stats_compute(&snap, 1, &stats, nullptr);
    ExpectEq(rc, AURA_OK, "null error (valid call): should succeed");
    ExpectEq(stats.count, 1, "null error (valid call): count");
}

void TestStatsAbiNegativeCount() {
    aura_snapshot_t snap{};
    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(&snap, -1, &stats, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "negative count: error code");
}

void TestStatsAbiZeroCount() {
    aura_snapshot_t snap{};
    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_stats_compute(&snap, 0, &stats, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "zero count: error code");
}

void TestStatsAbiNullErrorInvalid() {
    // Invalid call with null error — should return error code without crash
    const int rc = aura_stats_compute(nullptr, 5, nullptr, nullptr);
    ExpectTrue(rc != AURA_OK, "null error invalid: should return error code");
}

// ---------------------------------------------------------------------------
// Category F: DVR Store Integration
// ---------------------------------------------------------------------------

void TestDvrComputeStatsFullRange() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "dvr stats full: store open");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 50; ++i) {
        aura_snapshot_t snap{};
        snap.timestamp = base + static_cast<double>(i);
        snap.cpu_percent = static_cast<double>(i * 2);
        snap.memory_percent = 50.0 + static_cast<double>(i) * 0.5;
        snap.disk_read_bps = static_cast<double>(i) * 1000.0;
        rc = aura_store_append(store, &snap, &error);
        ExpectEq(rc, AURA_OK, "dvr stats full: append");
    }

    aura_stats_result_t stats{};
    rc = aura_dvr_compute_stats(store, 0, 0.0, 0, 0.0, &stats, &error);
    ExpectEq(rc, AURA_OK, "dvr stats full: compute should succeed");
    ExpectEq(stats.count, 50, "dvr stats full: count");
    ExpectTrue(stats.cpu.avg > 0.0, "dvr stats full: cpu avg > 0");
    ExpectTrue(stats.cpu.max >= stats.cpu.min, "dvr stats full: max >= min");

    aura_store_close(store);
}

void TestDvrComputeStatsSubRange() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "dvr stats sub: store open");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 50; ++i) {
        aura_snapshot_t snap{};
        snap.timestamp = base + static_cast<double>(i);
        snap.cpu_percent = static_cast<double>(i);
        snap.memory_percent = 50.0;
        rc = aura_store_append(store, &snap, &error);
        ExpectEq(rc, AURA_OK, "dvr stats sub: append");
    }

    // Query only indices 10-20 (11 snapshots)
    aura_stats_result_t stats{};
    rc = aura_dvr_compute_stats(store, 1, base + 10.0, 1, base + 20.0, &stats, &error);
    ExpectEq(rc, AURA_OK, "dvr stats sub: compute should succeed");
    ExpectEq(stats.count, 11, "dvr stats sub: count = 11");

    // CPU values 10..20, avg = 15.0
    ExpectNear(stats.cpu.avg, 15.0, 1e-9, "dvr stats sub: cpu avg = 15");
    ExpectNear(stats.cpu.min, 10.0, 1e-9, "dvr stats sub: cpu min = 10");
    ExpectNear(stats.cpu.max, 20.0, 1e-9, "dvr stats sub: cpu max = 20");

    aura_store_close(store);
}

void TestDvrComputeStatsEmptyRange() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "dvr stats empty: store open");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 10; ++i) {
        aura_snapshot_t snap{};
        snap.timestamp = base + static_cast<double>(i);
        snap.cpu_percent = static_cast<double>(i * 5);
        snap.memory_percent = 50.0;
        rc = aura_store_append(store, &snap, &error);
        ExpectEq(rc, AURA_OK, "dvr stats empty: append");
    }

    aura_stats_result_t stats{};
    // Query a range far beyond the data
    rc = aura_dvr_compute_stats(store, 1, base + 1000.0, 1, base + 2000.0, &stats, &error);
    ExpectTrue(rc != AURA_OK, "dvr stats empty range: should return error");

    aura_store_close(store);
}

void TestDvrComputeStatsNullStore() {
    aura_stats_result_t stats{};
    aura_error_t error{};
    const int rc = aura_dvr_compute_stats(nullptr, 0, 0.0, 0, 0.0, &stats, &error);
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "dvr stats null store: error code");
}
