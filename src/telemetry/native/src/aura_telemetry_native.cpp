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

extern "C" int aura_collect_process_details(
    const aura_process_query_options* options,
    aura_process_detail* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)options;
    (void)samples;
    (void)max_samples;
    if (out_count != nullptr) {
        *out_count = 0;
    }
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (samples == nullptr || out_count == nullptr || options == nullptr || max_samples == 0) {
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
    std::vector<aura_process_detail> collected;
    collected.reserve(static_cast<size_t>(max_samples) * 4U);

    const uint64_t sampled_at = now_100ns();
    const int cpu_count = logical_cpu_count();
    std::unordered_set<uint32_t> seen_pids;
    std::unordered_map<uint32_t, PROCESSENTRY32W> pid_to_entry;

    BOOL has_entry = Process32FirstW(snapshot, &entry);
    while (has_entry) {
        const uint32_t pid = static_cast<uint32_t>(entry.th32ProcessID);
        has_entry = Process32NextW(snapshot, &entry);
        if (pid == 0) {
            continue;
        }

        // Apply name filter if specified
        std::string utf8_name = utf8_from_utf16(entry.szExeFile);
        if (utf8_name.empty()) {
            utf8_name = "pid-" + std::to_string(pid);
        }

        if (options->name_filter[0] != '\0') {
            const std::string name_filter(options->name_filter);
            if (utf8_name.find(name_filter) == std::string::npos) {
                continue;
            }
        }

        seen_pids.insert(pid);
        pid_to_entry[pid] = entry;

        HANDLE process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
            FALSE,
            static_cast<DWORD>(pid)
        );

        aura_process_detail detail{};
        detail.pid = pid;
        detail.parent_pid = static_cast<uint32_t>(entry.th32ParentProcessID);
        write_utf8_name(detail.name, kProcessNameBytes, utf8_name);

        // CPU calculation
        double cpu = 0.0;
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            (void)compute_process_cpu_percent(pid, process, sampled_at, cpu_count, &cpu);
        }
        detail.cpu_percent = std::isfinite(cpu) && cpu > 0.0 ? cpu : 0.0;

        // Memory counters
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            if (GetProcessMemoryInfo(
                    process,
                    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                    sizeof(memory)
                )) {
                detail.memory_rss_bytes = static_cast<uint64_t>(memory.WorkingSetSize);
                detail.memory_private_bytes = static_cast<uint64_t>(memory.PrivateUsage);
                detail.memory_peak_bytes = static_cast<uint64_t>(memory.PeakWorkingSetSize);
            }
        }

        // Thread count
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            DWORD thread_count = 0;
            HANDLE thread_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (thread_snap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te32{};
                te32.dwSize = sizeof(te32);
                if (Thread32First(thread_snap, &te32)) {
                    do {
                        if (te32.th32OwnerProcessID == static_cast<DWORD>(pid)) {
                            thread_count++;
                        }
                    } while (Thread32Next(thread_snap, &te32));
                }
                CloseHandle(thread_snap);
            }
            detail.thread_count = thread_count;
        }

        // Handle count
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            DWORD handle_count = 0;
            typedef DWORD(WINAPI* GetProcessHandleCountFn)(HANDLE, PDWORD);
            static GetProcessHandleCountFn get_process_handle_count = nullptr;
            static bool handle_count_resolved = false;
            if (!handle_count_resolved) {
                HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
                if (kernel32 != nullptr) {
                    get_process_handle_count = reinterpret_cast<GetProcessHandleCountFn>(
                        GetProcAddress(kernel32, "GetProcessHandleCount")
                    );
                }
                handle_count_resolved = true;
            }
            if (get_process_handle_count != nullptr) {
                if (get_process_handle_count(process, &handle_count)) {
                    detail.handle_count = handle_count;
                }
            }
        }

        // Priority class
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            const DWORD priority_class = GetPriorityClass(process);
            detail.priority_class = static_cast<uint32_t>(priority_class);
        }

        // Start time from process entry
        FILETIME creation_time{};
        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            FILETIME exit_time{};
            FILETIME kernel_time{};
            FILETIME user_time{};
            if (GetProcessTimes(process, &creation_time, &exit_time, &kernel_time, &user_time)) {
                detail.start_time_100ns = filetime_to_uint64(creation_time);
            }
        }

        // Command line (if requested)
        if (options->include_command_line && process != nullptr && process != INVALID_HANDLE_VALUE) {
            // For now, set empty string as full command line requires more complex handling
            // with NtQueryInformationProcess and ProcessBasicInformation
            detail.command_line[0] = '\0';
        } else {
            detail.command_line[0] = '\0';
        }

        collected.push_back(detail);

        if (process != nullptr && process != INVALID_HANDLE_VALUE) {
            CloseHandle(process);
        }
    }

    CloseHandle(snapshot);
    prune_process_cpu_state(seen_pids);

    // Apply max_results limit
    const uint32_t result_limit = std::min(options->max_results, max_samples);
    const size_t result_count = std::min(static_cast<size_t>(result_limit), collected.size());

    if (result_count == 0U) {
        *out_count = 0;
        write_error(error_buffer, error_buffer_len, "");
        return AURA_STATUS_OK;
    }

    // Sort based on options
    auto process_rank = [options](const aura_process_detail& left, const aura_process_detail& right) -> bool {
        switch (options->sort_column) {
            case 0: // PID
                return left.pid < right.pid;
            case 1: // Name
                if (std::strcmp(left.name, right.name) != 0) {
                    return std::strcmp(left.name, right.name) < 0;
                }
                return left.pid < right.pid;
            case 2: // CPU
                if (left.cpu_percent != right.cpu_percent) {
                    return left.cpu_percent > right.cpu_percent;
                }
                return left.pid < right.pid;
            case 3: // Memory
                if (left.memory_rss_bytes != right.memory_rss_bytes) {
                    return left.memory_rss_bytes > right.memory_rss_bytes;
                }
                return left.pid < right.pid;
            case 4: // Threads
                if (left.thread_count != right.thread_count) {
                    return left.thread_count > right.thread_count;
                }
                return left.pid < right.pid;
            default:
                return left.pid < right.pid;
        }
    };

    std::sort(collected.begin(), collected.end(), process_rank);

    // If descending, reverse
    if (options->sort_descending) {
        std::reverse(collected.begin(), collected.end());
    }

    // Copy to output
    for (size_t i = 0; i < result_count; ++i) {
        samples[i] = collected[i];
    }
    *out_count = static_cast<uint32_t>(result_count);
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_build_process_tree(
    const aura_process_detail* process_details,
    uint32_t process_count,
    aura_process_tree_node* tree_nodes,
    uint32_t max_nodes,
    uint32_t* out_node_count,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)process_details;
    (void)process_count;
    (void)tree_nodes;
    (void)max_nodes;
    if (out_node_count != nullptr) {
        *out_node_count = 0;
    }
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (process_details == nullptr || tree_nodes == nullptr || out_node_count == nullptr) {
        write_error(error_buffer, error_buffer_len, "Invalid process tree arguments.");
        return AURA_STATUS_ERROR;
    }

    const uint32_t result_count = std::min(process_count, max_nodes);

    // Build parent-to-children map
    std::unordered_map<uint32_t, std::vector<uint32_t>> parent_to_children;
    for (uint32_t i = 0; i < result_count; ++i) {
        const uint32_t parent_pid = process_details[i].parent_pid;
        const uint32_t pid = process_details[i].pid;
        if (parent_pid != 0) {
            parent_to_children[parent_pid].push_back(pid);
        }
    }

    // Calculate depth for each process using DFS
    std::unordered_map<uint32_t, uint32_t> pid_to_depth;
    std::unordered_map<uint32_t, uint32_t> pid_to_child_count;

    for (uint32_t i = 0; i < result_count; ++i) {
        const uint32_t pid = process_details[i].pid;
        const uint32_t parent_pid = process_details[i].parent_pid;

        // Calculate depth
        uint32_t depth = 0;
        uint32_t current_pid = parent_pid;
        while (current_pid != 0) {
            depth++;
            // Find parent in process details
            bool found = false;
            for (uint32_t j = 0; j < result_count; ++j) {
                if (process_details[j].pid == current_pid) {
                    current_pid = process_details[j].parent_pid;
                    found = true;
                    break;
                }
            }
            if (!found) {
                break;
            }
            if (depth > 100) { // Prevent infinite loops
                break;
            }
        }
        pid_to_depth[pid] = depth;

        // Count children
        auto it = parent_to_children.find(pid);
        pid_to_child_count[pid] = (it != parent_to_children.end()) ?
            static_cast<uint32_t>(it->second.size()) : 0U;
    }

    // Fill tree nodes
    for (uint32_t i = 0; i < result_count; ++i) {
        const uint32_t pid = process_details[i].pid;
        tree_nodes[i].pid = pid;
        tree_nodes[i].depth = pid_to_depth[pid];
        tree_nodes[i].child_count = pid_to_child_count[pid];
        tree_nodes[i].has_children = pid_to_child_count[pid] > 0 ? 1 : 0;
    }

    *out_node_count = result_count;
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_get_process_by_pid(
    uint32_t pid,
    aura_process_detail* out_detail,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)pid;
    (void)out_detail;
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (out_detail == nullptr) {
        write_error(error_buffer, error_buffer_len, "Process detail output pointer must not be null.");
        return AURA_STATUS_ERROR;
    }

    // Zero-initialize output structure
    std::memset(out_detail, 0, sizeof(*out_detail));
    out_detail->pid = pid;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        write_error(error_buffer, error_buffer_len, "CreateToolhelp32Snapshot failed.");
        return AURA_STATUS_ERROR;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    BOOL has_entry = Process32FirstW(snapshot, &entry);
    while (has_entry) {
        if (static_cast<uint32_t>(entry.th32ProcessID) == pid) {
            found = true;
            std::string utf8_name = utf8_from_utf16(entry.szExeFile);
            if (utf8_name.empty()) {
                utf8_name = "pid-" + std::to_string(pid);
            }
            write_utf8_name(out_detail->name, kProcessNameBytes, utf8_name);
            out_detail->parent_pid = static_cast<uint32_t>(entry.th32ParentProcessID);
            break;
        }
        has_entry = Process32NextW(snapshot, &entry);
    }

    CloseHandle(snapshot);

    if (!found) {
        write_error(error_buffer, error_buffer_len, "Process not found.");
        return AURA_STATUS_ERROR;
    }

    HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
        FALSE,
        static_cast<DWORD>(pid)
    );

    if (process == nullptr || process == INVALID_HANDLE_VALUE) {
        write_error(error_buffer, error_buffer_len, "Failed to open process for details.");
        return AURA_STATUS_ERROR;
    }

    // CPU calculation
    const uint64_t sampled_at = now_100ns();
    const int cpu_count = logical_cpu_count();
    double cpu = 0.0;
    (void)compute_process_cpu_percent(pid, process, sampled_at, cpu_count, &cpu);
    out_detail->cpu_percent = std::isfinite(cpu) && cpu > 0.0 ? cpu : 0.0;

    // Memory counters
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(
            process,
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
            sizeof(memory)
        )) {
        out_detail->memory_rss_bytes = static_cast<uint64_t>(memory.WorkingSetSize);
        out_detail->memory_private_bytes = static_cast<uint64_t>(memory.PrivateUsage);
        out_detail->memory_peak_bytes = static_cast<uint64_t>(memory.PeakWorkingSetSize);
    }

    // Thread count
    DWORD thread_count = 0;
    HANDLE thread_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (thread_snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te32{};
        te32.dwSize = sizeof(te32);
        if (Thread32First(thread_snap, &te32)) {
            do {
                if (te32.th32OwnerProcessID == static_cast<DWORD>(pid)) {
                    thread_count++;
                }
            } while (Thread32Next(thread_snap, &te32));
        }
        CloseHandle(thread_snap);
    }
    out_detail->thread_count = thread_count;

    // Handle count
    DWORD handle_count = 0;
    typedef DWORD(WINAPI* GetProcessHandleCountFn)(HANDLE, PDWORD);
    static GetProcessHandleCountFn get_process_handle_count = nullptr;
    static bool handle_count_resolved = false;
    if (!handle_count_resolved) {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (kernel32 != nullptr) {
            get_process_handle_count = reinterpret_cast<GetProcessHandleCountFn>(
                GetProcAddress(kernel32, "GetProcessHandleCount")
            );
        }
        handle_count_resolved = true;
    }
    if (get_process_handle_count != nullptr) {
        if (get_process_handle_count(process, &handle_count)) {
            out_detail->handle_count = handle_count;
        }
    }

    // Priority class
    const DWORD priority_class = GetPriorityClass(process);
    out_detail->priority_class = static_cast<uint32_t>(priority_class);

    // Start time
    FILETIME creation_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (GetProcessTimes(process, &creation_time, &exit_time, &kernel_time, &user_time)) {
        out_detail->start_time_100ns = filetime_to_uint64(creation_time);
    }

    // Command line (empty for now)
    out_detail->command_line[0] = '\0';

    CloseHandle(process);
    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_terminate_process(
    uint32_t pid,
    uint32_t exit_code,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)pid;
    (void)exit_code;
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (pid == 0) {
        write_error(error_buffer, error_buffer_len, "Cannot terminate process with PID 0.");
        return AURA_STATUS_ERROR;
    }

    // Check if we're trying to terminate ourselves
    const DWORD current_pid = GetCurrentProcessId();
    if (pid == static_cast<uint32_t>(current_pid)) {
        write_error(error_buffer, error_buffer_len, "Cannot terminate current process.");
        return AURA_STATUS_ERROR;
    }

    HANDLE process = OpenProcess(
        PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION,
        FALSE,
        static_cast<DWORD>(pid)
    );

    if (process == nullptr || process == INVALID_HANDLE_VALUE) {
        write_error(error_buffer, error_buffer_len, "Failed to open process for termination.");
        return AURA_STATUS_ERROR;
    }

    const BOOL success = TerminateProcess(process, static_cast<UINT>(exit_code));
    CloseHandle(process);

    if (!success) {
        write_error(error_buffer, error_buffer_len, "Failed to terminate process.");
        return AURA_STATUS_ERROR;
    }

    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_set_process_priority(
    uint32_t pid,
    uint32_t priority_class,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)pid;
    (void)priority_class;
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (pid == 0) {
        write_error(error_buffer, error_buffer_len, "Cannot set priority for process with PID 0.");
        return AURA_STATUS_ERROR;
    }

    HANDLE process = OpenProcess(
        PROCESS_SET_INFORMATION,
        FALSE,
        static_cast<DWORD>(pid)
    );

    if (process == nullptr || process == INVALID_HANDLE_VALUE) {
        write_error(error_buffer, error_buffer_len, "Failed to open process for priority change.");
        return AURA_STATUS_ERROR;
    }

    const BOOL success = SetPriorityClass(process, static_cast<DWORD>(priority_class));
    CloseHandle(process);

    if (!success) {
        write_error(error_buffer, error_buffer_len, "Failed to set process priority.");
        return AURA_STATUS_ERROR;
    }

    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}

