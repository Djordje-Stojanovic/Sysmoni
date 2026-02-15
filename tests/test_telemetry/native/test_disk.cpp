#include "test_fakes.h"
#include "test_helpers.h"

#include <string>

using aura::telemetry::DiskSnapshot;
using aura::telemetry::TelemetryEngine;

int test_disk_rate_computation() {
    g_disk_status = AURA_STATUS_OK;
    g_disk_index = 0;
    g_disk_sequence = {
        aura_disk_counters{1000, 2000, 10, 20},
        aura_disk_counters{3000, 5000, 30, 50},
    };

    TelemetryEngine engine(make_collectors());
    DiskSnapshot first{};
    DiskSnapshot second{};
    std::string error;
    if (expect(engine.CollectDiskSnapshot(100.0, &first, &error), "first disk sample should succeed")) {
        return 1;
    }
    if (expect(engine.CollectDiskSnapshot(102.0, &second, &error), "second disk sample should succeed")) {
        return 1;
    }
    if (expect(nearly_equal(first.read_bytes_per_sec, 0.0), "first disk read rate should be zero")) {
        return 1;
    }
    if (expect(nearly_equal(second.read_bytes_per_sec, 1000.0), "disk read rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(second.write_bytes_per_sec, 1500.0), "disk write rate mismatch")) {
        return 1;
    }
    return 0;
}

int test_disk_non_increasing_timestamp_keeps_baseline() {
    g_disk_status = AURA_STATUS_OK;
    g_disk_index = 0;
    g_disk_sequence = {
        aura_disk_counters{1000, 2000, 10, 20},
        aura_disk_counters{2000, 3000, 20, 30},
        aura_disk_counters{5000, 7000, 50, 80},
    };

    TelemetryEngine engine(make_collectors());
    DiskSnapshot first{};
    DiskSnapshot non_increasing{};
    DiskSnapshot resumed{};
    std::string error;
    if (expect(engine.CollectDiskSnapshot(100.0, &first, &error), "disk baseline sample should succeed")) {
        return 1;
    }
    if (expect(
            engine.CollectDiskSnapshot(99.0, &non_increasing, &error),
            "disk non-increasing timestamp sample should succeed"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(non_increasing.read_bytes_per_sec, 0.0),
            "non-increasing disk read rate should stay zero"
        )) {
        return 1;
    }
    if (expect(
            engine.CollectDiskSnapshot(102.0, &resumed, &error),
            "disk resumed sample should succeed"
        )) {
        return 1;
    }
    if (expect(nearly_equal(resumed.read_bytes_per_sec, 2000.0), "disk resumed read rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.write_bytes_per_sec, 2500.0), "disk resumed write rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.read_ops_per_sec, 20.0), "disk resumed read ops mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.write_ops_per_sec, 30.0), "disk resumed write ops mismatch")) {
        return 1;
    }
    return 0;
}

int test_disk_unavailable_degrades_gracefully() {
    g_disk_status = AURA_STATUS_UNAVAILABLE;
    g_disk_index = 0;
    g_disk_sequence = {aura_disk_counters{500, 700, 5, 7}};

    TelemetryEngine engine(make_collectors());
    DiskSnapshot unavailable{};
    std::string error;
    if (expect(
            engine.CollectDiskSnapshot(100.0, &unavailable, &error),
            "disk unavailable should degrade gracefully"
        )) {
        return 1;
    }
    if (expect(unavailable.total_read_bytes == 0U, "disk unavailable read total should be zero")) {
        return 1;
    }
    if (expect(
            unavailable.total_write_bytes == 0U,
            "disk unavailable write total should be zero"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(unavailable.read_bytes_per_sec, 0.0),
            "disk unavailable read rate should be zero"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(unavailable.write_bytes_per_sec, 0.0),
            "disk unavailable write rate should be zero"
        )) {
        return 1;
    }

    g_disk_status = AURA_STATUS_OK;
    g_disk_index = 0;
    g_disk_sequence = {aura_disk_counters{500, 700, 5, 7}};
    DiskSnapshot recovered{};
    if (expect(
            engine.CollectDiskSnapshot(101.0, &recovered, &error),
            "disk collection should recover after unavailable"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(recovered.read_bytes_per_sec, 0.0),
            "first recovered disk sample should keep zero rate"
        )) {
        return 1;
    }
    if (expect(recovered.total_read_bytes == 500U, "disk recovered read total mismatch")) {
        return 1;
    }
    return 0;
}

int test_disk_error_message_clears_on_graceful_paths() {
    g_disk_status = AURA_STATUS_ERROR;
    g_disk_index = 0;
    g_disk_sequence.clear();

    TelemetryEngine engine(make_collectors());
    DiskSnapshot failed{};
    std::string error;
    if (expect(!engine.CollectDiskSnapshot(100.0, &failed, &error), "disk error should fail")) {
        return 1;
    }
    if (expect(!error.empty(), "disk error should populate error message")) {
        return 1;
    }

    g_disk_status = AURA_STATUS_UNAVAILABLE;
    g_disk_sequence = {aura_disk_counters{10, 20, 1, 2}};
    DiskSnapshot unavailable{};
    if (expect(
            engine.CollectDiskSnapshot(101.0, &unavailable, &error),
            "disk unavailable should return success"
        )) {
        return 1;
    }
    if (expect(error.empty(), "disk unavailable path should clear stale error")) {
        return 1;
    }

    g_disk_status = AURA_STATUS_OK;
    g_disk_index = 0;
    g_disk_sequence = {aura_disk_counters{50, 70, 5, 7}};
    DiskSnapshot recovered{};
    if (expect(engine.CollectDiskSnapshot(102.0, &recovered, &error), "disk success should return true")) {
        return 1;
    }
    if (expect(error.empty(), "disk success path should keep error empty")) {
        return 1;
    }
    return 0;
}

int test_disk_error_still_fails() {
    g_disk_status = AURA_STATUS_ERROR;
    g_disk_index = 0;
    g_disk_sequence.clear();

    TelemetryEngine engine(make_collectors());
    DiskSnapshot snapshot{};
    std::string error;
    if (expect(
            !engine.CollectDiskSnapshot(100.0, &snapshot, &error),
            "disk error should fail"
        )) {
        return 1;
    }
    if (expect(
            error.find("collect_disk_counters failed") != std::string::npos,
            "disk error message should include collector failure context"
        )) {
        return 1;
    }
    return 0;
}
