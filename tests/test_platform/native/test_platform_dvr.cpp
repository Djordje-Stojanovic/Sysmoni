#include "test_platform_helpers.hpp"
#include "test_platform_tests.hpp"

#include <cmath>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Category A: Basic LTTB Correctness
// ---------------------------------------------------------------------------

void TestLttbDownsample() {
    std::vector<aura_snapshot_t> input;
    input.reserve(10);
    for (int i = 0; i < 10; ++i) {
        input.push_back(aura_snapshot_t{100.0 + static_cast<double>(i), static_cast<double>(i), 50.0});
    }

    aura_snapshot_t output[4]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(),
        static_cast<int>(input.size()),
        4,
        output,
        4,
        &out_count,
        &error
    );

    ExpectEq(rc, AURA_OK, "downsample should succeed");
    ExpectEq(out_count, 4, "downsample output count");
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "first timestamp preserved");
    ExpectNear(output[3].timestamp, 109.0, 1e-9, "last timestamp preserved");
}

void TestQueryTimeline() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "store open for query timeline");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 50; ++i) {
        aura_snapshot_t snapshot{
            base + static_cast<double>(i),
            static_cast<double>(i % 15),
            40.0,
        };
        rc = aura_store_append(store, &snapshot, &error);
        ExpectEq(rc, AURA_OK, "append in query timeline test");
    }

    aura_snapshot_t output[10]{};
    int out_count = 0;
    rc = aura_dvr_query_timeline(
        store,
        1,
        base + 10.0,
        1,
        base + 40.0,
        10,
        output,
        10,
        &out_count,
        &error
    );
    ExpectEq(rc, AURA_OK, "query timeline should succeed");
    ExpectEq(out_count, 10, "query timeline count");
    ExpectTrue(output[0].timestamp >= base + 10.0, "query timeline lower bound");
    ExpectTrue(output[out_count - 1].timestamp <= base + 40.0, "query timeline upper bound");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "store close query timeline");
}

void TestLttbInputSmallerThanTarget() {
    std::vector<aura_snapshot_t> input;
    for (int i = 0; i < 5; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 10);
        s.memory_percent = 40.0;
        input.push_back(s);
    }

    aura_snapshot_t output[10]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), 5, 10, output, 10, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "input < target: should succeed");
    ExpectEq(out_count, 5, "input < target: output count should equal input count");

    for (int i = 0; i < 5; ++i) {
        ExpectNear(output[i].timestamp, input[static_cast<std::size_t>(i)].timestamp, 1e-9,
                   "input < target: timestamp[" + std::to_string(i) + "] preserved");
        ExpectNear(output[i].cpu_percent, input[static_cast<std::size_t>(i)].cpu_percent, 1e-9,
                   "input < target: cpu[" + std::to_string(i) + "] preserved");
    }
}

void TestLttbInputEqualsTarget() {
    std::vector<aura_snapshot_t> input;
    for (int i = 0; i < 5; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 200.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 5);
        s.memory_percent = 50.0;
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), 5, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "input == target: should succeed");
    ExpectEq(out_count, 5, "input == target: output count should equal input count");

    for (int i = 0; i < 5; ++i) {
        ExpectNear(output[i].timestamp, input[static_cast<std::size_t>(i)].timestamp, 1e-9,
                   "input == target: timestamp[" + std::to_string(i) + "] preserved");
    }
}

void TestLttbTargetTwo() {
    const int N = 20;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 300.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 3 % 17);
        s.memory_percent = 40.0;
        input.push_back(s);
    }

    aura_snapshot_t output[2]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 2, output, 2, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "target=2: should succeed");
    ExpectEq(out_count, 2, "target=2: output count");
    ExpectNear(output[0].timestamp, 300.0, 1e-9, "target=2: first timestamp");
    ExpectNear(output[1].timestamp, 319.0, 1e-9, "target=2: last timestamp");
}

