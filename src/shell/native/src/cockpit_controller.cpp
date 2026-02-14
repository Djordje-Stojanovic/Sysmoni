#include "aura_shell/cockpit_controller.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aura::shell {

namespace {

std::string trim_to_max_chars(const std::string& value, const std::size_t max_chars) {
    if (value.size() <= max_chars) {
        return value;
    }
    if (max_chars <= 3U) {
        return value.substr(0, max_chars);
    }
    return value.substr(0, max_chars - 3U) + "...";
}

std::string optional_or(const std::optional<std::string>& value, const std::string& fallback) {
    if (value.has_value() && !value->empty()) {
        return *value;
    }
    return fallback;
}

}  // namespace

CockpitController::CockpitController(
    std::unique_ptr<ITelemetryBridge> telemetry_bridge,
    std::unique_ptr<IRenderBridge> render_bridge,
    Config config
)
    : telemetry_bridge_(std::move(telemetry_bridge)),
      render_bridge_(std::move(render_bridge)),
      config_(std::move(config)) {
    if (!std::isfinite(config_.poll_interval_seconds) || config_.poll_interval_seconds <= 0.0) {
        config_.poll_interval_seconds = 1.0;
    }
    if (config_.max_process_rows == 0U) {
        config_.max_process_rows = 5U;
    }
}

CockpitUiState CockpitController::tick(
    const double elapsed_since_last_frame,
    const std::optional<double> timestamp_override
) {
    CockpitUiState state;
    state.timestamp = timestamp_override.value_or(now_seconds());

    std::string telemetry_error;
    std::optional<TelemetrySnapshot> snapshot;
    if (telemetry_bridge_ != nullptr) {
        snapshot = telemetry_bridge_->collect_snapshot(telemetry_error);
    } else {
        telemetry_error = "Telemetry bridge is not configured.";
    }

    if (!snapshot.has_value()) {
        return degraded_from_last_state(
            state.timestamp,
            telemetry_error.empty() ? "Telemetry collection unavailable." : telemetry_error
        );
    }

    state.telemetry_available = true;
    state.cpu_percent = clamp_percent(snapshot->cpu_percent);
    state.memory_percent = clamp_percent(snapshot->memory_percent);

    std::vector<ProcessSample> processes;
    std::string process_error;
    if (telemetry_bridge_ != nullptr) {
        processes = telemetry_bridge_->collect_top_processes(config_.max_process_rows, process_error);
    }
    if (processes.size() > config_.max_process_rows) {
        processes.resize(config_.max_process_rows);
    }

    std::optional<std::string> stream_error;
    if (!process_error.empty()) {
        stream_error = process_error;
        state.degraded = true;
    }

    const bool render_backend_available = render_bridge_ != nullptr && render_bridge_->available();
    state.render_available = render_backend_available;

    if (render_backend_available) {
        state.cpu_percent = clamp_percent(render_bridge_->sanitize_percent(state.cpu_percent));
        state.memory_percent = clamp_percent(render_bridge_->sanitize_percent(state.memory_percent));
    }

    std::string render_error;
    if (render_backend_available) {
        const auto frame = render_bridge_->compose_frame(
            frame_phase_,
            elapsed_since_last_frame,
            state.cpu_percent,
            state.memory_percent,
            render_error
        );
        if (frame.has_value()) {
            frame_phase_ = frame->phase;
            state.accent_intensity = std::clamp(frame->accent_intensity, 0.0, 1.0);
        } else {
            state.render_available = false;
            state.degraded = true;
            stream_error = optional_or(stream_error, render_error);
        }
    }

    if (!state.render_available) {
        frame_phase_ = std::fmod(frame_phase_ + std::max(0.0, elapsed_since_last_frame), 1.0);
        state.accent_intensity = std::clamp(
            0.20 + ((state.cpu_percent + state.memory_percent) / 250.0),
            0.0,
            1.0
        );
    }

    SnapshotLines lines = fallback_snapshot_lines(state.timestamp, state.cpu_percent, state.memory_percent);
    if (state.render_available) {
        const auto formatted = render_bridge_->format_snapshot_lines(
            state.timestamp,
            state.cpu_percent,
            state.memory_percent,
            render_error
        );
        if (formatted.has_value()) {
            lines = *formatted;
        } else {
            state.render_available = false;
            state.degraded = true;
            stream_error = optional_or(stream_error, render_error);
        }
    }
    state.cpu_line = lines.cpu;
    state.memory_line = lines.memory;
    state.timestamp_line = lines.timestamp;

    if (processes.empty()) {
        state.process_rows.push_back("<no process samples>");
    } else {
        state.process_rows.reserve(processes.size());
        for (std::size_t i = 0; i < processes.size(); ++i) {
            const ProcessSample& process = processes[i];
            std::string row = fallback_process_row(static_cast<int>(i + 1), process, 42U);
            if (state.render_available) {
                const auto formatted = render_bridge_->format_process_row(
                    static_cast<int>(i + 1),
                    process.name,
                    process.cpu_percent,
                    static_cast<double>(process.memory_rss_bytes),
                    42,
                    render_error
                );
                if (formatted.has_value()) {
                    row = *formatted;
                } else {
                    state.render_available = false;
                    state.degraded = true;
                    stream_error = optional_or(stream_error, render_error);
                }
            }
            state.process_rows.push_back(std::move(row));
        }
    }

    state.status_line = fallback_status_line(stream_error);
    if (state.render_available) {
        std::string status_error;
        const auto status = render_bridge_->format_stream_status(
            config_.db_path,
            std::nullopt,
            stream_error,
            status_error
        );
        if (status.has_value()) {
            state.status_line = *status;
        } else {
            state.degraded = true;
            if (!status_error.empty()) {
                stream_error = optional_or(stream_error, status_error);
                state.status_line = fallback_status_line(stream_error);
            }
        }
    }

    if (!state.render_available) {
        state.degraded = true;
        state.status_line = fallback_status_line(stream_error);
    }

    has_last_good_state_ = true;
    last_state_ = state;
    return state;
}

