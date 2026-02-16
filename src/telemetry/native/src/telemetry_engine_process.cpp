#include "telemetry_engine.h"
#include "telemetry_engine_internal.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace aura::telemetry {

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
            *error_message = detail::build_status_error(
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
        ProcessDetail pd{};
        pd.pid = raw.pid;
        pd.parent_pid = raw.parent_pid;
        pd.name = detail::decode_fixed_utf8(raw.name, sizeof(raw.name));
        if (pd.name.empty()) {
            pd.name = "pid-" + std::to_string(raw.pid);
        }
        pd.command_line = detail::decode_fixed_utf8(raw.command_line, sizeof(raw.command_line));
        pd.cpu_percent = detail::clamp_percent(raw.cpu_percent);
        pd.memory_rss_bytes = raw.memory_rss_bytes;
        pd.memory_private_bytes = raw.memory_private_bytes;
        pd.memory_peak_bytes = raw.memory_peak_bytes;
        pd.thread_count = raw.thread_count;
        pd.handle_count = raw.handle_count;
        pd.priority_class = raw.priority_class;
        pd.start_time_100ns = raw.start_time_100ns;
        out_details->push_back(std::move(pd));
    }

    detail::clear_error(error_message);
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

    for (const auto& pd : process_details) {
        aura_process_detail native{};
        native.pid = pd.pid;
        native.parent_pid = pd.parent_pid;
        native.cpu_percent = pd.cpu_percent;
        native.memory_rss_bytes = pd.memory_rss_bytes;
        native.memory_private_bytes = pd.memory_private_bytes;
        native.memory_peak_bytes = pd.memory_peak_bytes;
        native.thread_count = pd.thread_count;
        native.handle_count = pd.handle_count;
        native.priority_class = pd.priority_class;
        native.start_time_100ns = pd.start_time_100ns;

        // Copy name and command line
        const size_t name_len = std::min(pd.name.size(), sizeof(native.name) - 1);
        std::memcpy(native.name, pd.name.data(), name_len);
        native.name[name_len] = '\0';

        const size_t cmd_len = std::min(pd.command_line.size(), sizeof(native.command_line) - 1);
        std::memcpy(native.command_line, pd.command_line.data(), cmd_len);
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
            *error_message = detail::build_status_error(
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

    detail::clear_error(error_message);
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
            *error_message = detail::build_status_error(
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
    out_detail->name = detail::decode_fixed_utf8(raw.name, sizeof(raw.name));
    if (out_detail->name.empty()) {
        out_detail->name = "pid-" + std::to_string(pid);
    }
    out_detail->command_line = detail::decode_fixed_utf8(raw.command_line, sizeof(raw.command_line));
    out_detail->cpu_percent = detail::clamp_percent(raw.cpu_percent);
    out_detail->memory_rss_bytes = raw.memory_rss_bytes;
    out_detail->memory_private_bytes = raw.memory_private_bytes;
    out_detail->memory_peak_bytes = raw.memory_peak_bytes;
    out_detail->thread_count = raw.thread_count;
    out_detail->handle_count = raw.handle_count;
    out_detail->priority_class = raw.priority_class;
    out_detail->start_time_100ns = raw.start_time_100ns;

    detail::clear_error(error_message);
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
            *error_message = detail::build_status_error(
                "terminate_process",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    detail::clear_error(error_message);
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
            *error_message = detail::build_status_error(
                "set_process_priority",
                status,
                error_buffer.data(),
                error_buffer.size()
            );
        }
        return false;
    }

    detail::clear_error(error_message);
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
            *error_message = detail::build_status_error(
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

    detail::clear_error(error_message);
    return true;
}

}  // namespace aura::telemetry