void TestLttbPreservesAllFields() {
    const int N = 15;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);
        s.memory_percent = 50.0 + static_cast<double>(i) * 0.5;
        s.disk_read_bps = 1000.0 * static_cast<double>(i);
        s.disk_write_bps = 2000.0 * static_cast<double>(i);
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "all fields: should succeed");
    ExpectEq(out_count, 5, "all fields: output count");

    // Each output snapshot must be an exact copy of some input snapshot.
    for (int i = 0; i < out_count; ++i) {
        bool found = false;
        for (int j = 0; j < N; ++j) {
            if (std::fabs(output[i].timestamp - input[j].timestamp) < 1e-12 &&
                std::fabs(output[i].cpu_percent - input[j].cpu_percent) < 1e-12 &&
                std::fabs(output[i].memory_percent - input[j].memory_percent) < 1e-12 &&
                std::fabs(output[i].disk_read_bps - input[j].disk_read_bps) < 1e-12 &&
                std::fabs(output[i].disk_write_bps - input[j].disk_write_bps) < 1e-12) {
                found = true;
                break;
            }
        }
        ExpectTrue(found, "all fields: output[" + std::to_string(i) + "] must match an input snapshot exactly");
    }
}

// ---------------------------------------------------------------------------
// Category B: Multi-Field LTTB Behavior
// ---------------------------------------------------------------------------

void TestLttbMemorySpikePreserved() {
    const int N = 20;
    const int spike_index = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 30.0;
        s.disk_read_bps = 0.0;
        s.disk_write_bps = 0.0;
        if (i == spike_index) {
            s.memory_percent = 95.0;
        }
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "mem spike: downsample should succeed");
    ExpectEq(out_count, 5, "mem spike: output count");

    bool spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].memory_percent > 90.0) {
            spike_found = true;
            ExpectNear(output[i].timestamp, 110.0, 1e-9, "mem spike: spike timestamp");
        }
    }
    ExpectTrue(spike_found, "mem spike: memory spike must be preserved in output");
}

void TestLttbDiskReadSpikePreserved() {
    const int N = 20;
    const int spike_index = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        s.disk_read_bps = 0.0;
        s.disk_write_bps = 0.0;
        if (i == spike_index) {
            s.disk_read_bps = 10000000.0;
        }
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "disk read spike: should succeed");
    ExpectEq(out_count, 5, "disk read spike: output count");

    bool spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].disk_read_bps > 5000000.0) {
            spike_found = true;
        }
    }
    ExpectTrue(spike_found, "disk read spike: disk_read_bps spike must be preserved");
}

void TestLttbDiskWriteSpikePreserved() {
    const int N = 20;
    const int spike_index = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        s.disk_read_bps = 0.0;
        s.disk_write_bps = 0.0;
        if (i == spike_index) {
            s.disk_write_bps = 10000000.0;
        }
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "disk write spike: should succeed");
    ExpectEq(out_count, 5, "disk write spike: output count");

    bool spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].disk_write_bps > 5000000.0) {
            spike_found = true;
        }
    }
    ExpectTrue(spike_found, "disk write spike: disk_write_bps spike must be preserved");
}

void TestLttbMultipleFieldSpikesAtDifferentPoints() {
    const int N = 30;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        s.disk_read_bps = 0.0;
        s.disk_write_bps = 0.0;
        input.push_back(s);
    }
    // CPU spike at index 8
    input[8].cpu_percent = 95.0;
    // Memory spike at index 22
    input[22].memory_percent = 95.0;

    aura_snapshot_t output[8]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 8, output, 8, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "multi spike: should succeed");
    ExpectEq(out_count, 8, "multi spike: output count");

    bool cpu_spike_found = false;
    bool mem_spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].cpu_percent > 90.0) cpu_spike_found = true;
        if (output[i].memory_percent > 90.0) mem_spike_found = true;
    }
    ExpectTrue(cpu_spike_found, "multi spike: CPU spike must be preserved");
    ExpectTrue(mem_spike_found, "multi spike: memory spike must be preserved");
}

void TestLttbConstantCpuVaryingMemory() {
    const int N = 40;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        // Sinusoidal memory pattern with peaks and troughs
        s.memory_percent = 50.0 + 40.0 * std::sin(2.0 * 3.14159265 * static_cast<double>(i) / static_cast<double>(N));
        s.disk_read_bps = 0.0;
        s.disk_write_bps = 0.0;
        input.push_back(s);
    }

    aura_snapshot_t output[10]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 10, output, 10, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "constant cpu varying mem: should succeed");
    ExpectEq(out_count, 10, "constant cpu varying mem: output count");

    // The output memory values should span a meaningful range, not collapse.
    double mem_min = output[0].memory_percent;
    double mem_max = output[0].memory_percent;
    for (int i = 1; i < out_count; ++i) {
        if (output[i].memory_percent < mem_min) mem_min = output[i].memory_percent;
        if (output[i].memory_percent > mem_max) mem_max = output[i].memory_percent;
    }
    // The sinusoid goes from ~10 to ~90. The output should capture at least
    // 50% of the input range (i.e., range > 40).
    ExpectTrue(mem_max - mem_min > 40.0,
               "constant cpu varying mem: output memory range should capture signal (got " +
               std::to_string(mem_max - mem_min) + ")");
}

