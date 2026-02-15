#include "telemetry_abi.h"
#include "telemetry_utils.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

namespace {

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

}  // namespace

#endif  // _WIN32

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
