#include "telemetry_engine.h"

#include <algorithm>
#include <array>
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
using aura::telemetry::PerCoreCpuSnapshot;
using aura::telemetry::GpuSnapshot;

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

int g_per_core_cpu_status = AURA_STATUS_UNAVAILABLE;
std::vector<double> g_per_core_cpu_percents;

int g_gpu_status = AURA_STATUS_UNAVAILABLE;
aura_gpu_utilization g_gpu_data{};

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

int fake_collect_per_core_cpu(
    double* out_percents,
    uint32_t max_cores,
    uint32_t* out_core_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_per_core_cpu_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "per-core cpu unavailable");
        if (out_core_count != nullptr) {
            *out_core_count = 0;
        }
        return g_per_core_cpu_status;
    }
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(g_per_core_cpu_percents.size(), max_cores));
    for (uint32_t i = 0; i < count; ++i) {
        out_percents[i] = g_per_core_cpu_percents[i];
    }
    if (out_core_count != nullptr) {
        *out_core_count = count;
    }
    return AURA_STATUS_OK;
}

int fake_collect_gpu_utilization(
    aura_gpu_utilization* out_gpu,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_gpu_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "gpu unavailable");
        return g_gpu_status;
    }
    if (out_gpu != nullptr) {
        *out_gpu = g_gpu_data;
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
    collectors.collect_per_core_cpu = fake_collect_per_core_cpu;
    collectors.collect_gpu_utilization = fake_collect_gpu_utilization;
    return collectors;
}

bool nearly_equal(double left, double right, double tolerance = 1e-6) {
    return std::fabs(left - right) <= tolerance;
}

