#pragma once

#include <optional>
#include <string>

namespace aura::render_native {

std::string format_initial_status(
    const std::optional<std::string>& db_path,
    const std::optional<int>& sample_count,
    const std::optional<std::string>& error
);

std::string format_stream_status(
    const std::optional<std::string>& db_path,
    const std::optional<int>& sample_count,
    const std::optional<std::string>& error
);

}  // namespace aura::render_native
