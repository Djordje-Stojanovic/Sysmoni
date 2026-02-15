#include "test_fakes.h"
#include "test_helpers.h"

#include <string>

using aura::telemetry::SystemSnapshot;
using aura::telemetry::TelemetryEngine;

int test_system_snapshot_success() {
    g_system_status = AURA_STATUS_OK;
    g_system_cpu_percent = 55.5;
    g_system_memory_percent = 33.3;

    TelemetryEngine engine(make_collectors());
    SystemSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectSystemSnapshot(10.0, &snapshot, &error), "system snapshot should succeed")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.timestamp_seconds, 10.0), "timestamp mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.cpu_percent, 55.5), "cpu mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.memory_percent, 33.3), "memory mismatch")) {
        return 1;
    }
    return 0;
}

int test_system_error_message_clears_on_success_path() {
    g_system_status = AURA_STATUS_ERROR;

    TelemetryEngine engine(make_collectors());
    SystemSnapshot failed_snapshot{};
    std::string error;
    if (expect(
            !engine.CollectSystemSnapshot(9.0, &failed_snapshot, &error),
            "system failure should return false"
        )) {
        return 1;
    }
    if (expect(!error.empty(), "system failure should populate error message")) {
        return 1;
    }

    g_system_status = AURA_STATUS_OK;
    g_system_cpu_percent = 11.0;
    g_system_memory_percent = 22.0;
    SystemSnapshot success_snapshot{};
    if (expect(
            engine.CollectSystemSnapshot(10.0, &success_snapshot, &error),
            "system success should return true"
        )) {
        return 1;
    }
    if (expect(error.empty(), "system success should clear stale error")) {
        return 1;
    }
    return 0;
}
