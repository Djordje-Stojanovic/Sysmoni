#include "test_fakes.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

int g_system_status = AURA_STATUS_OK;
double g_system_cpu_percent = 12.5;
double g_system_memory_percent = 42.0;

int g_process_status = AURA_STATUS_OK;
std::vector<aura_process_sample> g_process_samples;

int g_disk_status = AURA_STATUS_OK;
std::vector<aura_disk_counters> g_disk_sequence;
size_t g_disk_index = 0;

int g_network_status = AURA_STATUS_OK;
std::vector<aura_network_counters> g_network_sequence;
size_t g_network_index = 0;

int g_thermal_status = AURA_STATUS_UNAVAILABLE;
std::vector<aura_thermal_reading> g_thermal_sequence;

int g_per_core_cpu_status = AURA_STATUS_UNAVAILABLE;
std::vector<double> g_per_core_cpu_percents;

int g_gpu_status = AURA_STATUS_UNAVAILABLE;
aura_gpu_utilization g_gpu_data{};

int g_process_by_pid_status = AURA_STATUS_OK;
aura_process_detail g_process_detail_data{};

int g_terminate_process_status = AURA_STATUS_OK;

int g_set_process_priority_status = AURA_STATUS_OK;

int g_get_children_status = AURA_STATUS_OK;

void write_error(char* error_buffer, size_t error_buffer_len, const char* message) {
    if (error_buffer == nullptr || error_buffer_len == 0) {
        return;
    }
    error_buffer[0] = '\0';
    if (message == nullptr || message[0] == '\0') {
        return;
    }
    std::strncpy(error_buffer, message, error_buffer_len - 1);
    error_buffer[error_buffer_len - 1] = '\0';
}

int fake_collect_system_snapshot(
    double* cpu_percent,
    double* memory_percent,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_system_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "system failed");
        return g_system_status;
    }
    *cpu_percent = g_system_cpu_percent;
    *memory_percent = g_system_memory_percent;
    return AURA_STATUS_OK;
}

int fake_collect_processes(
    aura_process_sample* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_process_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "process failed");
        if (out_count != nullptr) {
            *out_count = 0;
        }
        return g_process_status;
    }
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(g_process_samples.size(), max_samples));
    for (uint32_t i = 0; i < count; ++i) {
        samples[i] = g_process_samples[i];
    }
    if (out_count != nullptr) {
        *out_count = count;
    }
    return AURA_STATUS_OK;
}

int fake_collect_disk_counters(
    aura_disk_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_disk_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "disk failed");
        return g_disk_status;
    }
    if (g_disk_sequence.empty()) {
        write_error(error_buffer, error_buffer_len, "disk sequence empty");
        return AURA_STATUS_ERROR;
    }
    const size_t index = std::min(g_disk_index, g_disk_sequence.size() - 1U);
    *counters = g_disk_sequence[index];
    ++g_disk_index;
    return AURA_STATUS_OK;
}

int fake_collect_network_counters(
    aura_network_counters* counters,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_network_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "network failed");
        return g_network_status;
    }
    if (g_network_sequence.empty()) {
        write_error(error_buffer, error_buffer_len, "network sequence empty");
        return AURA_STATUS_ERROR;
    }
    const size_t index = std::min(g_network_index, g_network_sequence.size() - 1U);
    *counters = g_network_sequence[index];
    ++g_network_index;
    return AURA_STATUS_OK;
}

int fake_collect_thermal_readings(
    aura_thermal_reading* readings,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_thermal_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "thermal unavailable");
        if (out_count != nullptr) {
            *out_count = 0;
        }
        return g_thermal_status;
    }

    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(g_thermal_sequence.size(), max_samples));
    for (uint32_t i = 0; i < count; ++i) {
        readings[i] = g_thermal_sequence[i];
    }
    if (out_count != nullptr) {
        *out_count = count;
    }
    return AURA_STATUS_OK;
}

int fake_collect_per_core_cpu(
    double* out_percents,
    uint32_t max_cores,
    uint32_t* out_core_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_per_core_cpu_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "per-core cpu unavailable");
        if (out_core_count != nullptr) {
            *out_core_count = 0;
        }
        return g_per_core_cpu_status;
    }
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(g_per_core_cpu_percents.size(), max_cores));
    for (uint32_t i = 0; i < count; ++i) {
        out_percents[i] = g_per_core_cpu_percents[i];
    }
    if (out_core_count != nullptr) {
        *out_core_count = count;
    }
    return AURA_STATUS_OK;
}

