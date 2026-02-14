#include "telemetry_engine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using aura::telemetry::DiskSnapshot;
using aura::telemetry::NativeCollectors;
using aura::telemetry::NetworkSnapshot;
using aura::telemetry::ProcessSample;
using aura::telemetry::SystemSnapshot;
using aura::telemetry::TelemetryEngine;
using aura::telemetry::ThermalSnapshot;

int g_system_status = AURA_STATUS_OK;
double g_system_cpu_percent = 12.5;
double g_system_memory_percent = 42.0;

int g_process_status = AURA_STATUS_OK;
std::vector<aura_process_sample> g_process_samples;

int g_disk_status = AURA_STATUS_OK;
std::vector<aura_disk_counters> g_disk_sequence;
size_t g_disk_index = 0;

int g_network_status = AURA_STATUS_OK;
std::vector<aura_network_counters> g_network_sequence;
size_t g_network_index = 0;

int g_thermal_status = AURA_STATUS_UNAVAILABLE;
std::vector<aura_thermal_reading> g_thermal_sequence;

void write_error(char* error_buffer, size_t error_buffer_len, const char* message) {
    if (error_buffer == nullptr || error_buffer_len == 0) {
        return;
    }
    error_buffer[0] = '\0';
    if (message == nullptr || message[0] == '\0') {
        return;
    }
    std::strncpy(error_buffer, message, error_buffer_len - 1);
    error_buffer[error_buffer_len - 1] = '\0';
}

int fake_collect_system_snapshot(
    double* cpu_percent,
    double* memory_percent,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_system_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "system failed");
        return g_system_status;
    }
    *cpu_percent = g_system_cpu_percent;
    *memory_percent = g_system_memory_percent;
    return AURA_STATUS_OK;
}

int fake_collect_processes(
    aura_process_sample* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_process_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "process failed");
        if (out_count != nullptr) {
            *out_count = 0;
        }
        return g_process_status;
    }
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(g_process_samples.size(), max_samples));
    for (uint32_t i = 0; i < count; ++i) {
        samples[i] = g_process_samples[i];
    }
    if (out_count != nullptr) {
        *out_count = count;
    }
    return AURA_STATUS_OK;
}

int fake_collect_disk_counters(
    aura_disk_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_disk_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "disk failed");
        return g_disk_status;
    }
    if (g_disk_sequence.empty()) {
        write_error(error_buffer, error_buffer_len, "disk sequence empty");
        return AURA_STATUS_ERROR;
    }
    const size_t index = std::min(g_disk_index, g_disk_sequence.size() - 1U);
    *counters = g_disk_sequence[index];
    ++g_disk_index;
    return AURA_STATUS_OK;
}

int fake_collect_network_counters(
    aura_network_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_network_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "network failed");
        return g_network_status;
    }
    if (g_network_sequence.empty()) {
        write_error(error_buffer, error_buffer_len, "network sequence empty");
        return AURA_STATUS_ERROR;
    }
    const size_t index = std::min(g_network_index, g_network_sequence.size() - 1U);
    *counters = g_network_sequence[index];
    ++g_network_index;
    return AURA_STATUS_OK;
}

int fake_collect_thermal_readings(
    aura_thermal_reading* readings,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_thermal_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "thermal unavailable");
        if (out_count != nullptr) {
            *out_count = 0;
        }
        return g_thermal_status;
    }

    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(g_thermal_sequence.size(), max_samples));
    for (uint32_t i = 0; i < count; ++i) {
        readings[i] = g_thermal_sequence[i];
    }
    if (out_count != nullptr) {
        *out_count = count;
    }
    return AURA_STATUS_OK;
}

NativeCollectors make_collectors() {
    NativeCollectors collectors{};
    collectors.collect_system_snapshot = fake_collect_system_snapshot;
    collectors.collect_processes = fake_collect_processes;
    collectors.collect_disk_counters = fake_collect_disk_counters;
    collectors.collect_network_counters = fake_collect_network_counters;
    collectors.collect_thermal_readings = fake_collect_thermal_readings;
    return collectors;
}

bool nearly_equal(double left, double right, double tolerance = 1e-6) {
    return std::fabs(left - right) <= tolerance;
}

int expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
    return 0;
}

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

