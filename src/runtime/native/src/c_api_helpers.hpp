#pragma once

#include "aura_platform.h"
#include "platform_internal.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

using aura::platform::Snapshot;

// ---------------------------------------------------------------------------
// Thread-local error reporting (shared by c_api.cpp and c_api_alert.cpp)
// ---------------------------------------------------------------------------

inline thread_local std::string g_last_error;

inline void SetLastError(const std::string& message) {
    g_last_error = message;
}

inline void SetError(aura_error_t* out_error, int code, const std::string& message) {
    SetLastError(message);
    if (out_error == nullptr) {
        return;
    }
    out_error->code = code;
    std::memset(out_error->message, 0, sizeof(out_error->message));
#ifdef _WIN32
    strncpy_s(out_error->message, sizeof(out_error->message), message.c_str(), _TRUNCATE);
#else
    std::strncpy(out_error->message, message.c_str(), sizeof(out_error->message) - 1);
    out_error->message[sizeof(out_error->message) - 1] = '\0';
#endif
}

inline void ClearError(aura_error_t* out_error) {
    if (out_error != nullptr) {
        out_error->code = AURA_OK;
        out_error->message[0] = '\0';
    }
    g_last_error.clear();
}

inline int HandleException(const std::exception& exc, aura_error_t* out_error) {
    SetError(out_error, AURA_ERR_RUNTIME, exc.what());
    return AURA_ERR_RUNTIME;
}

// ---------------------------------------------------------------------------
// Snapshot ABI conversion helpers
// ---------------------------------------------------------------------------

inline Snapshot ToInternalSnapshot(const aura_snapshot_t& raw) {
    Snapshot out;
    out.timestamp = raw.timestamp;
    out.cpu_percent = raw.cpu_percent;
    out.memory_percent = raw.memory_percent;
    out.disk_read_bps = raw.disk_read_bps;
    out.disk_write_bps = raw.disk_write_bps;
    out.net_recv_bps = raw.net_recv_bps;
    out.net_sent_bps = raw.net_sent_bps;
    return out;
}

inline aura_snapshot_t ToAbiSnapshot(const Snapshot& raw) {
    aura_snapshot_t out{};
    out.timestamp = raw.timestamp;
    out.cpu_percent = raw.cpu_percent;
    out.memory_percent = raw.memory_percent;
    out.disk_read_bps = raw.disk_read_bps;
    out.disk_write_bps = raw.disk_write_bps;
    out.net_recv_bps = raw.net_recv_bps;
    out.net_sent_bps = raw.net_sent_bps;
    return out;
}

inline int CopySnapshotsToOutput(
    const std::vector<Snapshot>& snapshots,
    aura_snapshot_t* out_snapshots,
    const int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (out_count == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_count must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    if (out_capacity < 0) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_capacity must be >= 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    if (static_cast<int>(snapshots.size()) > out_capacity) {
        SetError(out_error, AURA_ERR_CAPACITY, "Output buffer capacity is too small.");
        return AURA_ERR_CAPACITY;
    }

    if (!snapshots.empty() && out_snapshots == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_snapshots must not be null when results are present.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        out_snapshots[i] = ToAbiSnapshot(snapshots[i]);
    }

    *out_count = static_cast<int>(snapshots.size());
    ClearError(out_error);
    return AURA_OK;
}
