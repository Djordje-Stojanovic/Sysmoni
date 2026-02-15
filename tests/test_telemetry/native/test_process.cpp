#include "test_fakes.h"
#include "test_helpers.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

using aura::telemetry::NativeCollectors;
using aura::telemetry::ProcessSample;
using aura::telemetry::TelemetryEngine;

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
