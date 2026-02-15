#include "telemetry_abi.h"
#include "telemetry_utils.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")
#endif

extern "C" int aura_collect_processes(
    aura_process_sample* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)samples;
    (void)max_samples;
    if (out_count != nullptr) {
        *out_count = 0;
    }
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (samples == nullptr || out_count == nullptr || max_samples == 0) {
        write_error(error_buffer, error_buffer_len, "Invalid process collection buffer arguments.");
        return AURA_STATUS_ERROR;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        write_error(error_buffer, error_buffer_len, "CreateToolhelp32Snapshot failed.");
        *out_count = 0;
        return AURA_STATUS_ERROR;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::vector<aura_process_sample> collected;
    collected.reserve(static_cast<size_t>(max_samples) * 4U);

    const uint64_t sampled_at = now_100ns();
    const int cpu_count = logical_cpu_count();
    std::unordered_set<uint32_t> seen_pids;

    BOOL has_entry = Process32FirstW(snapshot, &entry);
    while (has_entry) {
        const uint32_t pid = static_cast<uint32_t>(entry.th32ProcessID);
        has_entry = Process32NextW(snapshot, &entry);
        if (pid == 0) {
            continue;
        }
        seen_pids.insert(pid);

        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
            FALSE,
            static_cast<DWORD>(pid)
        );

        uint64_t rss_bytes = 0;
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            if (GetProcessMemoryInfo(
                    process,
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                    sizeof(memory)
                )) {
                rss_bytes = static_cast<uint64_t>(memory.WorkingSetSize);
            }
        }

        double cpu = 0.0;
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            (void)compute_process_cpu_percent(pid, process, sampled_at, cpu_count, &cpu);
        }

        aura_process_sample sample{};
        sample.pid = pid;
        sample.cpu_percent = std::isfinite(cpu) && cpu > 0.0 ? cpu : 0.0;
        sample.memory_rss_bytes = rss_bytes;

        std::string utf8_name = utf8_from_utf16(entry.szExeFile);
        if (utf8_name.empty()) {
            utf8_name = "pid-" + std::to_string(pid);
        }
        write_utf8_name(sample.name, kProcessNameBytes, utf8_name);
        collected.push_back(sample);

        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            CloseHandle(process);
        }
    }

    CloseHandle(snapshot);
    prune_process_cpu_state(seen_pids);

    const size_t result_count = std::min(static_cast<size_t>(max_samples), collected.size());
    if (result_count == 0U) {
        *out_count = 0;
        write_error(error_buffer, error_buffer_len, "");
        return AURA_STATUS_OK;
    }

    const auto process_rank = [](const aura_process_sample& left, const aura_process_sample& right) {
        if (left.cpu_percent != right.cpu_percent) {
            return left.cpu_percent > right.cpu_percent;
        }
        if (left.memory_rss_bytes != right.memory_rss_bytes) {
            return left.memory_rss_bytes > right.memory_rss_bytes;
        }
        return left.pid < right.pid;
    };

    std::partial_sort(
        collected.begin(),
        collected.begin() + static_cast<std::ptrdiff_t>(result_count),
        collected.end(),
        process_rank
    );

    for (size_t i = 0; i < result_count; ++i) {
        samples[i] = collected[i];
    }
    *out_count = static_cast<uint32_t>(result_count);
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}
