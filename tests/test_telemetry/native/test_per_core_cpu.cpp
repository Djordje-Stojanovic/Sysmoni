#include "test_fakes.h"
#include "test_helpers.h"

#include <string>

using aura::telemetry::NativeCollectors;
using aura::telemetry::PerCoreCpuSnapshot;
using aura::telemetry::TelemetryEngine;

int test_per_core_cpu_success() {
    g_per_core_cpu_status = AURA_STATUS_OK;
    g_per_core_cpu_percents = {25.0, 50.0, 75.0, 100.0};

    TelemetryEngine engine(make_collectors());
    PerCoreCpuSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectPerCoreCpu(10.0, &snapshot, &error), "per-core cpu should succeed")) {
        return 1;
    }
    if (expect(snapshot.core_percents.size() == 4U, "expected 4 cores")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.core_percents[0], 25.0), "core 0 mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.core_percents[2], 75.0), "core 2 mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.timestamp_seconds, 10.0), "per-core timestamp mismatch")) {
        return 1;
    }
    return 0;
}

int test_per_core_cpu_unavailable_degrades_gracefully() {
    g_per_core_cpu_status = AURA_STATUS_UNAVAILABLE;
    g_per_core_cpu_percents.clear();

    TelemetryEngine engine(make_collectors());
    PerCoreCpuSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectPerCoreCpu(20.0, &snapshot, &error), "per-core unavailable should degrade gracefully")) {
        return 1;
    }
    if (expect(snapshot.core_percents.empty(), "per-core readings should be empty when unavailable")) {
        return 1;
    }
    return 0;
}

int test_per_core_cpu_null_collector_degrades_gracefully() {
    NativeCollectors collectors = make_collectors();
    collectors.collect_per_core_cpu = nullptr;

    TelemetryEngine engine(collectors);
    PerCoreCpuSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectPerCoreCpu(30.0, &snapshot, &error), "per-core null collector should degrade gracefully")) {
        return 1;
    }
    if (expect(snapshot.core_percents.empty(), "per-core should be empty with null collector")) {
        return 1;
    }
    return 0;
}
