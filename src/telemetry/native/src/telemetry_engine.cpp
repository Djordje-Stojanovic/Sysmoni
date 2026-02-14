#include "telemetry_engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace aura::telemetry {

namespace {

constexpr double kCelsiusMin = -30.0;
constexpr double kCelsiusMax = 150.0;
constexpr double kCelsiusOptionalMax = 250.0;

bool is_finite(double value) {
    return std::isfinite(value) != 0;
}

double clamp_percent(double value) {
    if (!is_finite(value)) {
        return 0.0;
    }
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 100.0) {
        return 100.0;
    }
    return value;
}

std::string decode_error_buffer(const char* error_buffer, size_t error_buffer_len) {
    if (error_buffer == nullptr || error_buffer_len == 0) {
        return {};
    }
    size_t used = 0;
    while (used < error_buffer_len && error_buffer[used] != '\0') {
        ++used;
    }
    return std::string(error_buffer, used);
}

std::string build_status_error(
    const char* operation,
    int status,
    const char* error_buffer,
    size_t error_buffer_len
) {
    std::ostringstream oss;
    oss << operation << " failed with status=" << status;
    std::string native_message = decode_error_buffer(error_buffer, error_buffer_len);
    if (!native_message.empty()) {
        oss << ": " << native_message;
    }
    return oss.str();
}

std::string decode_fixed_utf8(const char* data, size_t data_len) {
    if (data == nullptr || data_len == 0) {
        return {};
    }
    size_t used = 0;
    while (used < data_len && data[used] != '\0') {
        ++used;
    }
    std::string decoded(data, used);
    const auto first_non_ws = decoded.find_first_not_of(" \t\r\n");
    if (first_non_ws == std::string::npos) {
        return {};
    }
    const auto last_non_ws = decoded.find_last_not_of(" \t\r\n");
    return decoded.substr(first_non_ws, last_non_ws - first_non_ws + 1);
}