int test_process_sort_and_limit() {
    g_process_status = AURA_STATUS_OK;
    g_process_samples.clear();

    aura_process_sample a{};
    a.pid = 10;
    std::strncpy(a.name, "alpha", sizeof(a.name) - 1);
    a.cpu_percent = 20.0;
    a.memory_rss_bytes = 1000;

    aura_process_sample b{};
    b.pid = 5;
    std::strncpy(b.name, "beta", sizeof(b.name) - 1);
    b.cpu_percent = 20.0;
    b.memory_rss_bytes = 3000;

    aura_process_sample c{};
    c.pid = 20;
    std::strncpy(c.name, "gamma", sizeof(c.name) - 1);
    c.cpu_percent = 40.0;
    c.memory_rss_bytes = 500;

    g_process_samples = {a, b, c};

    TelemetryEngine engine(make_collectors());
    std::vector<ProcessSample> samples;
    std::string error;
    if (expect(engine.CollectTopProcesses(2, &samples, &error), "process collection should succeed")) {
        return 1;
    }
    if (expect(samples.size() == 2U, "expected 2 top processes")) {
        return 1;
    }
    if (expect(samples[0].pid == 20U, "top process should be pid 20")) {
        return 1;
    }
    if (expect(samples[1].pid == 5U, "second process should be pid 5")) {
        return 1;
    }
    return 0;
}

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

int test_network_rate_computation() {
    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {
        aura_network_counters{1000, 2000, 10, 20},
        aura_network_counters{4000, 5000, 40, 80},
    };

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot first{};
    NetworkSnapshot second{};
    std::string error;
    if (expect(engine.CollectNetworkSnapshot(100.0, &first, &error), "first network sample should succeed")) {
        return 1;
    }
    if (expect(engine.CollectNetworkSnapshot(103.0, &second, &error), "second network sample should succeed")) {
        return 1;
    }
    if (expect(nearly_equal(first.bytes_sent_per_sec, 0.0), "first network send rate should be zero")) {
        return 1;
    }
    if (expect(nearly_equal(second.bytes_sent_per_sec, 1000.0), "network send rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(second.packets_recv_per_sec, 20.0), "network recv packet rate mismatch")) {
        return 1;
    }
    return 0;
}

int test_network_unavailable_degrades_gracefully() {
    g_network_status = AURA_STATUS_UNAVAILABLE;
    g_network_index = 0;
    g_network_sequence = {aura_network_counters{700, 900, 11, 13}};

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot unavailable{};
    std::string error;
    if (expect(
            engine.CollectNetworkSnapshot(100.0, &unavailable, &error),
            "network unavailable should degrade gracefully"
        )) {
        return 1;
    }
    if (expect(
            unavailable.total_bytes_sent == 0U,
            "network unavailable sent total should be zero"
        )) {
        return 1;
    }
    if (expect(
            unavailable.total_bytes_recv == 0U,
            "network unavailable recv total should be zero"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(unavailable.bytes_sent_per_sec, 0.0),
            "network unavailable send rate should be zero"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(unavailable.bytes_recv_per_sec, 0.0),
            "network unavailable recv rate should be zero"
        )) {
        return 1;
    }

    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {aura_network_counters{700, 900, 11, 13}};
    NetworkSnapshot recovered{};
    if (expect(
            engine.CollectNetworkSnapshot(101.0, &recovered, &error),
            "network collection should recover after unavailable"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(recovered.bytes_sent_per_sec, 0.0),
            "first recovered network sample should keep zero rate"
        )) {
        return 1;
    }
    if (expect(recovered.total_bytes_sent == 700U, "network recovered sent total mismatch")) {
        return 1;
    }
    return 0;
}

int test_network_error_still_fails() {
    g_network_status = AURA_STATUS_ERROR;
    g_network_index = 0;
    g_network_sequence.clear();

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot snapshot{};
    std::string error;
    if (expect(
            !engine.CollectNetworkSnapshot(100.0, &snapshot, &error),
            "network error should fail"
        )) {
        return 1;
    }
    if (expect(
            error.find("collect_network_counters failed") != std::string::npos,
            "network error message should include collector failure context"
        )) {
        return 1;
    }
    return 0;
}

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

}  // namespace

int main() {
    using TestCase = int (*)();
    const std::vector<TestCase> tests = {
        test_system_snapshot_success,
        test_process_sort_and_limit,
        test_disk_rate_computation,
        test_disk_unavailable_degrades_gracefully,
        test_disk_error_still_fails,
        test_network_rate_computation,
        test_network_unavailable_degrades_gracefully,
        test_network_error_still_fails,
        test_thermal_degrades_gracefully_when_unavailable,
        test_thermal_success,
    };

    int failures = 0;
    for (const TestCase test : tests) {
        failures += test();
    }

    if (failures == 0) {
        std::cout << "PASS: telemetry native tests\n";
        return 0;
    }

    std::cerr << "FAIL: telemetry native tests (" << failures << " failing cases)\n";
    return 1;
}