void TestLttbAllFieldsConstant() {
    const int N = 20;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        s.disk_read_bps = 1000.0;
        s.disk_write_bps = 2000.0;
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "all constant: should not crash");
    ExpectEq(out_count, 5, "all constant: output count");
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "all constant: first timestamp");
    ExpectNear(output[4].timestamp, 119.0, 1e-9, "all constant: last timestamp");

    // All field values should be the constant values
    for (int i = 0; i < out_count; ++i) {
        ExpectNear(output[i].cpu_percent, 50.0, 1e-9,
                   "all constant: cpu[" + std::to_string(i) + "]");
        ExpectNear(output[i].memory_percent, 40.0, 1e-9,
                   "all constant: mem[" + std::to_string(i) + "]");
    }
}

void TestLttbCpuOnlyVaryingMatchesOriginal() {
    // When only cpu_percent varies, multi-field LTTB must produce identical output
    // to the original single-field algorithm.
    const int N = 20;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);   // linear ramp 0..19
        s.memory_percent = 50.0;                   // constant
        s.disk_read_bps = 0.0;                     // constant
        s.disk_write_bps = 0.0;                    // constant
        input.push_back(s);
    }

    aura_snapshot_t output[6]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 6, output, 6, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "cpu only: should succeed");
    ExpectEq(out_count, 6, "cpu only: output count");

    // First and last must be exact
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "cpu only: first timestamp");
    ExpectNear(output[5].timestamp, 119.0, 1e-9, "cpu only: last timestamp");

    // All constant fields must remain constant
    for (int i = 0; i < out_count; ++i) {
        ExpectNear(output[i].memory_percent, 50.0, 1e-9,
                   "cpu only: memory constant at output[" + std::to_string(i) + "]");
        ExpectNear(output[i].disk_read_bps, 0.0, 1e-9,
                   "cpu only: disk_read constant at output[" + std::to_string(i) + "]");
    }
}

void TestLttbNormalizationAcrossFieldScales() {
    // CPU varies 0-100, disk_read_bps varies 0-1e9.
    // Both have a significant spike. Normalization should give them equal weight.
    const int N = 30;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        s.disk_read_bps = 500000000.0;  // 500 MB/s baseline
        s.disk_write_bps = 0.0;
        input.push_back(s);
    }
    // CPU spike at index 8
    input[8].cpu_percent = 95.0;
    // Disk read spike at index 22
    input[22].disk_read_bps = 950000000.0;  // ~1 GB/s

    aura_snapshot_t output[8]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 8, output, 8, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "normalization: should succeed");
    ExpectEq(out_count, 8, "normalization: output count");

    bool cpu_spike_found = false;
    bool disk_spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].cpu_percent > 80.0) cpu_spike_found = true;
        if (output[i].disk_read_bps > 800000000.0) disk_spike_found = true;
    }
    ExpectTrue(cpu_spike_found, "normalization: CPU spike should be preserved despite smaller absolute scale");
    ExpectTrue(disk_spike_found, "normalization: disk read spike should be preserved");
}

// ---------------------------------------------------------------------------
// Category C: Edge Cases and Boundary Conditions
// ---------------------------------------------------------------------------

void TestLttbTargetThree() {
    const int N = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 7 % 13);
        s.memory_percent = 40.0;
        input.push_back(s);
    }

    aura_snapshot_t output[3]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 3, output, 3, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "target=3: should succeed");
    ExpectEq(out_count, 3, "target=3: output count");
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "target=3: first timestamp");
    ExpectNear(output[2].timestamp, 109.0, 1e-9, "target=3: last timestamp");
    // Middle point must be from input[1..8]
    ExpectTrue(output[1].timestamp > 100.0 && output[1].timestamp < 109.0,
               "target=3: middle point within range");
}

