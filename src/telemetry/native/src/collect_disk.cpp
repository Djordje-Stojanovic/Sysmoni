#include "telemetry_abi.h"
#include "telemetry_utils.h"

#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>

namespace {

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

}  // namespace

#endif  // _WIN32

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
