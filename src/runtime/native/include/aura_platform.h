#pragma once

#include <stddef.h>

#define AURA_PLATFORM_ABI_VERSION 1

#ifdef _WIN32
#define AURA_PLATFORM_EXPORT __declspec(dllexport)
#else
#define AURA_PLATFORM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aura_store aura_store_t;

typedef struct aura_snapshot_t {
    double timestamp;
    double cpu_percent;
    double memory_percent;
    double disk_read_bps;
    double disk_write_bps;
} aura_snapshot_t;

enum aura_db_source_t {
    AURA_DB_SOURCE_CLI = 0,
    AURA_DB_SOURCE_ENV = 1,
    AURA_DB_SOURCE_CONFIG = 2,
    AURA_DB_SOURCE_AUTO = 3,
    AURA_DB_SOURCE_DISABLED = 4
};

enum aura_error_code_t {
    AURA_OK = 0,
    AURA_ERR_INVALID_ARGUMENT = 1,
    AURA_ERR_RUNTIME = 2,
    AURA_ERR_IO = 3,
    AURA_ERR_STORE = 4,
    AURA_ERR_CAPACITY = 5
};

typedef struct aura_error_t {
    int code;
    char message[512];
} aura_error_t;

typedef struct aura_runtime_config_t {
    int persistence_enabled;
    double retention_seconds;
    int db_source;
    char db_path[1024];
} aura_runtime_config_t;

typedef struct aura_config_request_t {
    const char* cli_db_path;
    int no_persist;
    int has_cli_retention;
    double cli_retention_seconds;
    const char* config_path_override;
} aura_config_request_t;

AURA_PLATFORM_EXPORT const char* aura_platform_version(void);
AURA_PLATFORM_EXPORT const char* aura_last_error_message(void);

AURA_PLATFORM_EXPORT int aura_config_resolve(
    const aura_config_request_t* request,
    aura_runtime_config_t* out_config,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_store_open(
    const char* db_path,
    double retention_seconds,
    aura_store_t** out_store,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_store_append(
    aura_store_t* store,
    const aura_snapshot_t* snapshot,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_collect_snapshot(
    aura_snapshot_t* out_snapshot,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_store_count(
    aura_store_t* store,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_store_latest(
    aura_store_t* store,
    int limit,
    aura_snapshot_t* out_snapshots,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_store_between(
    aura_store_t* store,
    int has_start,
    double start_timestamp,
    int has_end,
    double end_timestamp,
    aura_snapshot_t* out_snapshots,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_dvr_downsample_lttb(
    const aura_snapshot_t* input_snapshots,
    int input_count,
    int target,
    aura_snapshot_t* out_snapshots,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_dvr_query_timeline(
    aura_store_t* store,
    int has_start,
    double start_timestamp,
    int has_end,
    double end_timestamp,
    int resolution,
    aura_snapshot_t* out_snapshots,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_store_close(aura_store_t* store);

#ifdef __cplusplus
}
#endif
