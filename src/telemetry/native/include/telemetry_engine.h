#ifndef AURA_TELEMETRY_ENGINE_H
#define AURA_TELEMETRY_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "telemetry_abi.h"

namespace aura::telemetry {

struct SystemSnapshot {
    double timestamp_seconds = 0.0;
    double cpu_percent = 0.0;
    double memory_percent = 0.0;
};

struct ProcessSample {
    uint32_t pid = 0;
    std::string name;
    double cpu_percent = 0.0;
    uint64_t memory_rss_bytes = 0;
};

/* Extended process detail for process management panel */
struct ProcessDetail {
    uint32_t pid = 0;
    uint32_t parent_pid = 0;
    std::string name;
    std::string command_line;
    double cpu_percent = 0.0;
    uint64_t memory_rss_bytes = 0;
    uint64_t memory_private_bytes = 0;
    uint64_t memory_peak_bytes = 0;
    uint32_t thread_count = 0;
    uint32_t handle_count = 0;
    uint32_t priority_class = 0;
    uint64_t start_time_100ns = 0;
};

/* Process tree node for hierarchy */
struct ProcessTreeNode {
    uint32_t pid = 0;
    uint32_t depth = 0;
    uint32_t child_count = 0;
    bool has_children = false;
};

/* Process enumeration options */
struct ProcessQueryOptions {
    uint32_t max_results = 256;
    bool include_tree = true;
    bool include_command_line = true;
    uint8_t sort_column = 2;  /* 0=pid, 1=name, 2=cpu, 3=memory, 4=threads */
    bool sort_descending = true;
    std::string name_filter;
};

struct DiskSnapshot {
    double timestamp_seconds = 0.0;
    double read_bytes_per_sec = 0.0;
    double write_bytes_per_sec = 0.0;
    double read_ops_per_sec = 0.0;
    double write_ops_per_sec = 0.0;
    uint64_t total_read_bytes = 0;
    uint64_t total_write_bytes = 0;
};

struct NetworkSnapshot {
    double timestamp_seconds = 0.0;
    double bytes_sent_per_sec = 0.0;
    double bytes_recv_per_sec = 0.0;
    double packets_sent_per_sec = 0.0;
    double packets_recv_per_sec = 0.0;
    uint64_t total_bytes_sent = 0;
    uint64_t total_bytes_recv = 0;
};

struct ThermalReading {
    std::string label;
    double current_celsius = 0.0;
    std::optional<double> high_celsius;
    std::optional<double> critical_celsius;
};

struct ThermalSnapshot {
    double timestamp_seconds = 0.0;
    std::vector<ThermalReading> readings;
    std::optional<double> hottest_celsius;
};

struct PerCoreCpuSnapshot {
    double timestamp_seconds = 0.0;
    std::vector<double> core_percents;
};

struct GpuSnapshot {
    double timestamp_seconds = 0.0;
    bool available = false;
    double gpu_percent = 0.0;
    double vram_percent = 0.0;
    uint64_t vram_used_bytes = 0;
    uint64_t vram_total_bytes = 0;
};

using CollectSystemSnapshotFn = int (*)(double*, double*, char*, size_t);
using CollectProcessesFn = int (*)(
    aura_process_sample*,
    uint32_t,
    uint32_t*,
    char*,
    size_t
);
using CollectDiskCountersFn = int (*)(aura_disk_counters*, char*, size_t);
using CollectNetworkCountersFn = int (*)(aura_network_counters*, char*, size_t);
using CollectThermalReadingsFn = int (*)(
    aura_thermal_reading*,
    uint32_t,
    uint32_t*,
    char*,
    size_t
);
using CollectPerCoreCpuFn = int (*)(double*, uint32_t, uint32_t*, char*, size_t);
using CollectGpuUtilizationFn = int (*)(aura_gpu_utilization*, char*, size_t);
using CollectProcessDetailsFn = int (*)(
    const aura_process_query_options*,
    aura_process_detail*,
    uint32_t,
    uint32_t*,
    char*,
    size_t
);
using BuildProcessTreeFn = int (*)(
    const aura_process_detail*,
    uint32_t,
    aura_process_tree_node*,
    uint32_t,
    uint32_t*,
    char*,
    size_t
);
using GetProcessByPidFn = int (*)(uint32_t, aura_process_detail*, char*, size_t);
using TerminateProcessFn = int (*)(uint32_t, uint32_t, char*, size_t);
using SetProcessPriorityFn = int (*)(uint32_t, uint32_t, char*, size_t);
using GetProcessChildrenFn = int (*)(
    uint32_t,
    uint32_t*,
    uint32_t,
    uint32_t*,
    char*,
    size_t
);

struct NativeCollectors {
    CollectSystemSnapshotFn collect_system_snapshot = nullptr;
    CollectProcessesFn collect_processes = nullptr;
    CollectDiskCountersFn collect_disk_counters = nullptr;
    CollectNetworkCountersFn collect_network_counters = nullptr;
    CollectThermalReadingsFn collect_thermal_readings = nullptr;
    CollectPerCoreCpuFn collect_per_core_cpu = nullptr;
    CollectGpuUtilizationFn collect_gpu_utilization = nullptr;
    CollectProcessDetailsFn collect_process_details = nullptr;
    BuildProcessTreeFn build_process_tree = nullptr;
    GetProcessByPidFn get_process_by_pid = nullptr;
    TerminateProcessFn terminate_process = nullptr;
    SetProcessPriorityFn set_process_priority = nullptr;
    GetProcessChildrenFn get_process_children = nullptr;
};

NativeCollectors DefaultNativeCollectors();

class TelemetryEngine {
public:
    explicit TelemetryEngine(NativeCollectors collectors = DefaultNativeCollectors());

