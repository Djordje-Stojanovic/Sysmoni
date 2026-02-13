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

#ifdef __cplusplus
}
#endif

#endif  // AURA_TELEMETRY_ABI_H
