#include "test_fakes.h"
#include "test_helpers.h"

#include <cstring>
#include <string>

using aura::telemetry::TelemetryEngine;
using aura::telemetry::ThermalSnapshot;

int test_thermal_degrades_gracefully_when_unavailable() {
    g_thermal_status = AURA_STATUS_UNAVAILABLE;
    g_thermal_sequence.clear();

    TelemetryEngine engine(make_collectors());
    ThermalSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectThermalSnapshot(200.0, &snapshot, &error), "thermal collection should degrade gracefully")) {
        return 1;
    }
    if (expect(snapshot.readings.empty(), "thermal readings should be empty")) {
        return 1;
    }
    if (expect(!snapshot.hottest_celsius.has_value(), "hottest should be empty")) {
        return 1;
    }
    return 0;
}

int test_thermal_success() {
    g_thermal_status = AURA_STATUS_OK;
    g_thermal_sequence.clear();

    aura_thermal_reading reading{};
    std::strncpy(reading.label, "CPU", sizeof(reading.label) - 1);
    reading.current_celsius = 70.0;
    reading.high_celsius = 90.0;
    reading.critical_celsius = 100.0;
    reading.has_high = 1;
    reading.has_critical = 1;
    g_thermal_sequence.push_back(reading);

    TelemetryEngine engine(make_collectors());
    ThermalSnapshot snapshot{};
    std::string error;
    if (expect(engine.CollectThermalSnapshot(300.0, &snapshot, &error), "thermal collection should succeed")) {
        return 1;
    }
    if (expect(snapshot.readings.size() == 1U, "expected one thermal reading")) {
        return 1;
    }
    if (expect(snapshot.readings[0].label == "CPU", "thermal label mismatch")) {
        return 1;
    }
    if (expect(snapshot.hottest_celsius.has_value(), "hottest should be present")) {
        return 1;
    }
    if (expect(nearly_equal(*snapshot.hottest_celsius, 70.0), "hottest value mismatch")) {
        return 1;
    }
    return 0;
}

int test_thermal_error_message_clears_on_graceful_paths() {
    TelemetryEngine engine(make_collectors());
    std::string error;

    if (expect(
            !engine.CollectThermalSnapshot(100.0, nullptr, &error),
            "thermal invalid output should fail"
        )) {
        return 1;
    }
    if (expect(!error.empty(), "thermal failure should populate error message")) {
        return 1;
    }

    g_thermal_status = AURA_STATUS_UNAVAILABLE;
    g_thermal_sequence.clear();
    ThermalSnapshot unavailable{};
    if (expect(
            engine.CollectThermalSnapshot(101.0, &unavailable, &error),
            "thermal unavailable should degrade gracefully"
        )) {
        return 1;
    }
    if (expect(error.empty(), "thermal unavailable path should clear stale error")) {
        return 1;
    }

    g_thermal_status = AURA_STATUS_ERROR;
    ThermalSnapshot graceful_error{};
    if (expect(
            engine.CollectThermalSnapshot(102.0, &graceful_error, &error),
            "thermal error status should degrade gracefully"
        )) {
        return 1;
    }
    if (expect(error.empty(), "thermal graceful error path should clear error")) {
        return 1;
    }

    g_thermal_status = AURA_STATUS_OK;
    g_thermal_sequence.clear();
    aura_thermal_reading reading{};
    std::strncpy(reading.label, "GPU", sizeof(reading.label) - 1);
    reading.current_celsius = 60.0;
    reading.has_high = 0;
    reading.has_critical = 0;
    g_thermal_sequence.push_back(reading);

    ThermalSnapshot success{};
    if (expect(engine.CollectThermalSnapshot(103.0, &success, &error), "thermal success should return true")) {
        return 1;
    }
    if (expect(error.empty(), "thermal success path should keep error empty")) {
        return 1;
    }
    return 0;
}
