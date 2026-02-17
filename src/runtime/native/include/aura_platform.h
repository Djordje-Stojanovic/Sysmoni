#pragma once

#include <stddef.h>

#define AURA_PLATFORM_ABI_VERSION 2

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
    double net_recv_bps;
    double net_sent_bps;
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

/* -----------------------------------------------------------------------
 * Alert Engine
 * ----------------------------------------------------------------------- */

typedef struct aura_alert_engine aura_alert_engine_t;

enum aura_alert_metric_t {
    AURA_METRIC_CPU_PERCENT    = 0,
    AURA_METRIC_MEMORY_PERCENT = 1,
    AURA_METRIC_DISK_READ_BPS  = 2,
    AURA_METRIC_DISK_WRITE_BPS = 3,
    AURA_METRIC_NET_RECV_BPS   = 4,
    AURA_METRIC_NET_SENT_BPS   = 5
};

enum aura_alert_comparator_t {
    AURA_COMPARATOR_ABOVE = 0,
    AURA_COMPARATOR_BELOW = 1
};

enum aura_alert_state_t {
    AURA_ALERT_IDLE      = 0,
    AURA_ALERT_PENDING   = 1,
    AURA_ALERT_TRIGGERED = 2,
    AURA_ALERT_COOLDOWN  = 3
};

typedef struct aura_alert_rule_t {
    int    id;
    int    metric;
    int    comparator;
    double threshold;
    double sustained_seconds;
    double cooldown_seconds;
    char   name[256];
} aura_alert_rule_t;

typedef struct aura_alert_status_t {
    int    rule_id;
    int    state;
    double last_value;
    double peak_value;
    double triggered_at;
    double duration;
    int    acknowledged;
} aura_alert_status_t;

typedef struct aura_alert_event_t {
    int    rule_id;
    int    event_type;
    double timestamp;
    double value;
} aura_alert_event_t;

AURA_PLATFORM_EXPORT int aura_alert_engine_create(
    aura_alert_engine_t** out_engine,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_destroy(
    aura_alert_engine_t* engine
);

AURA_PLATFORM_EXPORT int aura_alert_engine_add_rule(
    aura_alert_engine_t* engine,
    const aura_alert_rule_t* rule,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_remove_rule(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_get_rule(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_alert_rule_t* out_rule,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_rule_count(
    aura_alert_engine_t* engine,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_evaluate(
    aura_alert_engine_t* engine,
    const aura_snapshot_t* snapshot,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_get_status(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_alert_status_t* out_status,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_get_active(
    aura_alert_engine_t* engine,
    aura_alert_status_t* out_statuses,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_acknowledge(
    aura_alert_engine_t* engine,
    int rule_id,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_get_history(
    aura_alert_engine_t* engine,
    aura_alert_event_t* out_events,
    int out_capacity,
    int* out_count,
    aura_error_t* out_error
);

AURA_PLATFORM_EXPORT int aura_alert_engine_clear_history(
    aura_alert_engine_t* engine,
    aura_error_t* out_error
);

#ifdef __cplusplus
}
#endif
