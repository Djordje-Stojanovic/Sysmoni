#include "telemetry_abi.h"
#include "telemetry_utils.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
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
