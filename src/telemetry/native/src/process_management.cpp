#include "telemetry_abi.h"
#include "telemetry_utils.h"

#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#endif

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
