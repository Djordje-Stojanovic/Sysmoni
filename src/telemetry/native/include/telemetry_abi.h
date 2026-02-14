#ifndef AURA_TELEMETRY_ABI_H
#define AURA_TELEMETRY_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AURA_EXPORT __declspec(dllexport)
#else
#define AURA_EXPORT
#endif

enum aura_status {
    AURA_STATUS_OK = 0,
    AURA_STATUS_UNAVAILABLE = 1,
    AURA_STATUS_ERROR = 2,
};

struct aura_process_sample {
    uint32_t pid;
    char name[260];
    double cpu_percent;
    uint64_t memory_rss_bytes;
};

/* Extended process detail for process management panel */
struct aura_process_detail {
    uint32_t pid;                    /* Process ID */
    uint32_t parent_pid;             /* Parent Process ID for tree building */
    char name[260];                  /* Process executable name */
    char command_line[2048];         /* Full command line arguments */
    double cpu_percent;              /* CPU usage percent */
    uint64_t memory_rss_bytes;       /* Working set size */
    uint64_t memory_private_bytes;   /* Private bytes (non-shared) */
    uint64_t memory_peak_bytes;      /* Peak working set */
    uint32_t thread_count;           /* Number of threads */
    uint32_t handle_count;           /* Number of handles */
    uint32_t priority_class;         /* Process priority class enum */
    uint64_t start_time_100ns;       /* Process start time (FILETIME format) */
};

/* Process tree node for hierarchy */
struct aura_process_tree_node {
    uint32_t pid;
    uint32_t depth;                  /* Tree depth (0 = root) */
    uint32_t child_count;            /* Number of direct children */
    uint8_t has_children;            /* Flag: 1 if has children, 0 otherwise */
};

/* Process enumeration options */
struct aura_process_query_options {
    uint32_t max_results;            /* Maximum processes to return */
    uint8_t include_tree;            /* Build process tree structure */
    uint8_t include_command_line;     /* Include command line data */
    uint8_t sort_column;             /* 0=pid, 1=name, 2=cpu, 3=memory, 4=threads */
    uint8_t sort_descending;         /* Sort order: 1=desc, 0=asc */
    char name_filter[128];           /* Filter by name prefix (empty = all) */
};

/* Priority class values (Windows-specific, maps to SetPriorityClass) */
enum aura_process_priority {
    AURA_PRIORITY_IDLE = 0x40,
    AURA_PRIORITY_NORMAL = 0x20,
    AURA_PRIORITY_HIGH = 0x80,
    AURA_PRIORITY_REALTIME = 0x100,
    AURA_PRIORITY_BELOW_NORMAL = 0x4000,
    AURA_PRIORITY_ABOVE_NORMAL = 0x8000
};

struct aura_disk_counters {
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t read_count;
    uint64_t write_count;
};

struct aura_network_counters {
    uint64_t bytes_sent;
    uint64_t bytes_recv;
    uint64_t packets_sent;
    uint64_t packets_recv;
};

struct aura_thermal_reading {
    char label[128];
    double current_celsius;
    double high_celsius;
    double critical_celsius;
    uint8_t has_high;
    uint8_t has_critical;
};

struct aura_per_core_cpu {
    double* percents;
    uint32_t count;
};

struct aura_gpu_utilization {
    double gpu_percent;
    double vram_percent;
    uint64_t vram_used_bytes;
    uint64_t vram_total_bytes;
};

#ifdef __cplusplus
extern "C" {
#endif

AURA_EXPORT int aura_collect_system_snapshot(
    double* cpu_percent,
    double* memory_percent,
    char* error_buffer,
    size_t error_buffer_len
);

AURA_EXPORT int aura_collect_processes(
    aura_process_sample* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
);

AURA_EXPORT int aura_collect_disk_counters(
    aura_disk_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
);

AURA_EXPORT int aura_collect_network_counters(
    aura_network_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
);

AURA_EXPORT int aura_collect_thermal_readings(
    aura_thermal_reading* readings,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
);

AURA_EXPORT int aura_collect_per_core_cpu(
    double* out_percents,
    uint32_t max_cores,
    uint32_t* out_core_count,
    char* error_buffer,
    size_t error_buffer_len
);

AURA_EXPORT int aura_collect_gpu_utilization(
    aura_gpu_utilization* out_gpu,
    char* error_buffer,
    size_t error_buffer_len
);

/*
 * Collect extended process details with sorting and filtering
 *
 * Returns AURA_STATUS_OK on success, error code otherwise.
 * Fills samples array with up to max_results entries.
 * Actual count written to out_count.
 */
AURA_EXPORT int aura_collect_process_details(
    const aura_process_query_options* options,
    aura_process_detail* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
);

/*
 * Build process tree structure from collected processes
 *
 * Expects process_details array to be sorted and populated.
 * Tree nodes array must match size and order of process_details.
 */
AURA_EXPORT int aura_build_process_tree(
    const aura_process_detail* process_details,
    uint32_t process_count,
    aura_process_tree_node* tree_nodes,
    uint32_t max_nodes,
    uint32_t* out_node_count,
    char* error_buffer,
    size_t error_buffer_len
);

/*
 * Get detailed information for a single process by PID
 *
 * Returns AURA_STATUS_OK on success, AURA_STATUS_NOT_FOUND if PID doesn't exist.
 */
AURA_EXPORT int aura_get_process_by_pid(
    uint32_t pid,
    aura_process_detail* out_detail,
    char* error_buffer,
    size_t error_buffer_len
);

/*
 * Terminate a process by PID
 *
 * Returns AURA_STATUS_OK on success, error code otherwise.
 * WARNING: This is destructive - confirm with user before calling.
 */
AURA_EXPORT int aura_terminate_process(
    uint32_t pid,
    uint32_t exit_code,
    char* error_buffer,
    size_t error_buffer_len
);

/*
 * Set process priority class
 *
 * Returns AURA_STATUS_OK on success, error code otherwise.
 */
AURA_EXPORT int aura_set_process_priority(
    uint32_t pid,
    uint32_t priority_class,
    char* error_buffer,
    size_t error_buffer_len
);

/*
 * Get process child PIDs (for tree expansion)
 *
 * Returns array of PIDs that are direct children.
 */
AURA_EXPORT int aura_get_process_children(
    uint32_t pid,
    uint32_t* child_pids,
    uint32_t max_children,
    uint32_t* out_child_count,
    char* error_buffer,
    size_t error_buffer_len
);

#ifdef __cplusplus
}
#endif

#endif  // AURA_TELEMETRY_ABI_H
