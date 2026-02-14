#include "telemetry_abi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winioctl.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#endif

namespace {

constexpr size_t kProcessNameBytes = 260;
constexpr size_t kThermalLabelBytes = 128;

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

#ifdef _WIN32
uint64_t filetime_to_uint64(const FILETIME& ft) {
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return static_cast<uint64_t>(value.QuadPart);
}

uint64_t now_100ns() {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    return filetime_to_uint64(now);
}

double clamp_percent(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 100.0) {
        return 100.0;
    }
    return value;
}

std::string utf8_from_utf16(const wchar_t* input) {
    if (input == nullptr || input[0] == L'\0') {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        input,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        input,
        -1,
        output.data(),
        required,
        nullptr,
        nullptr
    );
    if (written <= 0) {
        return {};
    }

    if (!output.empty() && output.back() == '\0') {
        output.pop_back();
    }
    return output;
}

void write_utf8_name(char* destination, size_t destination_size, const std::string& value) {
    if (destination == nullptr || destination_size == 0) {
        return;
    }
    destination[0] = '\0';
    if (value.empty()) {
        return;
    }
    const size_t copy_len = std::min(destination_size - 1, value.size());
    std::memcpy(destination, value.data(), copy_len);
    destination[copy_len] = '\0';
}

struct CpuSnapshotState {
    bool has_previous = false;
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
};

CpuSnapshotState g_cpu_snapshot_state;
std::mutex g_cpu_snapshot_mutex;

struct ProcessCpuState {
    uint64_t process_total_100ns = 0;
    uint64_t sampled_at_100ns = 0;
};

std::unordered_map<uint32_t, ProcessCpuState> g_process_cpu_state;
std::mutex g_process_cpu_mutex;

int logical_cpu_count() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const DWORD cpus = info.dwNumberOfProcessors;
    if (cpus == 0) {
        return 1;
    }
    return static_cast<int>(cpus);
}

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

bool collect_disk_counters_impl(aura_disk_counters* counters) {
    if (counters == nullptr) {
        return false;
    }

    uint64_t read_bytes = 0;
    uint64_t write_bytes = 0;
    uint64_t read_count = 0;
    uint64_t write_count = 0;
    bool collected_any = false;

    for (int index = 0; index < 64; ++index) {
        std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(index);
        HANDLE disk = CreateFileW(
            path.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (disk == INVALID_HANDLE_VALUE) {
            continue;
        }

        DISK_PERFORMANCE perf{};
        DWORD bytes_returned = 0;
        const BOOL ok = DeviceIoControl(
            disk,
            IOCTL_DISK_PERFORMANCE,
            nullptr,
            0,
            &perf,
            sizeof(perf),
            &bytes_returned,
            nullptr
        );
        CloseHandle(disk);
        if (!ok) {
            continue;
        }

        collected_any = true;
        read_bytes += static_cast<uint64_t>(perf.BytesRead.QuadPart);
        write_bytes += static_cast<uint64_t>(perf.BytesWritten.QuadPart);
        read_count += static_cast<uint64_t>(perf.ReadCount);
        write_count += static_cast<uint64_t>(perf.WriteCount);
    }

    if (!collected_any) {
        return false;
    }

    counters->read_bytes = read_bytes;
    counters->write_bytes = write_bytes;
    counters->read_count = read_count;
    counters->write_count = write_count;
    return true;
}

bool collect_network_counters_impl(aura_network_counters* counters) {
    if (counters == nullptr) {
        return false;
    }

    MIB_IF_TABLE2* table = nullptr;
    const DWORD result = GetIfTable2(&table);
    if (result != NO_ERROR || table == nullptr) {
        return false;
    }

    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;
    uint64_t packets_sent = 0;
    uint64_t packets_recv = 0;

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        bytes_sent += static_cast<uint64_t>(row.OutOctets);
        bytes_recv += static_cast<uint64_t>(row.InOctets);
        packets_sent += static_cast<uint64_t>(row.OutUcastPkts + row.OutNUcastPkts);
        packets_recv += static_cast<uint64_t>(row.InUcastPkts + row.InNUcastPkts);
    }

    FreeMibTable(table);
    counters->bytes_sent = bytes_sent;
    counters->bytes_recv = bytes_recv;
    counters->packets_sent = packets_sent;
    counters->packets_recv = packets_recv;
    return true;
}
#endif

}  // namespace

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

extern "C" int aura_collect_disk_counters(
    aura_disk_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)counters;
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (counters == nullptr) {
        write_error(error_buffer, error_buffer_len, "Disk counters output pointer must not be null.");
        return AURA_STATUS_ERROR;
    }
    if (!collect_disk_counters_impl(counters)) {
        write_error(error_buffer, error_buffer_len, "Unable to read disk counters via IOCTL_DISK_PERFORMANCE.");
        return AURA_STATUS_UNAVAILABLE;
    }
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_collect_network_counters(
    aura_network_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)counters;
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (counters == nullptr) {
        write_error(error_buffer, error_buffer_len, "Network counters output pointer must not be null.");
        return AURA_STATUS_ERROR;
    }
    if (!collect_network_counters_impl(counters)) {
        write_error(error_buffer, error_buffer_len, "GetIfTable2 failed.");
        return AURA_STATUS_ERROR;
    }
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_collect_thermal_readings(
    aura_thermal_reading* readings,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)readings;
    (void)max_samples;
    if (out_count != nullptr) {
        *out_count = 0;
    }
    write_error(
        error_buffer,
        error_buffer_len,
        "Thermal backend is currently unavailable in native collector."
    );
    return AURA_STATUS_UNAVAILABLE;
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

extern "C" int aura_collect_gpu_utilization(
    aura_gpu_utilization* out_gpu,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)out_gpu;
    write_error(
        error_buffer,
        error_buffer_len,
        "GPU telemetry backend is not yet implemented."
    );
    return AURA_STATUS_UNAVAILABLE;
}
