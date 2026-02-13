#include "render_native/status.hpp"

namespace aura::render_native {

namespace {

int normalized_sample_count(const std::optional<int>& sample_count) {
    return sample_count.value_or(0);
}

}  // namespace

std::string format_initial_status(
    const std::optional<std::string>& db_path,
    const std::optional<int>& sample_count,
    const std::optional<std::string>& error
) {
    if (!db_path.has_value()) {
        return "Collecting telemetry...";
    }
    if (error.has_value()) {
        return "Collecting telemetry... | DVR unavailable: " + error.value();
    }
    return "Collecting telemetry... | DVR samples: " +
           std::to_string(normalized_sample_count(sample_count));
}

std::string format_stream_status(
    const std::optional<std::string>& db_path,
    const std::optional<int>& sample_count,
    const std::optional<std::string>& error
) {
    if (!db_path.has_value()) {
        return "Streaming telemetry";
    }
    if (error.has_value()) {
        return "Streaming telemetry | DVR unavailable: " + error.value();
    }
    return "Streaming telemetry | DVR samples: " +
           std::to_string(normalized_sample_count(sample_count));
}

}  // namespace aura::render_native