void clear_error(std::string* error_message) {
    if (error_message != nullptr) {
        error_message->clear();
    }
}

}  // namespace

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
    if (!is_finite(timestamp_seconds)) {
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
            *error_message = build_status_error(
                "collect_system_snapshot",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->cpu_percent = clamp_percent(cpu_percent);
    out_snapshot->memory_percent = clamp_percent(memory_percent);
    clear_error(error_message);
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
            *error_message = build_status_error(
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
        sample.name = decode_fixed_utf8(raw.name, sizeof(raw.name));
        if (sample.name.empty()) {
            sample.name = "pid-" + std::to_string(sample.pid);
        }
        sample.cpu_percent = clamp_percent(raw.cpu_percent);
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
    clear_error(error_message);
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
    if (!is_finite(timestamp_seconds)) {
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
            *error_message = build_status_error(
                "collect_disk_counters",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        if (status == AURA_STATUS_UNAVAILABLE) {
            clear_error(error_message);
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
    clear_error(error_message);
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
    if (!is_finite(timestamp_seconds)) {
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
            *error_message = build_status_error(
                "collect_network_counters",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        if (status == AURA_STATUS_UNAVAILABLE) {
            clear_error(error_message);
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
    clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectThermalSnapshot(
    double timestamp_seconds,
    ThermalSnapshot* out_snapshot,
    std::string* error_message
) const noexcept {
    if (out_snapshot == nullptr || !is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectThermalSnapshot requires finite timestamp and output.";
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->readings.clear();
    out_snapshot->hottest_celsius.reset();

    if (collectors_.collect_thermal_readings == nullptr) {
        clear_error(error_message);
        return true;
    }

    std::array<aura_thermal_reading, kMaxThermalReadings> raw_readings{};
    uint32_t out_count = 0;
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_thermal_readings(
        raw_readings.data(),
        static_cast<uint32_t>(raw_readings.size()),
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        clear_error(error_message);
        return true;
    }

    const uint32_t bounded_count =
        std::min(out_count, static_cast<uint32_t>(raw_readings.size()));
    out_snapshot->readings.reserve(bounded_count);
    for (uint32_t i = 0; i < bounded_count; ++i) {
        const aura_thermal_reading& raw = raw_readings[static_cast<size_t>(i)];
        if (!is_finite(raw.current_celsius)) {
            continue;
        }
        if (raw.current_celsius < kCelsiusMin || raw.current_celsius > kCelsiusMax) {
            continue;
        }

        ThermalReading reading{};
        reading.label = decode_fixed_utf8(raw.label, sizeof(raw.label));
        if (reading.label.empty()) {
            reading.label = "sensor-" + std::to_string(i);
        }
        reading.current_celsius = raw.current_celsius;

        if (raw.has_high != 0 && is_finite(raw.high_celsius) && raw.high_celsius >= kCelsiusMin &&
            raw.high_celsius <= kCelsiusOptionalMax) {
            reading.high_celsius = raw.high_celsius;
        }
        if (raw.has_critical != 0 && is_finite(raw.critical_celsius) &&
            raw.critical_celsius >= kCelsiusMin &&
            raw.critical_celsius <= kCelsiusOptionalMax) {
            reading.critical_celsius = raw.critical_celsius;
        }

        out_snapshot->readings.push_back(std::move(reading));
    }

    if (!out_snapshot->readings.empty()) {
        double hottest = -std::numeric_limits<double>::infinity();
        for (const ThermalReading& reading : out_snapshot->readings) {
            hottest = std::max(hottest, reading.current_celsius);
        }
        if (is_finite(hottest)) {
            out_snapshot->hottest_celsius = hottest;
        }
    }

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectPerCoreCpu(
    double timestamp_seconds,
    PerCoreCpuSnapshot* out_snapshot,
    std::string* error_message
) const {
    if (out_snapshot == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectPerCoreCpu requires out_snapshot.";
        }
        return false;
    }
    if (!is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectPerCoreCpu requires finite timestamp.";
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->core_percents.clear();

    if (collectors_.collect_per_core_cpu == nullptr) {
        clear_error(error_message);
        return true;
    }

    std::array<double, kMaxCores> raw_percents{};
    uint32_t out_count = 0;
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_per_core_cpu(
        raw_percents.data(),
        static_cast<uint32_t>(raw_percents.size()),
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        clear_error(error_message);
        return true;
    }

    const uint32_t bounded_count = std::min(out_count, static_cast<uint32_t>(raw_percents.size()));
    out_snapshot->core_percents.reserve(bounded_count);
    for (uint32_t i = 0; i < bounded_count; ++i) {
        out_snapshot->core_percents.push_back(clamp_percent(raw_percents[i]));
    }

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectGpuSnapshot(
    double timestamp_seconds,
    GpuSnapshot* out_snapshot,
    std::string* error_message
) const noexcept {
    if (out_snapshot == nullptr || !is_finite(timestamp_seconds)) {
        if (error_message != nullptr) {
            *error_message = "CollectGpuSnapshot requires finite timestamp and output.";
        }
        return false;
    }

    out_snapshot->timestamp_seconds = timestamp_seconds;
    out_snapshot->available = false;
    out_snapshot->gpu_percent = 0.0;
    out_snapshot->vram_percent = 0.0;
    out_snapshot->vram_used_bytes = 0;
    out_snapshot->vram_total_bytes = 0;

    if (collectors_.collect_gpu_utilization == nullptr) {
        clear_error(error_message);
        return true;
    }

    aura_gpu_utilization raw{};
    std::array<char, 512> error_buffer{};
    const int status = collectors_.collect_gpu_utilization(
        &raw,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        clear_error(error_message);
        return true;
    }

    out_snapshot->available = true;
    out_snapshot->gpu_percent = clamp_percent(raw.gpu_percent);
    out_snapshot->vram_percent = clamp_percent(raw.vram_percent);
    out_snapshot->vram_used_bytes = raw.vram_used_bytes;
    out_snapshot->vram_total_bytes = raw.vram_total_bytes;
    clear_error(error_message);
    return true;
}

bool TelemetryEngine::CollectProcessDetails(
    const ProcessQueryOptions& options,
    std::vector<ProcessDetail>* out_details,
    std::string* error_message
) const noexcept {
    if (out_details == nullptr) {
        if (error_message != nullptr) {
            *error_message = "CollectProcessDetails requires out_details.";
        }
        return false;
    }
    if (collectors_.collect_process_details == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process details collector is not configured.";
        }
        return false;
    }

    // Build native query options
    aura_process_query_options native_options{};
    native_options.max_results = options.max_results;
    native_options.include_tree = options.include_tree ? 1 : 0;
    native_options.include_command_line = options.include_command_line ? 1 : 0;
    native_options.sort_column = options.sort_column;
    native_options.sort_descending = options.sort_descending ? 1 : 0;
    const size_t filter_len = std::min(options.name_filter.size(), sizeof(native_options.name_filter) - 1);
    std::memcpy(native_options.name_filter, options.name_filter.data(), filter_len);
    native_options.name_filter[filter_len] = '\0';

    const uint32_t max_samples = static_cast<uint32_t>(std::min(static_cast<size_t>(options.max_results), static_cast<size_t>(256)));
    std::vector<aura_process_detail> raw_samples(max_samples);
    uint32_t out_count = 0;
    std::array<char, 512> error_buffer{};

    const int status = collectors_.collect_process_details(
        &native_options,
        raw_samples.data(),
        max_samples,
        &out_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = build_status_error(
                "collect_process_details",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    out_details->clear();
    out_details->reserve(out_count);

    for (uint32_t i = 0; i < out_count; ++i) {
        const aura_process_detail& raw = raw_samples[i];
        ProcessDetail detail{};
        detail.pid = raw.pid;
        detail.parent_pid = raw.parent_pid;
        detail.name = decode_fixed_utf8(raw.name, sizeof(raw.name));
        if (detail.name.empty()) {
            detail.name = "pid-" + std::to_string(raw.pid);
        }
        detail.command_line = decode_fixed_utf8(raw.command_line, sizeof(raw.command_line));
        detail.cpu_percent = clamp_percent(raw.cpu_percent);
        detail.memory_rss_bytes = raw.memory_rss_bytes;
        detail.memory_private_bytes = raw.memory_private_bytes;
        detail.memory_peak_bytes = raw.memory_peak_bytes;
        detail.thread_count = raw.thread_count;
        detail.handle_count = raw.handle_count;
        detail.priority_class = raw.priority_class;
        detail.start_time_100ns = raw.start_time_100ns;
        out_details->push_back(std::move(detail));
    }

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::BuildProcessTree(
    const std::vector<ProcessDetail>& process_details,
    std::vector<ProcessTreeNode>* out_tree_nodes,
    std::string* error_message
) const noexcept {
    if (out_tree_nodes == nullptr) {
        if (error_message != nullptr) {
            *error_message = "BuildProcessTree requires out_tree_nodes.";
        }
        return false;
    }
    if (collectors_.build_process_tree == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process tree builder is not configured.";
        }
        return false;
    }

    // Convert C++ process details to native format
    std::vector<aura_process_detail> native_details;
    native_details.reserve(process_details.size());

    for (const auto& detail : process_details) {
        aura_process_detail native{};
        native.pid = detail.pid;
        native.parent_pid = detail.parent_pid;
        native.cpu_percent = detail.cpu_percent;
        native.memory_rss_bytes = detail.memory_rss_bytes;
        native.memory_private_bytes = detail.memory_private_bytes;
        native.memory_peak_bytes = detail.memory_peak_bytes;
        native.thread_count = detail.thread_count;
        native.handle_count = detail.handle_count;
        native.priority_class = detail.priority_class;
        native.start_time_100ns = detail.start_time_100ns;

        // Copy name and command line
        const size_t name_len = std::min(detail.name.size(), sizeof(native.name) - 1);
        std::memcpy(native.name, detail.name.data(), name_len);
        native.name[name_len] = '\0';

        const size_t cmd_len = std::min(detail.command_line.size(), sizeof(native.command_line) - 1);
        std::memcpy(native.command_line, detail.command_line.data(), cmd_len);
        native.command_line[cmd_len] = '\0';

        native_details.push_back(native);
    }

    std::vector<aura_process_tree_node> tree_nodes(process_details.size());
    uint32_t node_count = 0;
    std::array<char, 512> error_buffer{};

    const int status = collectors_.build_process_tree(
        native_details.data(),
        static_cast<uint32_t>(native_details.size()),
        tree_nodes.data(),
        static_cast<uint32_t>(tree_nodes.size()),
        &node_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = build_status_error(
                "build_process_tree",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    // Convert native tree nodes to C++ format
    out_tree_nodes->clear();
    out_tree_nodes->reserve(node_count);

    for (uint32_t i = 0; i < node_count; ++i) {
        const aura_process_tree_node& raw = tree_nodes[i];
        ProcessTreeNode node{};
        node.pid = raw.pid;
        node.depth = raw.depth;
        node.child_count = raw.child_count;
        node.has_children = raw.has_children != 0;
        out_tree_nodes->push_back(std::move(node));
    }

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::GetProcessByPid(
    uint32_t pid,
    ProcessDetail* out_detail,
    std::string* error_message
) const noexcept {
    if (out_detail == nullptr) {
        if (error_message != nullptr) {
            *error_message = "GetProcessByPid requires out_detail.";
        }
        return false;
    }
    if (pid == 0) {
        if (error_message != nullptr) {
            *error_message = "GetProcessByPid requires pid > 0.";
        }
        return false;
    }
    if (collectors_.get_process_by_pid == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process by PID collector is not configured.";
        }
        return false;
    }

    aura_process_detail raw{};
    std::array<char, 512> error_buffer{};
    const int status = collectors_.get_process_by_pid(
        pid,
        &raw,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = build_status_error(
                "get_process_by_pid",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    out_detail->pid = raw.pid;
    out_detail->parent_pid = raw.parent_pid;
    out_detail->name = decode_fixed_utf8(raw.name, sizeof(raw.name));
    if (out_detail->name.empty()) {
        out_detail->name = "pid-" + std::to_string(pid);
    }
    out_detail->command_line = decode_fixed_utf8(raw.command_line, sizeof(raw.command_line));
    out_detail->cpu_percent = clamp_percent(raw.cpu_percent);
    out_detail->memory_rss_bytes = raw.memory_rss_bytes;
    out_detail->memory_private_bytes = raw.memory_private_bytes;
    out_detail->memory_peak_bytes = raw.memory_peak_bytes;
    out_detail->thread_count = raw.thread_count;
    out_detail->handle_count = raw.handle_count;
    out_detail->priority_class = raw.priority_class;
    out_detail->start_time_100ns = raw.start_time_100ns;

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::TerminateProcess(
    uint32_t pid,
    uint32_t exit_code,
    std::string* error_message
) const noexcept {
    if (pid == 0) {
        if (error_message != nullptr) {
            *error_message = "TerminateProcess requires pid > 0.";
        }
        return false;
    }
    if (collectors_.terminate_process == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process termination function is not configured.";
        }
        return false;
    }

    std::array<char, 512> error_buffer{};
    const int status = collectors_.terminate_process(
        pid,
        exit_code,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = build_status_error(
                "terminate_process",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::SetProcessPriority(
    uint32_t pid,
    uint32_t priority_class,
    std::string* error_message
) const noexcept {
    if (pid == 0) {
        if (error_message != nullptr) {
            *error_message = "SetProcessPriority requires pid > 0.";
        }
        return false;
    }
    if (collectors_.set_process_priority == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process priority function is not configured.";
        }
        return false;
    }

    std::array<char, 512> error_buffer{};
    const int status = collectors_.set_process_priority(
        pid,
        priority_class,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = build_status_error(
                "set_process_priority",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    clear_error(error_message);
    return true;
}

bool TelemetryEngine::GetProcessChildren(
    uint32_t pid,
    std::vector<uint32_t>* out_child_pids,
    std::string* error_message
) const noexcept {
    if (out_child_pids == nullptr) {
        if (error_message != nullptr) {
            *error_message = "GetProcessChildren requires out_child_pids.";
        }
        return false;
    }
    if (collectors_.get_process_children == nullptr) {
        if (error_message != nullptr) {
            *error_message = "Process children collector is not configured.";
        }
        return false;
    }

    const uint32_t max_children = 256;
    std::vector<uint32_t> child_pids(max_children);
    uint32_t child_count = 0;
    std::array<char, 512> error_buffer{};

    const int status = collectors_.get_process_children(
        pid,
        child_pids.data(),
        max_children,
        &child_count,
        error_buffer.data(),
        error_buffer.size()
    );

    if (status != AURA_STATUS_OK) {
        if (error_message != nullptr) {
            *error_message = build_status_error(
                "get_process_children",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    out_child_pids->clear();
    out_child_pids->resize(child_count);
    for (uint32_t i = 0; i < child_count; ++i) {
        (*out_child_pids)[i] = child_pids[i];
    }

    clear_error(error_message);
    return true;
}

}  // namespace aura::telemetry