extern "C" int aura_get_process_children(
    uint32_t pid,
    uint32_t* child_pids,
    uint32_t max_children,
    uint32_t* out_child_count,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)pid;
    (void)child_pids;
    (void)max_children;
    if (out_child_count != nullptr) {
        *out_child_count = 0;
    }
    write_error(error_buffer, error_buffer_len, "Windows telemetry backend is unavailable.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (child_pids == nullptr || out_child_count == nullptr) {
        write_error(error_buffer, error_buffer_len, "Invalid child PIDs buffer arguments.");
        return AURA_STATUS_ERROR;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        write_error(error_buffer, error_buffer_len, "CreateToolhelp32Snapshot failed.");
        *out_child_count = 0;
        return AURA_STATUS_ERROR;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::vector<uint32_t> children;
    children.reserve(static_cast<size_t>(max_children));

    BOOL has_entry = Process32FirstW(snapshot, &entry);
    while (has_entry) {
        if (static_cast<uint32_t>(entry.th32ParentProcessID) == pid) {
            const uint32_t child_pid = static_cast<uint32_t>(entry.th32ProcessID);
            if (children.size() < static_cast<size_t>(max_children)) {
                children.push_back(child_pid);
            }
        }
        has_entry = Process32NextW(snapshot, &entry);
    }

    CloseHandle(snapshot);

    const uint32_t result_count = std::min(static_cast<uint32_t>(children.size()), max_children);
    for (uint32_t i = 0; i < result_count; ++i) {
        child_pids[i] = children[i];
    }
    *out_child_count = result_count;

    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}