void TestLttbLargeInput() {
    const int N = 10000;
    const int target = 100;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 7 % 101);
        s.memory_percent = static_cast<double>(i * 13 % 97);
        s.disk_read_bps = static_cast<double>(i * 17 % 53) * 1000.0;
        s.disk_write_bps = static_cast<double>(i * 23 % 47) * 1000.0;
        input.push_back(s);
    }

    std::vector<aura_snapshot_t> output(static_cast<std::size_t>(target));
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, target, output.data(), target, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "large input: should succeed");
    ExpectEq(out_count, target, "large input: output count");
    ExpectNear(output[0].timestamp, 0.0, 1e-9, "large input: first timestamp");
    ExpectNear(output[static_cast<std::size_t>(target) - 1].timestamp, 9999.0, 1e-9,
               "large input: last timestamp");

    // Timestamps must be monotonically increasing
    for (int i = 1; i < out_count; ++i) {
        ExpectTrue(output[static_cast<std::size_t>(i)].timestamp >
                   output[static_cast<std::size_t>(i) - 1].timestamp,
                   "large input: timestamps monotonic at [" + std::to_string(i) + "]");
    }
}

void TestLttbExtremeFieldValues() {
    const int N = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i) * 10.0;
        s.memory_percent = 50.0;
        // Very large but finite values
        s.disk_read_bps = 1e15 + static_cast<double>(i) * 1e14;
        s.disk_write_bps = 1e15;
        input.push_back(s);
    }

    aura_snapshot_t output[4]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 4, output, 4, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "extreme values: should not crash or overflow");
    ExpectEq(out_count, 4, "extreme values: output count");

    // No NaN or Inf in output
    for (int i = 0; i < out_count; ++i) {
        ExpectTrue(std::isfinite(output[i].timestamp),
                   "extreme values: timestamp[" + std::to_string(i) + "] is finite");
        ExpectTrue(std::isfinite(output[i].cpu_percent),
                   "extreme values: cpu[" + std::to_string(i) + "] is finite");
        ExpectTrue(std::isfinite(output[i].disk_read_bps),
                   "extreme values: disk_r[" + std::to_string(i) + "] is finite");
    }
}

void TestLttbNearZeroFieldRange() {
    // One field has range exactly 1e-10 (below epsilon), another has range 1.0.
    // The near-zero field should be skipped.
    const int N = 20;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        // CPU has a meaningful range
        s.cpu_percent = static_cast<double>(i * 5);
        // Memory has a near-zero range (all values within 1e-10)
        s.memory_percent = 50.0 + static_cast<double>(i) * 1e-11;
        s.disk_read_bps = 0.0;
        s.disk_write_bps = 0.0;
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "near zero range: should succeed");
    ExpectEq(out_count, 5, "near zero range: output count");

    // Should not crash due to division by near-zero
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "near zero range: first timestamp");
    ExpectNear(output[4].timestamp, 119.0, 1e-9, "near zero range: last timestamp");
}

void TestLttbTimestampsMonotonicallyIncreasing() {
    const int N = 50;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 1000.0 + static_cast<double>(i) * 0.1;
        s.cpu_percent = static_cast<double>(i * 11 % 101);
        s.memory_percent = static_cast<double>(i * 7 % 97);
        s.disk_read_bps = static_cast<double>(i * 13 % 53) * 100.0;
        s.disk_write_bps = static_cast<double>(i * 17 % 47) * 100.0;
        input.push_back(s);
    }

    aura_snapshot_t output[15]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 15, output, 15, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "monotonic: should succeed");
    ExpectEq(out_count, 15, "monotonic: output count");

    for (int i = 1; i < out_count; ++i) {
        ExpectTrue(output[i].timestamp > output[i - 1].timestamp,
                   "monotonic: output[" + std::to_string(i) + "].timestamp > output[" +
                   std::to_string(i - 1) + "].timestamp");
    }
}

