#include "test_fakes.h"
#include "test_helpers.h"

#include <string>

using aura::telemetry::GpuSnapshot;
using aura::telemetry::NativeCollectors;
using aura::telemetry::TelemetryEngine;

int test_gpu_stub_returns_unavailable_gracefully() {
    g_gpu_status = AURA_STATUS_UNAVAILABLE;

    TelemetryEngine engine(make_collectors());
    GpuSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectGpuSnapshot(40.0, &snapshot, &error), "gpu unavailable should degrade gracefully")) {
        return 1;
    }
    if (expect(!snapshot.available, "gpu should not be available")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.gpu_percent, 0.0), "gpu percent should be zero")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.timestamp_seconds, 40.0), "gpu timestamp mismatch")) {
        return 1;
    }
    return 0;
}

int test_gpu_success_when_available() {
    g_gpu_status = AURA_STATUS_OK;
    g_gpu_data.gpu_percent = 65.0;
    g_gpu_data.vram_percent = 40.0;
    g_gpu_data.vram_used_bytes = 4000000000ULL;
    g_gpu_data.vram_total_bytes = 10000000000ULL;

    TelemetryEngine engine(make_collectors());
    GpuSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectGpuSnapshot(50.0, &snapshot, &error), "gpu available should succeed")) {
        return 1;
    }
    if (expect(snapshot.available, "gpu should be available")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.gpu_percent, 65.0), "gpu percent mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(snapshot.vram_percent, 40.0), "vram percent mismatch")) {
        return 1;
    }
    if (expect(snapshot.vram_used_bytes == 4000000000ULL, "vram used mismatch")) {
        return 1;
    }
    return 0;
}

int test_gpu_null_collector_degrades_gracefully() {
    NativeCollectors collectors = make_collectors();
    collectors.collect_gpu_utilization = nullptr;

    TelemetryEngine engine(collectors);
    GpuSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectGpuSnapshot(60.0, &snapshot, &error), "gpu null collector should degrade gracefully")) {
        return 1;
    }
    if (expect(!snapshot.available, "gpu should not be available with null collector")) {
        return 1;
    }
    return 0;
}
