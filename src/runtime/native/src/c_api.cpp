#include "aura_platform.h"
#include "platform_internal.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>

using aura::platform::ConfigRequest;
using aura::platform::DbSource;
using aura::platform::Error;
using aura::platform::RuntimeConfig;
using aura::platform::Snapshot;
using aura::platform::TelemetryStore;

namespace {

struct AuraStore {
    explicit AuraStore(std::unique_ptr<TelemetryStore> store_in)
        : store(std::move(store_in)) {}

    std::unique_ptr<TelemetryStore> store;
};

thread_local std::string g_last_error;

void SetLastError(const std::string& message) {
    g_last_error = message;
}

void SetError(aura_error_t* out_error, int code, const std::string& message) {
    SetLastError(message);
    if (out_error == nullptr) {
        return;
    }
    out_error->code = code;
    std::memset(out_error->message, 0, sizeof(out_error->message));
    const std::size_t max_copy = sizeof(out_error->message) - 1;
    std::strncpy(out_error->message, message.c_str(), max_copy);
    out_error->message[max_copy] = '\0';
}

void ClearError(aura_error_t* out_error) {
    if (out_error != nullptr) {
        out_error->code = AURA_OK;
        out_error->message[0] = '\0';
    }
    g_last_error.clear();
}

Snapshot ToInternalSnapshot(const aura_snapshot_t& raw) {
    Snapshot out;
    out.timestamp = raw.timestamp;
    out.cpu_percent = raw.cpu_percent;
    out.memory_percent = raw.memory_percent;
    return out;
}

aura_snapshot_t ToAbiSnapshot(const Snapshot& raw) {
    aura_snapshot_t out{};
    out.timestamp = raw.timestamp;
    out.cpu_percent = raw.cpu_percent;
    out.memory_percent = raw.memory_percent;
    return out;
}

int CopySnapshotsToOutput(
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

ConfigRequest ToInternalRequest(const aura_config_request_t* request) {
    ConfigRequest out;
    if (request == nullptr) {
        return out;
    }

    if (request->cli_db_path != nullptr && request->cli_db_path[0] != '\0') {
        out.cli_db_path = std::string(request->cli_db_path);
    }
    out.no_persist = request->no_persist != 0;
    if (request->has_cli_retention != 0) {
        out.cli_retention_seconds = request->cli_retention_seconds;
    }
    if (request->config_path_override != nullptr && request->config_path_override[0] != '\0') {
        out.config_path_override = std::string(request->config_path_override);
    }
    return out;
}

int ToAbiDbSource(const DbSource source) {
    switch (source) {
        case DbSource::Cli:
            return AURA_DB_SOURCE_CLI;
        case DbSource::Env:
            return AURA_DB_SOURCE_ENV;
        case DbSource::Config:
            return AURA_DB_SOURCE_CONFIG;
        case DbSource::Auto:
            return AURA_DB_SOURCE_AUTO;
        case DbSource::Disabled:
            return AURA_DB_SOURCE_DISABLED;
    }
    return AURA_DB_SOURCE_AUTO;
}

void WriteConfigOutput(const RuntimeConfig& config, aura_runtime_config_t* out_config) {
    out_config->persistence_enabled = config.persistence_enabled ? 1 : 0;
    out_config->retention_seconds = config.retention_seconds;
    out_config->db_source = ToAbiDbSource(config.db_source);
    std::memset(out_config->db_path, 0, sizeof(out_config->db_path));
    if (!config.db_path.empty()) {
        const std::size_t max_copy = sizeof(out_config->db_path) - 1;
        std::strncpy(out_config->db_path, config.db_path.c_str(), max_copy);
        out_config->db_path[max_copy] = '\0';
    }
}

int HandleException(const std::exception& exc, aura_error_t* out_error) {
    SetError(out_error, AURA_ERR_RUNTIME, exc.what());
    return AURA_ERR_RUNTIME;
}

} // namespace

extern "C" {

AURA_PLATFORM_EXPORT const char* aura_platform_version(void) {
    return "1.0.0";
}

AURA_PLATFORM_EXPORT const char* aura_last_error_message(void) {
    return g_last_error.c_str();
}

AURA_PLATFORM_EXPORT int aura_config_resolve(
    const aura_config_request_t* request,
    aura_runtime_config_t* out_config,
    aura_error_t* out_error
) {
    if (out_config == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_config must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        const RuntimeConfig config = aura::platform::ResolveRuntimeConfig(ToInternalRequest(request));
        WriteConfigOutput(config, out_config);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_store_open(
    const char* db_path,
    const double retention_seconds,
    aura_store_t** out_store,
    aura_error_t* out_error
) {
    if (db_path == nullptr || out_store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "db_path and out_store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto wrapped = std::make_unique<AuraStore>(
            aura::platform::OpenStore(std::string(db_path), retention_seconds)
        );
        *out_store = reinterpret_cast<aura_store_t*>(wrapped.release());
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_store_append(
    aura_store_t* store,
    const aura_snapshot_t* snapshot,
    aura_error_t* out_error
) {
    if (store == nullptr || snapshot == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store and snapshot must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        typed_store->store->Append(ToInternalSnapshot(*snapshot));
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_collect_snapshot(
    aura_snapshot_t* out_snapshot,
    aura_error_t* out_error
) {
    if (out_snapshot == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_snapshot must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        const Snapshot snapshot = aura::platform::CollectSystemSnapshot();
        *out_snapshot = ToAbiSnapshot(snapshot);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_store_count(
    aura_store_t* store,
    int* out_count,
    aura_error_t* out_error
) {
    if (store == nullptr || out_count == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store and out_count must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        *out_count = typed_store->store->Count();
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_store_latest(
    aura_store_t* store,
    const int limit,
    aura_snapshot_t* out_snapshots,
    const int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        const std::vector<Snapshot> snapshots = typed_store->store->Latest(limit);
        return CopySnapshotsToOutput(snapshots, out_snapshots, out_capacity, out_count, out_error);
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_store_between(
    aura_store_t* store,
    const int has_start,
    const double start_timestamp,
    const int has_end,
    const double end_timestamp,
    aura_snapshot_t* out_snapshots,
    const int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        const std::optional<double> start = has_start != 0 ? std::optional<double>(start_timestamp) : std::nullopt;
        const std::optional<double> end = has_end != 0 ? std::optional<double>(end_timestamp) : std::nullopt;
        const std::vector<Snapshot> snapshots = typed_store->store->Between(start, end);
        return CopySnapshotsToOutput(snapshots, out_snapshots, out_capacity, out_count, out_error);
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_dvr_downsample_lttb(
    const aura_snapshot_t* input_snapshots,
    const int input_count,
    const int target,
    aura_snapshot_t* out_snapshots,
    const int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (input_count < 0) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "input_count must be >= 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (input_count > 0 && input_snapshots == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "input_snapshots must not be null when input_count > 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        std::vector<Snapshot> internal;
        internal.reserve(static_cast<std::size_t>(input_count));
        for (int i = 0; i < input_count; ++i) {
            internal.push_back(ToInternalSnapshot(input_snapshots[i]));
        }

        const std::vector<Snapshot> output = aura::platform::DownsampleLttb(internal, target);
        return CopySnapshotsToOutput(output, out_snapshots, out_capacity, out_count, out_error);
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_dvr_query_timeline(
    aura_store_t* store,
    const int has_start,
    const double start_timestamp,
    const int has_end,
    const double end_timestamp,
    const int resolution,
    aura_snapshot_t* out_snapshots,
    const int out_capacity,
    int* out_count,
    aura_error_t* out_error
) {
    if (store == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "store must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed_store = reinterpret_cast<AuraStore*>(store);
        const std::optional<double> start = has_start != 0 ? std::optional<double>(start_timestamp) : std::nullopt;
        const std::optional<double> end = has_end != 0 ? std::optional<double>(end_timestamp) : std::nullopt;
        const std::vector<Snapshot> output = aura::platform::QueryTimeline(*typed_store->store, start, end, resolution);
        return CopySnapshotsToOutput(output, out_snapshots, out_capacity, out_count, out_error);
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_store_close(aura_store_t* store) {
    if (store == nullptr) {
        return AURA_OK;
    }

    auto* typed_store = reinterpret_cast<AuraStore*>(store);
    delete typed_store;
    return AURA_OK;
}

} // extern "C"
