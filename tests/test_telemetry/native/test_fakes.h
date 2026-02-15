#pragma once

#include "telemetry_engine.h"
#include <cstdint>
#include <cstring>
#include <vector>

// --- Extern global state (definitions in test_fakes.cpp) ---
extern int g_system_status;
extern double g_system_cpu_percent;
extern double g_system_memory_percent;

extern int g_process_status;
extern std::vector<aura_process_sample> g_process_samples;

extern int g_disk_status;
extern std::vector<aura_disk_counters> g_disk_sequence;
extern size_t g_disk_index;

extern int g_network_status;
extern std::vector<aura_network_counters> g_network_sequence;
extern size_t g_network_index;

extern int g_thermal_status;
extern std::vector<aura_thermal_reading> g_thermal_sequence;

extern int g_per_core_cpu_status;
extern std::vector<double> g_per_core_cpu_percents;

extern int g_gpu_status;
extern aura_gpu_utilization g_gpu_data;

extern int g_process_by_pid_status;
extern aura_process_detail g_process_detail_data;

extern int g_terminate_process_status;
extern int g_set_process_priority_status;
extern int g_get_children_status;

// Fake collector declarations
int fake_collect_system_snapshot(double*, double*, char*, size_t);
int fake_collect_processes(aura_process_sample*, uint32_t, uint32_t*, char*, size_t);
int fake_collect_disk_counters(aura_disk_counters*, char*, size_t);
int fake_collect_network_counters(aura_network_counters*, char*, size_t);
int fake_collect_thermal_readings(aura_thermal_reading*, uint32_t, uint32_t*, char*, size_t);
int fake_collect_per_core_cpu(double*, uint32_t, uint32_t*, char*, size_t);
int fake_collect_gpu_utilization(aura_gpu_utilization*, char*, size_t);
int fake_get_process_by_pid(uint32_t, aura_process_detail*, char*, size_t);
int fake_collect_process_details(const aura_process_query_options*, aura_process_detail*, uint32_t, uint32_t*, char*, size_t);
int fake_build_process_tree(const aura_process_detail*, uint32_t, aura_process_tree_node*, uint32_t, uint32_t*, char*, size_t);
int fake_terminate_process(uint32_t, uint32_t, char*, size_t);
int fake_set_process_priority(uint32_t, uint32_t, char*, size_t);
int fake_get_process_children(uint32_t, uint32_t*, uint32_t, uint32_t*, char*, size_t);

aura::telemetry::NativeCollectors make_collectors();
void write_error(char*, size_t, const char*);
