#include "telemetry_abi.h"
#include "telemetry_utils.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

namespace {

struct CpuSnapshotState {
    bool has_previous = false;
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
};

CpuSnapshotState g_cpu_snapshot_state;
std::mutex g_cpu_snapshot_mutex;

}  // namespace

struct ProcessCpuState {
    uint64_t process_total_100ns = 0;
    uint64_t sampled_at_100ns = 0;
};

static std::unordered_map<uint32_t, ProcessCpuState> g_process_cpu_state;
static std::mutex g_process_cpu_mutex;

bool compute_process_cpu_percent(
    uint32_t pid,
    HANDLE process_handle,
    uint64_t sampled_at_100ns,
    int cpu_count,
    double* out_percent
) {
    if (out_percent == nullptr) {
        return false;
    }
    *out_percent = 0.0;
    if (process_handle == nullptr || process_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(process_handle, &creation, &exit, &kernel, &user)) {
        return false;
    }

    const uint64_t process_total = filetime_to_uint64(kernel) + filetime_to_uint64(user);
    ProcessCpuState previous{};
    bool had_previous = false;
    {
        std::lock_guard<std::mutex> lock(g_process_cpu_mutex);
        auto existing = g_process_cpu_state.find(pid);
        if (existing != g_process_cpu_state.end()) {
            previous = existing->second;
            had_previous = true;
        }
        g_process_cpu_state[pid] = ProcessCpuState{process_total, sampled_at_100ns};
    }

    if (!had_previous || sampled_at_100ns <= previous.sampled_at_100ns || cpu_count <= 0) {
        return true;
    }
    if (process_total < previous.process_total_100ns) {
        return true;
    }

    const uint64_t delta_process_100ns = process_total - previous.process_total_100ns;
    const uint64_t delta_wall_100ns = sampled_at_100ns - previous.sampled_at_100ns;
    if (delta_wall_100ns == 0) {
        return true;
    }

    const double cpu = (static_cast<double>(delta_process_100ns) * 100.0) /
                       (static_cast<double>(delta_wall_100ns) * static_cast<double>(cpu_count));
    if (std::isfinite(cpu) && cpu > 0.0) {
        *out_percent = cpu;
    }
    return true;
}

void prune_process_cpu_state(const std::unordered_set<uint32_t>& seen_pids) {
    std::lock_guard<std::mutex> lock(g_process_cpu_mutex);
    for (auto it = g_process_cpu_state.begin(); it != g_process_cpu_state.end();) {
        if (seen_pids.find(it->first) == seen_pids.end()) {
            it = g_process_cpu_state.erase(it);
        } else {
            ++it;
        }
    }
}

namespace {

bool collect_system_cpu_percent(double* cpu_percent) {
    if (cpu_percent == nullptr) {
        return false;
    }

    FILETIME idle{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return false;
    }

    const uint64_t idle_100ns = filetime_to_uint64(idle);
    const uint64_t kernel_100ns = filetime_to_uint64(kernel);
    const uint64_t user_100ns = filetime_to_uint64(user);

    double usage = 0.0;
    {
        std::lock_guard<std::mutex> lock(g_cpu_snapshot_mutex);
        if (g_cpu_snapshot_state.has_previous) {
            const uint64_t delta_idle = idle_100ns - g_cpu_snapshot_state.idle;
            const uint64_t delta_kernel = kernel_100ns - g_cpu_snapshot_state.kernel;
            const uint64_t delta_user = user_100ns - g_cpu_snapshot_state.user;
            const uint64_t delta_total = delta_kernel + delta_user;
            if (delta_total > 0 && delta_total >= delta_idle) {
                usage = (static_cast<double>(delta_total - delta_idle) * 100.0) /
                        static_cast<double>(delta_total);
            }
        }

        g_cpu_snapshot_state.has_previous = true;
        g_cpu_snapshot_state.idle = idle_100ns;
        g_cpu_snapshot_state.kernel = kernel_100ns;
        g_cpu_snapshot_state.user = user_100ns;
    }

    *cpu_percent = clamp_percent(usage);
    return true;
}

bool collect_memory_percent(double* memory_percent) {
    if (memory_percent == nullptr) {
        return false;
    }

    MEMORYSTATUSEX state{};
    state.dwLength = sizeof(state);
    if (!GlobalMemoryStatusEx(&state)) {
        return false;
    }
    *memory_percent = clamp_percent(static_cast<double>(state.dwMemoryLoad));
    return true;
}

}  // namespace

#endif  // _WIN32