bool process_ranks_before(
    const aura_process_sample& left,
    const aura_process_sample& right
) {
    if (left.cpu_percent != right.cpu_percent) {
        return left.cpu_percent > right.cpu_percent;
    }
    if (left.memory_rss_bytes != right.memory_rss_bytes) {
        return left.memory_rss_bytes > right.memory_rss_bytes;
    }
    return left.pid < right.pid;
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

int test_process_error_message_clears_on_success_path() {
    g_process_status = AURA_STATUS_ERROR;
    g_process_samples.clear();

    TelemetryEngine engine(make_collectors());
    std::vector<ProcessSample> failed_samples;
    std::string error;
    if (expect(
            !engine.CollectTopProcesses(1, &failed_samples, &error),
            "process failure should return false"
        )) {
        return 1;
    }
    if (expect(!error.empty(), "process failure should populate error message")) {
        return 1;
    }

    g_process_status = AURA_STATUS_OK;
    aura_process_sample sample{};
    sample.pid = 101;
    std::strncpy(sample.name, "recover", sizeof(sample.name) - 1);
    sample.cpu_percent = 12.0;
    sample.memory_rss_bytes = 2048;
    g_process_samples = {sample};

    std::vector<ProcessSample> recovered_samples;
    if (expect(
            engine.CollectTopProcesses(1, &recovered_samples, &error),
            "process success should return true"
        )) {
        return 1;
    }
    if (expect(error.empty(), "process success should clear stale error")) {
        return 1;
    }
    if (expect(recovered_samples.size() == 1U, "process success should return one sample")) {
        return 1;
    }
    return 0;
}

int test_process_tie_break_is_deterministic() {
    g_process_status = AURA_STATUS_OK;
    g_process_samples.clear();

    aura_process_sample a{};
    a.pid = 40;
    std::strncpy(a.name, "alpha", sizeof(a.name) - 1);
    a.cpu_percent = 30.0;
    a.memory_rss_bytes = 1000;

    aura_process_sample b{};
    b.pid = 12;
    std::strncpy(b.name, "beta", sizeof(b.name) - 1);
    b.cpu_percent = 30.0;
    b.memory_rss_bytes = 2000;

    aura_process_sample c{};
    c.pid = 18;
    std::strncpy(c.name, "gamma", sizeof(c.name) - 1);
    c.cpu_percent = 30.0;
    c.memory_rss_bytes = 2000;

    aura_process_sample d{};
    d.pid = 7;
    std::strncpy(d.name, "delta", sizeof(d.name) - 1);
    d.cpu_percent = 50.0;
    d.memory_rss_bytes = 500;

    aura_process_sample e{};
    e.pid = 3;
    std::strncpy(e.name, "epsilon", sizeof(e.name) - 1);
    e.cpu_percent = 50.0;
    e.memory_rss_bytes = 500;

    aura_process_sample f{};
    f.pid = 60;
    std::strncpy(f.name, "zeta", sizeof(f.name) - 1);
    f.cpu_percent = 10.0;
    f.memory_rss_bytes = 9999;

    g_process_samples = {a, b, c, d, e, f};

    TelemetryEngine engine(make_collectors());
    std::vector<ProcessSample> samples;
    std::string error;
    if (expect(
            engine.CollectTopProcesses(4, &samples, &error),
            "process tie-break collection should succeed"
        )) {
        return 1;
    }
    if (expect(samples.size() == 4U, "expected 4 top processes")) {
        return 1;
    }
    if (expect(samples[0].pid == 3U, "first process should be pid 3")) {
        return 1;
    }
    if (expect(samples[1].pid == 7U, "second process should be pid 7")) {
        return 1;
    }
    if (expect(samples[2].pid == 12U, "third process should be pid 12")) {
        return 1;
    }
    if (expect(samples[3].pid == 18U, "fourth process should be pid 18")) {
        return 1;
    }
    return 0;
}

int test_process_empty_collection_returns_empty() {
    g_process_status = AURA_STATUS_OK;
    g_process_samples.clear();

    TelemetryEngine engine(make_collectors());
    std::vector<ProcessSample> samples;
    std::string error = "stale";
    if (expect(
            engine.CollectTopProcesses(8, &samples, &error),
            "empty process collection should succeed"
        )) {
        return 1;
    }
    if (expect(samples.empty(), "empty process collection should return no samples")) {
        return 1;
    }
    return 0;
}

int test_process_empty_name_falls_back_to_pid() {
    g_process_status = AURA_STATUS_OK;
    g_process_samples.clear();

    aura_process_sample unnamed{};
    unnamed.pid = 42;
    unnamed.cpu_percent = 12.0;
    unnamed.memory_rss_bytes = 4000;
    g_process_samples.push_back(unnamed);

    TelemetryEngine engine(make_collectors());
    std::vector<ProcessSample> samples;
    std::string error;
    if (expect(
            engine.CollectTopProcesses(1, &samples, &error),
            "process fallback-name collection should succeed"
        )) {
        return 1;
    }
    if (expect(samples.size() == 1U, "expected one process sample")) {
        return 1;
    }
    if (expect(samples[0].name == "pid-42", "empty process name should fall back to pid label")) {
        return 1;
    }
    return 0;
}

int test_native_process_collector_returns_ranked_top_k() {
#ifndef _WIN32
    return 0;
#else
    constexpr uint32_t kMaxSamples = 8;
    std::array<aura_process_sample, kMaxSamples> samples{};
    uint32_t out_count = 0;
    std::array<char, 256> error_buffer{};
    const int status = aura_collect_processes(
        samples.data(),
        kMaxSamples,
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );
    if (status == AURA_STATUS_UNAVAILABLE) {
        return 0;
    }
    if (expect(status == AURA_STATUS_OK, "native process collector should succeed")) {
        return 1;
    }
    if (expect(out_count <= kMaxSamples, "native process collector exceeded max samples")) {
        return 1;
    }
    for (uint32_t i = 0; i < out_count; ++i) {
        if (expect(samples[i].pid != 0U, "native process collector should not return pid 0")) {
            return 1;
        }
    }
    for (uint32_t i = 1; i < out_count; ++i) {
        const aura_process_sample& previous = samples[static_cast<size_t>(i - 1U)];
        const aura_process_sample& current = samples[static_cast<size_t>(i)];
        if (expect(
                !process_ranks_before(current, previous),
                "native process collector should keep ranked ordering"
            )) {
            return 1;
        }
    }
    return 0;
#endif
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

int test_network_non_increasing_timestamp_keeps_baseline() {
    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {
        aura_network_counters{1000, 2000, 10, 20},
        aura_network_counters{2000, 3000, 20, 30},
        aura_network_counters{5000, 8000, 50, 80},
    };

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot first{};
    NetworkSnapshot non_increasing{};
    NetworkSnapshot resumed{};
    std::string error;
    if (expect(engine.CollectNetworkSnapshot(200.0, &first, &error), "network baseline sample should succeed")) {
        return 1;
    }
    if (expect(
            engine.CollectNetworkSnapshot(200.0, &non_increasing, &error),
            "network non-increasing timestamp sample should succeed"
        )) {
        return 1;
    }
    if (expect(
            nearly_equal(non_increasing.bytes_sent_per_sec, 0.0),
            "non-increasing network send rate should stay zero"
        )) {
        return 1;
    }
    if (expect(
            engine.CollectNetworkSnapshot(204.0, &resumed, &error),
            "network resumed sample should succeed"
        )) {
        return 1;
    }
    if (expect(nearly_equal(resumed.bytes_sent_per_sec, 1000.0), "network resumed send rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.bytes_recv_per_sec, 1500.0), "network resumed recv rate mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.packets_sent_per_sec, 10.0), "network resumed send packets mismatch")) {
        return 1;
    }
    if (expect(nearly_equal(resumed.packets_recv_per_sec, 15.0), "network resumed recv packets mismatch")) {
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

int test_network_error_message_clears_on_graceful_paths() {
    g_network_status = AURA_STATUS_ERROR;
    g_network_index = 0;
    g_network_sequence.clear();

    TelemetryEngine engine(make_collectors());
    NetworkSnapshot failed{};
    std::string error;
    if (expect(!engine.CollectNetworkSnapshot(100.0, &failed, &error), "network error should fail")) {
        return 1;
    }
    if (expect(!error.empty(), "network error should populate error message")) {
        return 1;
    }

    g_network_status = AURA_STATUS_UNAVAILABLE;
    g_network_sequence = {aura_network_counters{10, 20, 1, 2}};
    NetworkSnapshot unavailable{};
    if (expect(
            engine.CollectNetworkSnapshot(101.0, &unavailable, &error),
            "network unavailable should return success"
        )) {
        return 1;
    }
    if (expect(error.empty(), "network unavailable path should clear stale error")) {
        return 1;
    }

    g_network_status = AURA_STATUS_OK;
    g_network_index = 0;
    g_network_sequence = {aura_network_counters{50, 70, 5, 7}};
    NetworkSnapshot recovered{};
    if (expect(
            engine.CollectNetworkSnapshot(102.0, &recovered, &error),
            "network success should return true"
        )) {
        return 1;
    }
    if (expect(error.empty(), "network success path should keep error empty")) {
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

}  // namespace

int main() {
    using TestCase = int (*)();
    const std::vector<TestCase> tests = {
        test_system_snapshot_success,
        test_system_error_message_clears_on_success_path,
        test_process_sort_and_limit,
        test_process_error_message_clears_on_success_path,
        test_process_tie_break_is_deterministic,
        test_process_empty_collection_returns_empty,
        test_process_empty_name_falls_back_to_pid,
        test_native_process_collector_returns_ranked_top_k,
        test_disk_rate_computation,
        test_disk_non_increasing_timestamp_keeps_baseline,
        test_disk_unavailable_degrades_gracefully,
        test_disk_error_still_fails,
        test_disk_error_message_clears_on_graceful_paths,
        test_network_rate_computation,
        test_network_non_increasing_timestamp_keeps_baseline,
        test_network_unavailable_degrades_gracefully,
        test_network_error_still_fails,
        test_network_error_message_clears_on_graceful_paths,
        test_thermal_degrades_gracefully_when_unavailable,
        test_thermal_success,
        test_thermal_error_message_clears_on_graceful_paths,
        test_per_core_cpu_success,
        test_per_core_cpu_unavailable_degrades_gracefully,
        test_per_core_cpu_null_collector_degrades_gracefully,
        test_gpu_stub_returns_unavailable_gracefully,
        test_gpu_success_when_available,
        test_gpu_null_collector_degrades_gracefully,
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