int fake_collect_gpu_utilization(
    aura_gpu_utilization* out_gpu,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_gpu_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "gpu unavailable");
        return g_gpu_status;
    }
    if (out_gpu != nullptr) {
        *out_gpu = g_gpu_data;
    }
    return AURA_STATUS_OK;
}

int fake_get_process_by_pid(
    uint32_t pid,
    aura_process_detail* out_detail,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (g_process_by_pid_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "process detail unavailable");
        return g_process_by_pid_status;
    }
    if (out_detail != nullptr) {
        *out_detail = g_process_detail_data;
        out_detail->pid = pid;
    }
    return AURA_STATUS_OK;
}

int fake_collect_process_details(
    const aura_process_query_options* options,
    aura_process_detail* samples,
    uint32_t max_samples,
    uint32_t* out_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    if (options == nullptr) {
        write_error(error_buffer, error_buffer_len, "null options");
        return AURA_STATUS_ERROR;
    }

    const uint32_t count = static_cast<uint32_t>(std::min(static_cast<size_t>(options->max_results), static_cast<size_t>(max_samples)));

    if (g_process_by_pid_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "process details unavailable");
        return g_process_by_pid_status;
    }

    for (uint32_t i = 0; i < count; ++i) {
        samples[i] = g_process_detail_data;
        samples[i].pid = i + 1;
    }

    if (out_count != nullptr) {
        *out_count = count;
    }

    return AURA_STATUS_OK;
}

int fake_build_process_tree(
    const aura_process_detail* process_details,
    uint32_t process_count,
    aura_process_tree_node* tree_nodes,
    uint32_t max_nodes,
    uint32_t* out_node_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)process_details;
    const uint32_t count = std::min(process_count, max_nodes);

    if (out_node_count != nullptr) {
        *out_node_count = count;
    }

    for (uint32_t i = 0; i < count; ++i) {
        tree_nodes[i].pid = i + 1;
        tree_nodes[i].depth = 0;
        tree_nodes[i].child_count = 0;
        tree_nodes[i].has_children = 0;
    }

    return AURA_STATUS_OK;
}

int fake_terminate_process(
    uint32_t pid,
    uint32_t exit_code,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)pid;
    (void)exit_code;
    if (g_terminate_process_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "terminate unavailable");
        return g_terminate_process_status;
    }
    return AURA_STATUS_OK;
}

int fake_set_process_priority(
    uint32_t pid,
    uint32_t priority_class,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)pid;
    (void)priority_class;
    if (g_set_process_priority_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "set priority unavailable");
        return g_set_process_priority_status;
    }
    return AURA_STATUS_OK;
}

int fake_get_process_children(
    uint32_t pid,
    uint32_t* child_pids,
    uint32_t max_children,
    uint32_t* out_child_count,
    char* error_buffer,
    size_t error_buffer_len
) {
    (void)pid;
    if (g_get_children_status != AURA_STATUS_OK) {
        write_error(error_buffer, error_buffer_len, "get children unavailable");
        return g_get_children_status;
    }

    const uint32_t count = std::min(static_cast<uint32_t>(2), max_children);
    if (out_child_count != nullptr) {
        *out_child_count = count;
    }

    for (uint32_t i = 0; i < count; ++i) {
        child_pids[i] = (i + 1) * 10;
    }

    return AURA_STATUS_OK;
}

aura::telemetry::NativeCollectors make_collectors() {
    aura::telemetry::NativeCollectors collectors{};
    collectors.collect_system_snapshot = fake_collect_system_snapshot;
    collectors.collect_processes = fake_collect_processes;
    collectors.collect_disk_counters = fake_collect_disk_counters;
    collectors.collect_network_counters = fake_collect_network_counters;
    collectors.collect_thermal_readings = fake_collect_thermal_readings;
    collectors.collect_per_core_cpu = fake_collect_per_core_cpu;
    collectors.collect_gpu_utilization = fake_collect_gpu_utilization;
    collectors.collect_process_details = fake_collect_process_details;
    collectors.build_process_tree = fake_build_process_tree;
    collectors.get_process_by_pid = fake_get_process_by_pid;
    collectors.terminate_process = fake_terminate_process;
    collectors.set_process_priority = fake_set_process_priority;
    collectors.get_process_children = fake_get_process_children;
    return collectors;
}