const CockpitUiState& CockpitController::last_state() const {
    return last_state_;
}

double CockpitController::now_seconds() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

double CockpitController::clamp_percent(const double value) {
    if (!std::isfinite(value)) {
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

SnapshotLines CockpitController::fallback_snapshot_lines(
    const double timestamp,
    const double cpu_percent,
    const double memory_percent
) {
    std::ostringstream cpu;
    cpu << "CPU " << std::fixed << std::setprecision(1) << clamp_percent(cpu_percent) << "%";

    std::ostringstream memory;
    memory << "Memory " << std::fixed << std::setprecision(1) << clamp_percent(memory_percent) << "%";

    std::ostringstream ts;
    ts << "Timestamp " << std::fixed << std::setprecision(3) << timestamp;

    SnapshotLines lines;
    lines.cpu = cpu.str();
    lines.memory = memory.str();
    lines.timestamp = ts.str();
    return lines;
}

std::string CockpitController::fallback_process_row(
    const int rank,
    const ProcessSample& process,
    const std::size_t max_chars
) {
    std::ostringstream out;
    out << "#" << rank << " ";
    out << trim_to_max_chars(process.name.empty() ? ("pid-" + std::to_string(process.pid)) : process.name, max_chars);
    out << " cpu " << std::fixed << std::setprecision(1) << clamp_percent(process.cpu_percent) << "%";
    return out.str();
}

std::string CockpitController::fallback_status_line(const std::optional<std::string>& error) const {
    std::ostringstream status;
    status << "db=" << optional_or(config_.db_path, "<none>");
    status << " telemetry=ok";
    if (render_bridge_ != nullptr && render_bridge_->available()) {
        status << " render=ok";
    } else {
        status << " render=fallback";
    }
    if (error.has_value() && !error->empty()) {
        status << " warning=" << trim_to_max_chars(*error, 96U);
    }
    return status.str();
}

CockpitUiState CockpitController::degraded_from_last_state(
    const double timestamp,
    const std::string& reason
) const {
    if (has_last_good_state_) {
        CockpitUiState state = last_state_;
        state.timestamp = timestamp;
        state.telemetry_available = false;
        state.degraded = true;
        state.status_line = "Telemetry degraded: " + reason;
        return state;
    }

    CockpitUiState state;
    state.timestamp = timestamp;
    state.telemetry_available = false;
    state.render_available = render_bridge_ != nullptr && render_bridge_->available();
    state.degraded = true;

    const SnapshotLines lines = fallback_snapshot_lines(timestamp, 0.0, 0.0);
    state.cpu_line = lines.cpu;
    state.memory_line = lines.memory;
    state.timestamp_line = lines.timestamp;
    state.process_rows.push_back("<telemetry unavailable>");
    state.status_line = "Telemetry degraded: " + reason;
    return state;
}

}  // namespace aura::shell