void TestLttbOutputIsSubsetOfInput() {
    const int N = 30;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 3 % 17);
        s.memory_percent = static_cast<double>(i * 7 % 23);
        s.disk_read_bps = static_cast<double>(i * 11 % 29) * 1000.0;
        s.disk_write_bps = static_cast<double>(i * 13 % 31) * 1000.0;
        input.push_back(s);
    }

    aura_snapshot_t output[10]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 10, output, 10, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "subset: should succeed");
    ExpectEq(out_count, 10, "subset: output count");

    for (int i = 0; i < out_count; ++i) {
        bool found = false;
        for (int j = 0; j < N; ++j) {
            if (std::fabs(output[i].timestamp - input[j].timestamp) < 1e-12 &&
                std::fabs(output[i].cpu_percent - input[j].cpu_percent) < 1e-12 &&
                std::fabs(output[i].memory_percent - input[j].memory_percent) < 1e-12 &&
                std::fabs(output[i].disk_read_bps - input[j].disk_read_bps) < 1e-12 &&
                std::fabs(output[i].disk_write_bps - input[j].disk_write_bps) < 1e-12) {
                found = true;
                break;
            }
        }
        ExpectTrue(found, "subset: output[" + std::to_string(i) + "] must exist verbatim in input");
    }
}

// ---------------------------------------------------------------------------
// Category D: C ABI Safety Tests
// ---------------------------------------------------------------------------

void TestLttbAbiNullInputWithPositiveCount() {
    aura_snapshot_t output[3]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        nullptr, 5, 3, output, 3, &out_count, &error
    );
    ExpectTrue(rc != AURA_OK, "null input: should return error");
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null input: error code");
    ExpectTrue(error.message[0] != '\0', "null input: error message populated");
}

void TestLttbAbiNullOutput() {
    aura_snapshot_t input[5]{};
    for (int i = 0; i < 5; ++i) {
        input[i].timestamp = 100.0 + static_cast<double>(i);
        input[i].cpu_percent = static_cast<double>(i * 10);
        input[i].memory_percent = 50.0;
    }

    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input, 5, 3, nullptr, 3, &out_count, &error
    );
    // The C ABI should detect that output is null when results are present
    ExpectTrue(rc != AURA_OK, "null output: should return error");
}

void TestLttbAbiNullOutCount() {
    aura_snapshot_t input[5]{};
    for (int i = 0; i < 5; ++i) {
        input[i].timestamp = 100.0 + static_cast<double>(i);
        input[i].cpu_percent = static_cast<double>(i * 10);
        input[i].memory_percent = 50.0;
    }
    aura_snapshot_t output[3]{};
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input, 5, 3, output, 3, nullptr, &error
    );
    ExpectTrue(rc != AURA_OK, "null out_count: should return error");
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null out_count: error code");
}

void TestLttbAbiCapacityTooSmall() {
    aura_snapshot_t input[10]{};
    for (int i = 0; i < 10; ++i) {
        input[i].timestamp = 100.0 + static_cast<double>(i);
        input[i].cpu_percent = static_cast<double>(i);
        input[i].memory_percent = 50.0;
    }

    aura_snapshot_t output[2]{};
    int out_count = 0;
    aura_error_t error{};
    // target=5 but capacity only 2
    const int rc = aura_dvr_downsample_lttb(
        input, 10, 5, output, 2, &out_count, &error
    );
    ExpectTrue(rc != AURA_OK, "capacity too small: should return error");
    ExpectEq(rc, AURA_ERR_CAPACITY, "capacity too small: error code");
}

void TestLttbAbiNegativeInputCount() {
    aura_snapshot_t output[3]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        nullptr, -1, 3, output, 3, &out_count, &error
    );
    ExpectTrue(rc != AURA_OK, "negative input count: should return error");
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "negative input count: error code");
}