extern "C" int aura_collect_system_snapshot(
    double* cpu_percent,
    double* memory_percent,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)cpu_percent;
    (void)memory_percent;
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (cpu_percent == nullptr || memory_percent == nullptr) {
        write_error(error_buffer, error_buffer_len, "Output pointers must not be null.");
        return AURA_STATUS_ERROR;
    }

    double cpu = 0.0;
    double memory = 0.0;
    if (!collect_system_cpu_percent(&cpu)) {
        write_error(error_buffer, error_buffer_len, "GetSystemTimes failed.");
        return AURA_STATUS_ERROR;
    }
    if (!collect_memory_percent(&memory)) {
        write_error(error_buffer, error_buffer_len, "GlobalMemoryStatusEx failed.");
        return AURA_STATUS_ERROR;
    }

    *cpu_percent = cpu;
    *memory_percent = memory;
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_collect_per_core_cpu(
    double* out_percents,
    uint32_t max_cores,
    uint32_t* out_core_count,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)out_percents;
    (void)max_cores;
    if (out_core_count != nullptr) {
        *out_core_count = 0;
    }
    write_error(error_buffer, error_buffer_len, "Per-core CPU is unavailable on this platform.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (out_percents == nullptr || out_core_count == nullptr || max_cores == 0) {
        write_error(error_buffer, error_buffer_len, "Invalid per-core CPU buffer arguments.");
        return AURA_STATUS_ERROR;
    }

    const int cpu_count = logical_cpu_count();
    const uint32_t cores = std::min(static_cast<uint32_t>(cpu_count), max_cores);

    using NtQuerySysInfoFn = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);
    static NtQuerySysInfoFn nt_query = nullptr;
    static bool nt_query_resolved = false;
    if (!nt_query_resolved) {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll != nullptr) {
            nt_query = reinterpret_cast<NtQuerySysInfoFn>(
                GetProcAddress(ntdll, "NtQuerySystemInformation")
            );
        }
        nt_query_resolved = true;
    }

    if (nt_query == nullptr) {
        *out_core_count = 0;
        write_error(error_buffer, error_buffer_len, "NtQuerySystemInformation not available.");
        return AURA_STATUS_UNAVAILABLE;
    }

    struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
        LARGE_INTEGER IdleTime;
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER Reserved1[2];
        ULONG Reserved2;
    };

    const size_t buf_count = static_cast<size_t>(cpu_count);
    std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> perf_info(buf_count);
    ULONG return_length = 0;
    const LONG status = nt_query(
        8,
        perf_info.data(),
        static_cast<ULONG>(buf_count * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)),
        &return_length
    );
    if (status != 0) {
        *out_core_count = 0;
        write_error(error_buffer, error_buffer_len, "NtQuerySystemInformation failed.");
        return AURA_STATUS_UNAVAILABLE;
    }

    const uint32_t returned_cores = static_cast<uint32_t>(
        return_length / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)
    );
    const uint32_t actual_cores = std::min(returned_cores, cores);

    struct PerCoreState {
        uint64_t idle = 0;
        uint64_t kernel = 0;
        uint64_t user = 0;
    };
    static std::vector<PerCoreState> g_per_core_state;
    static bool g_per_core_has_previous = false;
    static std::mutex g_per_core_mutex;

    std::lock_guard<std::mutex> lock(g_per_core_mutex);

    if (!g_per_core_has_previous || g_per_core_state.size() != static_cast<size_t>(actual_cores)) {
        g_per_core_state.resize(static_cast<size_t>(actual_cores));
        for (uint32_t i = 0; i < actual_cores; ++i) {
            g_per_core_state[i].idle = static_cast<uint64_t>(perf_info[i].IdleTime.QuadPart);
            g_per_core_state[i].kernel = static_cast<uint64_t>(perf_info[i].KernelTime.QuadPart);
            g_per_core_state[i].user = static_cast<uint64_t>(perf_info[i].UserTime.QuadPart);
            out_percents[i] = 0.0;
        }
        g_per_core_has_previous = true;
        *out_core_count = actual_cores;
        write_error(error_buffer, error_buffer_len, "");
        return AURA_STATUS_OK;
    }

    for (uint32_t i = 0; i < actual_cores; ++i) {
        const uint64_t idle = static_cast<uint64_t>(perf_info[i].IdleTime.QuadPart);
        const uint64_t kernel = static_cast<uint64_t>(perf_info[i].KernelTime.QuadPart);
        const uint64_t user = static_cast<uint64_t>(perf_info[i].UserTime.QuadPart);

        const uint64_t delta_idle = idle - g_per_core_state[i].idle;
        const uint64_t delta_kernel = kernel - g_per_core_state[i].kernel;
        const uint64_t delta_user = user - g_per_core_state[i].user;
        const uint64_t delta_total = delta_kernel + delta_user;

        double usage = 0.0;
        if (delta_total > 0 && delta_total >= delta_idle) {
            usage = (static_cast<double>(delta_total - delta_idle) * 100.0) /
                    static_cast<double>(delta_total);
        }
        out_percents[i] = clamp_percent(usage);

        g_per_core_state[i].idle = idle;
        g_per_core_state[i].kernel = kernel;
        g_per_core_state[i].user = user;
    }

    *out_core_count = actual_cores;
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}
