#include "telemetry_engine.h"
#include "telemetry_engine_internal.hpp"

#include <algorithm>
#include <array>

namespace aura::telemetry {

NativeCollectors DefaultNativeCollectors() {
    NativeCollectors collectors{};
    collectors.collect_system_snapshot = aura_collect_system_snapshot;
    collectors.collect_processes = aura_collect_processes;
    collectors.collect_disk_counters = aura_collect_disk_counters;
    collectors.collect_network_counters = aura_collect_network_counters;
    collectors.collect_thermal_readings = aura_collect_thermal_readings;
    collectors.collect_per_core_cpu = aura_collect_per_core_cpu;
    collectors.collect_gpu_utilization = aura_collect_gpu_utilization;
    collectors.collect_process_details = aura_collect_process_details;
    collectors.build_process_tree = aura_build_process_tree;
    collectors.get_process_by_pid = aura_get_process_by_pid;
    collectors.terminate_process = aura_terminate_process;
    collectors.set_process_priority = aura_set_process_priority;
    collectors.get_process_children = aura_get_process_children;
    return collectors;
}

TelemetryEngine::TelemetryEngine(NativeCollectors collectors) : collectors_(collectors) {}

bool TelemetryEngine::CollectSystemSnapshot(
    double timestamp_seconds,
    SystemSnapshot* out_snapshot,
    std::string* error_message
) const {
    if (out_snapshot == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectSystemSnapshot requires out_snapshot.";
        }
        return false;
    }
    if (!detail::is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectSystemSnapshot requires finite timestamp.";
        }
        return false;
    }
    if (collectors_.collect_system_snapshot == nullptr) {
        if (error_message != nullptr) {
            *error_message = "System collector is not configured.";
        }
        return false;
    }

    std::array<char, 512> error_buffer{};
    double cpu_percent = 0.0;
    double memory_percent = 0.0;
    const int status = collectors_.collect_system_snapshot(
        &cpu_percent,
        &memory_percent,
        error_buffer.data(),
        error_buffer.size()
    );
    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = detail::build_status_error(
                "collect_system_snapshot",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->cpu_percent = detail::clamp_percent(cpu_percent);
    out_snapshot->memory_percent = detail::clamp_percent(memory_percent);
    detail::clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectTopProcesses(
    uint32_t limit,
    std::vector<ProcessSample>* out_samples,
    std::string* error_message
) const {
    if (out_samples == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectTopProcesses requires out_samples.";
        }
        return false;
    }
    if (limit == 0) {
        if (error_message != nullptr) {
            *error_message = "CollectTopProcesses requires limit > 0.";
        }
        return false;
    }
    if (collectors_.collect_processes == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process collector is not configured.";
        }
        return false;
    }

    const uint32_t collect_capacity = kMaxProcessSamples;
    std::vector<aura_process_sample> raw_samples(static_cast<size_t>(collect_capacity));
    uint32_t out_count = 0;
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_processes(
        raw_samples.data(),
        collect_capacity,
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );
    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = detail::build_status_error(
                "collect_processes",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    const uint32_t bounded_count = std::min(out_count, collect_capacity);
    out_samples->clear();
    out_samples->reserve(static_cast<size_t>(bounded_count));
    for (uint32_t i = 0; i < bounded_count; ++i) {
        const aura_process_sample& raw = raw_samples[static_cast<size_t>(i)];
        if (raw.pid == 0) {
            continue;
        }

        ProcessSample sample{};
        sample.pid = raw.pid;
        sample.name = detail::decode_fixed_utf8(raw.name, sizeof(raw.name));
        if (sample.name.empty()) {
            sample.name = "pid-" + std::to_string(sample.pid);
        }
        sample.cpu_percent = detail::clamp_percent(raw.cpu_percent);
        sample.memory_rss_bytes = raw.memory_rss_bytes;
        out_samples->push_back(std::move(sample));
    }

    std::sort(
        out_samples->begin(),
        out_samples->end(),
        [](const ProcessSample& left, const ProcessSample& right) {
            if (left.cpu_percent != right.cpu_percent) {
                return left.cpu_percent > right.cpu_percent;
            }
            if (left.memory_rss_bytes != right.memory_rss_bytes) {
                return left.memory_rss_bytes > right.memory_rss_bytes;
            }
            return left.pid < right.pid;
        }
    );

    if (out_samples->size() > limit) {
        out_samples->resize(limit);
    }
    detail::clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectDiskSnapshot(
    double timestamp_seconds,
    DiskSnapshot* out_snapshot,
    std::string* error_message
) {
    if (out_snapshot == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectDiskSnapshot requires out_snapshot.";
        }
        return false;
    }
    if (!detail::is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectDiskSnapshot requires finite timestamp.";
        }
        return false;
    }
    if (collectors_.collect_disk_counters == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Disk collector is not configured.";
        }
        return false;
    }

    aura_disk_counters current{};
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_disk_counters(
        &current,
        error_buffer.data(),
        error_buffer.size()
    );
    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->read_bytes_per_sec = 0.0;
    out_snapshot->write_bytes_per_sec = 0.0;
    out_snapshot->read_ops_per_sec = 0.0;
    out_snapshot->write_ops_per_sec = 0.0;
    out_snapshot->total_read_bytes = 0;
    out_snapshot->total_write_bytes = 0;

    if (status != AURA_STATUS_OK) {
        if (status != AURA_STATUS_UNAVAILABLE && error_message != nullptr) {
            *error_message = detail::build_status_error(
                "collect_disk_counters",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        if (status == AURA_STATUS_UNAVAILABLE) {
            detail::clear_error(error_message);
        }
        return status == AURA_STATUS_UNAVAILABLE;
    }

    out_snapshot->total_read_bytes = current.read_bytes;
    out_snapshot->total_write_bytes = current.write_bytes;

    std::lock_guard<std::mutex> lock(disk_mutex_);
    bool update_baseline = true;
    if (disk_state_.has_previous) {
        const double elapsed = timestamp_seconds - disk_state_.timestamp_seconds;
        if (elapsed <= 0.0) {
            update_baseline = false;
        } else {
            const bool counters_monotonic =
                current.read_bytes >= disk_state_.counters.read_bytes &&
                current.write_bytes >= disk_state_.counters.write_bytes &&
                current.read_count >= disk_state_.counters.read_count &&
                current.write_count >= disk_state_.counters.write_count;
            if (counters_monotonic) {
                out_snapshot->read_bytes_per_sec =
                    static_cast<double>(current.read_bytes - disk_state_.counters.read_bytes) /
                    elapsed;
                out_snapshot->write_bytes_per_sec =
                    static_cast<double>(current.write_bytes - disk_state_.counters.write_bytes) /
                    elapsed;
                out_snapshot->read_ops_per_sec =
                    static_cast<double>(current.read_count - disk_state_.counters.read_count) /
                    elapsed;
                out_snapshot->write_ops_per_sec =
                    static_cast<double>(current.write_count - disk_state_.counters.write_count) /
                    elapsed;
            }
        }
    }

    if (update_baseline) {
        disk_state_.has_previous = true;
        disk_state_.timestamp_seconds = timestamp_seconds;
        disk_state_.counters = current;
    }
    detail::clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectNetworkSnapshot(
    double timestamp_seconds,
    NetworkSnapshot* out_snapshot,
    std::string* error_message
) {
    if (out_snapshot == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectNetworkSnapshot requires out_snapshot.";
        }
        return false;
    }
    if (!detail::is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectNetworkSnapshot requires finite timestamp.";
        }
        return false;
    }
    if (collectors_.collect_network_counters == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Network collector is not configured.";
        }
        return false;
    }

    aura_network_counters current{};
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_network_counters(
        &current,
        error_buffer.data(),
        error_buffer.size()
    );
    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->bytes_sent_per_sec = 0.0;
    out_snapshot->bytes_recv_per_sec = 0.0;
    out_snapshot->packets_sent_per_sec = 0.0;
    out_snapshot->packets_recv_per_sec = 0.0;
    out_snapshot->total_bytes_sent = 0;
    out_snapshot->total_bytes_recv = 0;

    if (status != AURA_STATUS_OK) {
        if (status != AURA_STATUS_UNAVAILABLE && error_message != nullptr) {
            *error_message = detail::build_status_error(
                "collect_network_counters",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        if (status == AURA_STATUS_UNAVAILABLE) {
            detail::clear_error(error_message);
        }
        return status == AURA_STATUS_UNAVAILABLE;
    }

    out_snapshot->total_bytes_sent = current.bytes_sent;
    out_snapshot->total_bytes_recv = current.bytes_recv;

    std::lock_guard<std::mutex> lock(network_mutex_);
    bool update_baseline = true;
    if (network_state_.has_previous) {
        const double elapsed = timestamp_seconds - network_state_.timestamp_seconds;
        if (elapsed <= 0.0) {
            update_baseline = false;
        } else {
            const bool counters_monotonic =
                current.bytes_sent >= network_state_.counters.bytes_sent &&
                current.bytes_recv >= network_state_.counters.bytes_recv &&
                current.packets_sent >= network_state_.counters.packets_sent &&
                current.packets_recv >= network_state_.counters.packets_recv;
            if (counters_monotonic) {
                out_snapshot->bytes_sent_per_sec =
                    static_cast<double>(current.bytes_sent - network_state_.counters.bytes_sent) /
                    elapsed;
                out_snapshot->bytes_recv_per_sec =
                    static_cast<double>(current.bytes_recv - network_state_.counters.bytes_recv) /
                    elapsed;
                out_snapshot->packets_sent_per_sec =
                    static_cast<double>(current.packets_sent - network_state_.counters.packets_sent) /
                    elapsed;
                out_snapshot->packets_recv_per_sec =
                    static_cast<double>(current.packets_recv - network_state_.counters.packets_recv) /
                    elapsed;
            }
        }
    }

    if (update_baseline) {
        network_state_.has_previous = true;
        network_state_.timestamp_seconds = timestamp_seconds;
        network_state_.counters = current;
    }
    detail::clear_error(error_message);
    return true;
}

}  // namespace aura::telemetry