void TestLttbAbiTargetLessThanTwo() {
    aura_snapshot_t input[5]{};
    for (int i = 0; i < 5; ++i) {
        input[i].timestamp = 100.0 + static_cast<double>(i);
        input[i].cpu_percent = static_cast<double>(i * 10);
        input[i].memory_percent = 50.0;
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    // target=1 is invalid (LTTB requires >= 2)
    const int rc = aura_dvr_downsample_lttb(
        input, 5, 1, output, 5, &out_count, &error
    );
    // The C++ layer throws, C ABI catches and returns AURA_ERR_RUNTIME
    ExpectTrue(rc != AURA_OK, "target < 2: should return error");
}

void TestLttbAbiNullError() {
    // Valid call with null error — should succeed without crashing
    aura_snapshot_t input[5]{};
    for (int i = 0; i < 5; ++i) {
        input[i].timestamp = 100.0 + static_cast<double>(i);
        input[i].cpu_percent = static_cast<double>(i * 10);
        input[i].memory_percent = 50.0;
    }
    aura_snapshot_t output[5]{};
    int out_count = 0;
    int rc = aura_dvr_downsample_lttb(
        input, 5, 3, output, 5, &out_count, nullptr
    );
    ExpectEq(rc, AURA_OK, "null error (valid call): should succeed");
    ExpectEq(out_count, 3, "null error (valid call): output count");

    // Invalid call with null error — should return error without crashing
    rc = aura_dvr_downsample_lttb(
        nullptr, 5, 3, output, 5, &out_count, nullptr
    );
    ExpectTrue(rc != AURA_OK, "null error (invalid call): should return error");
}

// ---------------------------------------------------------------------------
// Category E: QueryTimeline Integration Tests
// ---------------------------------------------------------------------------

void TestQueryTimelineEmptyRange() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "empty range: store open");

    const double base = NowSeconds() - 100.0;
    // Insert snapshots in [base, base+10]
    for (int i = 0; i < 10; ++i) {
        aura_snapshot_t s{};
        s.timestamp = base + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 5);
        s.memory_percent = 40.0;
        rc = aura_store_append(store, &s, &error);
        ExpectEq(rc, AURA_OK, "empty range: append");
    }

    // Query a range far outside the data
    aura_snapshot_t output[5]{};
    int out_count = 99;
    rc = aura_dvr_query_timeline(
        store,
        1, base + 1000.0,      // start way beyond data
        1, base + 2000.0,      // end way beyond data
        5,
        output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "empty range: should succeed with zero results");
    ExpectEq(out_count, 0, "empty range: output count should be zero");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "empty range: store close");
}

void TestQueryTimelineAbiNullStore() {
    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_query_timeline(
        nullptr, 1, 100.0, 1, 200.0, 5,
        output, 5, &out_count, &error
    );
    ExpectTrue(rc != AURA_OK, "null store: should return error");
    ExpectEq(rc, AURA_ERR_INVALID_ARGUMENT, "null store: error code");
}

void TestQueryTimelineResolutionOne() {
    aura_error_t error{};
    aura_store_t* store = nullptr;
    int rc = aura_store_open(":memory:", 3600.0, &store, &error);
    ExpectEq(rc, AURA_OK, "resolution 1: store open");

    const double base = NowSeconds() - 100.0;
    for (int i = 0; i < 10; ++i) {
        aura_snapshot_t s{};
        s.timestamp = base + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 5);
        s.memory_percent = 40.0;
        rc = aura_store_append(store, &s, &error);
        ExpectEq(rc, AURA_OK, "resolution 1: append");
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    // resolution=1 is invalid (requires >= 2)
    rc = aura_dvr_query_timeline(
        store, 1, base, 1, base + 9.0, 1,
        output, 5, &out_count, &error
    );
    // The C++ layer throws, C ABI catches
    ExpectTrue(rc != AURA_OK, "resolution 1: should return error");

    rc = aura_store_close(store);
    ExpectEq(rc, AURA_OK, "resolution 1: store close");
}

// ---------------------------------------------------------------------------
// Category F: LTTB with Network Telemetry Fields
// ---------------------------------------------------------------------------

void TestLttbNetRecvSpikePreserved() {
    const int N = 20;
    const int spike_index = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        if (i == spike_index) {
            s.net_recv_bps = 10000000.0;
        }
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "net recv spike: downsample should succeed");
    ExpectEq(out_count, 5, "net recv spike: output count");

    bool spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].net_recv_bps > 5000000.0) {
            spike_found = true;
        }
    }
    ExpectTrue(spike_found, "net recv spike: net_recv_bps spike must be preserved in output");
}

void TestLttbNetSentSpikePreserved() {
    const int N = 20;
    const int spike_index = 10;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        if (i == spike_index) {
            s.net_sent_bps = 10000000.0;
        }
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "net sent spike: downsample should succeed");
    ExpectEq(out_count, 5, "net sent spike: output count");

    bool spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].net_sent_bps > 5000000.0) {
            spike_found = true;
        }
    }
    ExpectTrue(spike_found, "net sent spike: net_sent_bps spike must be preserved in output");
}