    bool CollectSystemSnapshot(
        double timestamp_seconds,
        SystemSnapshot* out_snapshot,
        std::string* error_message
    ) const;

    bool CollectTopProcesses(
        uint32_t limit,
        std::vector<ProcessSample>* out_samples,
        std::string* error_message
    ) const;

    bool CollectDiskSnapshot(
        double timestamp_seconds,
        DiskSnapshot* out_snapshot,
        std::string* error_message
    );

    bool CollectNetworkSnapshot(
        double timestamp_seconds,
        NetworkSnapshot* out_snapshot,
        std::string* error_message
    );

    bool CollectThermalSnapshot(
        double timestamp_seconds,
        ThermalSnapshot* out_snapshot,
        std::string* error_message
    ) const noexcept;

    bool CollectPerCoreCpu(
        double timestamp_seconds,
        PerCoreCpuSnapshot* out_snapshot,
        std::string* error_message
    ) const;

    bool CollectGpuSnapshot(
        double timestamp_seconds,
        GpuSnapshot* out_snapshot,
        std::string* error_message
    ) const noexcept;

    bool CollectProcessDetails(
        const ProcessQueryOptions& options,
        std::vector<ProcessDetail>* out_details,
        std::string* error_message
    ) const noexcept;

    bool BuildProcessTree(
        const std::vector<ProcessDetail>& process_details,
        std::vector<ProcessTreeNode>* out_tree_nodes,
        std::string* error_message
    ) const noexcept;

    bool GetProcessByPid(
        uint32_t pid,
        ProcessDetail* out_detail,
        std::string* error_message
    ) const noexcept;

    bool TerminateProcess(
        uint32_t pid,
        uint32_t exit_code,
        std::string* error_message
    ) const noexcept;

    bool SetProcessPriority(
        uint32_t pid,
        uint32_t priority_class,
        std::string* error_message
    ) const noexcept;

    bool GetProcessChildren(
        uint32_t pid,
        std::vector<uint32_t>* out_child_pids,
        std::string* error_message
    ) const noexcept;

private:
    struct DiskState {
        bool has_previous = false;
        double timestamp_seconds = 0.0;
        aura_disk_counters counters{};
    };

    struct NetworkState {
        bool has_previous = false;
        double timestamp_seconds = 0.0;
        aura_network_counters counters{};
    };

    static constexpr uint32_t kMaxProcessSamples = 256;
    static constexpr uint32_t kMaxThermalReadings = 256;
    static constexpr uint32_t kMaxCores = 256;

    NativeCollectors collectors_;

    std::mutex disk_mutex_;
    std::mutex network_mutex_;
    DiskState disk_state_;
    NetworkState network_state_;
};

}  // namespace aura::telemetry

#endif  // AURA_TELEMETRY_ENGINE_H