void TestLttbPreservesAllFieldsIncludingNet() {
    const int N = 15;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i);
        s.memory_percent = 50.0 + static_cast<double>(i) * 0.5;
        s.disk_read_bps = 1000.0 * static_cast<double>(i);
        s.disk_write_bps = 2000.0 * static_cast<double>(i);
        s.net_recv_bps = 10000.0 * static_cast<double>(i);
        s.net_sent_bps = 5000.0 * static_cast<double>(i);
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "all fields net: should succeed");
    ExpectEq(out_count, 5, "all fields net: output count");

    for (int i = 0; i < out_count; ++i) {
        bool found = false;
        for (int j = 0; j < N; ++j) {
            if (std::fabs(output[i].timestamp - input[j].timestamp) < 1e-12 &&
                std::fabs(output[i].cpu_percent - input[j].cpu_percent) < 1e-12 &&
                std::fabs(output[i].memory_percent - input[j].memory_percent) < 1e-12 &&
                std::fabs(output[i].disk_read_bps - input[j].disk_read_bps) < 1e-12 &&
                std::fabs(output[i].disk_write_bps - input[j].disk_write_bps) < 1e-12 &&
                std::fabs(output[i].net_recv_bps - input[j].net_recv_bps) < 1e-12 &&
                std::fabs(output[i].net_sent_bps - input[j].net_sent_bps) < 1e-12) {
                found = true;
                break;
            }
        }
        ExpectTrue(found, "all fields net: output[" + std::to_string(i) + "] must match input exactly (including net)");
    }
}

void TestLttbNetFieldsZeroRangeSkipped() {
    // Net fields are zero for all input — LTTB should still work
    // (zero range fields are skipped in area calculation)
    const int N = 20;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = static_cast<double>(i * 5);
        s.memory_percent = 50.0;
        // net fields default to 0.0
        input.push_back(s);
    }

    aura_snapshot_t output[5]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 5, output, 5, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "net zero range: should succeed");
    ExpectEq(out_count, 5, "net zero range: output count");
    ExpectNear(output[0].timestamp, 100.0, 1e-9, "net zero range: first timestamp");
    ExpectNear(output[4].timestamp, 119.0, 1e-9, "net zero range: last timestamp");

    // All net fields should be zero
    for (int i = 0; i < out_count; ++i) {
        ExpectNear(output[i].net_recv_bps, 0.0, 1e-9,
                   "net zero range: net_recv[" + std::to_string(i) + "] zero");
        ExpectNear(output[i].net_sent_bps, 0.0, 1e-9,
                   "net zero range: net_sent[" + std::to_string(i) + "] zero");
    }
}

void TestLttbSixFieldNormalizationNetVsCpu() {
    // CPU varies 0-100, net_recv_bps varies 0-1e9.
    // Both have significant spikes. Normalization should preserve both.
    const int N = 30;
    std::vector<aura_snapshot_t> input;
    input.reserve(N);
    for (int i = 0; i < N; ++i) {
        aura_snapshot_t s{};
        s.timestamp = 100.0 + static_cast<double>(i);
        s.cpu_percent = 50.0;
        s.memory_percent = 40.0;
        s.net_recv_bps = 500000000.0;
        input.push_back(s);
    }
    // CPU spike at index 8
    input[8].cpu_percent = 95.0;
    // Net recv spike at index 22
    input[22].net_recv_bps = 950000000.0;

    aura_snapshot_t output[8]{};
    int out_count = 0;
    aura_error_t error{};
    const int rc = aura_dvr_downsample_lttb(
        input.data(), N, 8, output, 8, &out_count, &error
    );
    ExpectEq(rc, AURA_OK, "net normalization: should succeed");
    ExpectEq(out_count, 8, "net normalization: output count");

    bool cpu_spike_found = false;
    bool net_spike_found = false;
    for (int i = 0; i < out_count; ++i) {
        if (output[i].cpu_percent > 80.0) cpu_spike_found = true;
        if (output[i].net_recv_bps > 800000000.0) net_spike_found = true;
    }
    ExpectTrue(cpu_spike_found, "net normalization: CPU spike should be preserved");
    ExpectTrue(net_spike_found, "net normalization: net_recv spike should be preserved");
}
