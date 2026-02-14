#include "aura_shell/cockpit_controller.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

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

double clamp_unit(const double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

double clamp_percent_100(const double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 100.0);
}

RenderStyleTokens fallback_style_tokens(
    const double phase,
    const double accent_intensity,
    const double cpu_percent,
    const double memory_percent
) {
    RenderStyleTokens tokens;
    if (!std::isfinite(phase)) {
        tokens.phase = 0.0;
    } else {
        tokens.phase = std::fmod(phase, 1.0);
    }
    if (tokens.phase < 0.0) {
        tokens.phase += 1.0;
    }
    tokens.next_delay_seconds = 1.0 / 60.0;
    tokens.accent_intensity = clamp_unit(accent_intensity);
    tokens.accent_red = clamp_unit(0.20 + tokens.accent_intensity * 0.50);
    tokens.accent_green = clamp_unit(0.45 + tokens.accent_intensity * 0.25);
    tokens.accent_blue = 0.75;
    tokens.accent_alpha = clamp_unit(0.15 + tokens.accent_intensity * 0.35);
    tokens.frost_intensity = clamp_unit(0.25 + tokens.accent_intensity * 0.55);
    tokens.tint_strength = clamp_unit(0.35 + tokens.accent_intensity * 0.45);
    tokens.ring_line_width = std::clamp(1.0 + (tokens.accent_intensity * 6.0), 1.0, 7.0);
    tokens.ring_glow_strength = tokens.accent_intensity;
    tokens.cpu_alpha = clamp_unit(0.30 + clamp_percent_100(cpu_percent) / 100.0 * 0.70);
    tokens.memory_alpha = clamp_unit(0.30 + clamp_percent_100(memory_percent) / 100.0 * 0.70);
    return tokens;
}

}  // namespace

CockpitController::CockpitController(
    std::unique_ptr<ITelemetryBridge> telemetry_bridge,
    std::unique_ptr<IRenderBridge> render_bridge,
    std::unique_ptr<ITimelineBridge> timeline_bridge,
    Config config
)
    : telemetry_bridge_(std::move(telemetry_bridge)),
      render_bridge_(std::move(render_bridge)),
      timeline_bridge_(std::move(timeline_bridge)),
      config_(std::move(config)) {
    if (!std::isfinite(config_.poll_interval_seconds) || config_.poll_interval_seconds <= 0.0) {
        config_.poll_interval_seconds = 1.0;
    }
    if (config_.max_process_rows == 0U) {
        config_.max_process_rows = 5U;
    }
    if (config_.timeline_live_capacity == 0U) {
        config_.timeline_live_capacity = 120U;
    }
    if (!std::isfinite(config_.timeline_window_seconds) || config_.timeline_window_seconds <= 0.0) {
        config_.timeline_window_seconds = 300.0;
    }
    if (config_.timeline_resolution < 2) {
        config_.timeline_resolution = 64;
    }
    if (config_.timeline_refresh_ticks == 0U) {
        config_.timeline_refresh_ticks = 1U;
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
    if (state.render_available) {
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

    if (state.render_available) {
        const auto style_tokens = render_bridge_->compute_style_tokens(
            frame_phase_,
            elapsed_since_last_frame,
            state.cpu_percent,
            state.memory_percent,
            render_error
        );
        if (style_tokens.has_value()) {
            state.style_tokens = *style_tokens;
            state.style_tokens_available = true;
            frame_phase_ = state.style_tokens.phase;
            state.accent_intensity = clamp_unit(state.style_tokens.accent_intensity);
        } else {
            state.degraded = true;
            state.style_token_error =
                render_error.empty() ? render_bridge_->last_error_text() : render_error;
            if (state.style_token_error.empty()) {
                state.style_token_error = "Render style token computation failed.";
            }
            stream_error = optional_or(stream_error, state.style_token_error);
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

    if (!state.style_tokens_available) {
        state.style_tokens = fallback_style_tokens(
            frame_phase_,
            state.accent_intensity,
            state.cpu_percent,
            state.memory_percent
        );
        if (state.style_token_error.empty() && !state.render_available) {
            state.style_token_error =
                render_error.empty() ? "Render backend unavailable." : render_error;
        }
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

    populate_timeline_state(state, stream_error);

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

std::string CockpitController::timeline_source_to_string(const TimelineSource source) {
    switch (source) {
        case TimelineSource::None:
            return "none";
        case TimelineSource::Live:
            return "live";
        case TimelineSource::Dvr:
            return "dvr";
    }
    return "unknown";
}

std::string CockpitController::fallback_timeline_line(
    const TimelineSource source,
    const std::size_t point_count,
    const double cpu_percent,
    const double memory_percent
) {
    std::ostringstream timeline;
    timeline << "timeline=" << timeline_source_to_string(source);
    timeline << " points=" << point_count;
    timeline << " cpu_now=" << std::fixed << std::setprecision(1) << clamp_percent(cpu_percent) << "%";
    timeline << " mem_now=" << std::fixed << std::setprecision(1) << clamp_percent(memory_percent) << "%";
    return timeline.str();
}

void CockpitController::append_live_timeline_point(
    const double timestamp,
    const double cpu_percent,
    const double memory_percent
) {
    TimelinePoint next;
    next.timestamp = timestamp;
    next.cpu_percent = clamp_percent(cpu_percent);
    next.memory_percent = clamp_percent(memory_percent);
    live_timeline_points_.push_back(next);

    if (live_timeline_points_.size() > config_.timeline_live_capacity) {
        const std::size_t overflow = live_timeline_points_.size() - config_.timeline_live_capacity;
        live_timeline_points_.erase(
            live_timeline_points_.begin(),
            live_timeline_points_.begin() + static_cast<std::ptrdiff_t>(overflow)
        );
    }

    const double cutoff = timestamp - config_.timeline_window_seconds;
    live_timeline_points_.erase(
        std::remove_if(
            live_timeline_points_.begin(),
            live_timeline_points_.end(),
            [cutoff](const TimelinePoint& point) { return point.timestamp < cutoff; }
        ),
        live_timeline_points_.end()
    );
}

std::vector<TimelinePoint> CockpitController::copy_live_timeline_window(const double now_timestamp) const {
    std::vector<TimelinePoint> output;
    const double cutoff = now_timestamp - config_.timeline_window_seconds;
    output.reserve(live_timeline_points_.size());
    for (const TimelinePoint& point : live_timeline_points_) {
        if (point.timestamp >= cutoff) {
            output.push_back(point);
        }
    }
    return output;
}

void CockpitController::populate_timeline_state(
    CockpitUiState& state,
    std::optional<std::string>& stream_error
) {
    append_live_timeline_point(state.timestamp, state.cpu_percent, state.memory_percent);
    const std::vector<TimelinePoint> live_points = copy_live_timeline_window(state.timestamp);

    const bool has_db_path = config_.db_path.has_value() && !config_.db_path->empty();
    const bool can_query_dvr = config_.prefer_dvr_timeline && has_db_path &&
                               timeline_bridge_ != nullptr && timeline_bridge_->available();

    if (can_query_dvr) {
        ++ticks_since_timeline_query_;
        if (!has_dvr_timeline_cache_ || ticks_since_timeline_query_ >= config_.timeline_refresh_ticks) {
            std::string timeline_error;
            const auto queried = timeline_bridge_->query_recent(
                *config_.db_path,
                state.timestamp,
                config_.timeline_window_seconds,
                config_.timeline_resolution,
                timeline_error
            );
            ticks_since_timeline_query_ = 0U;
            if (!timeline_error.empty()) {
                has_dvr_timeline_cache_ = false;
                dvr_timeline_cache_.clear();
                if (live_points.size() < 2U) {
                    stream_error = optional_or(stream_error, timeline_error);
                }
            } else if (queried.size() >= 8U) {
                dvr_timeline_cache_ = queried;
                has_dvr_timeline_cache_ = true;
            } else {
                has_dvr_timeline_cache_ = false;
                dvr_timeline_cache_.clear();
            }
        }
    } else {
        ticks_since_timeline_query_ = 0U;
        has_dvr_timeline_cache_ = false;
        dvr_timeline_cache_.clear();
    }

    if (has_dvr_timeline_cache_ && !dvr_timeline_cache_.empty()) {
        state.timeline_source = TimelineSource::Dvr;
        state.timeline_points = dvr_timeline_cache_;
    } else if (live_points.size() >= 2U) {
        state.timeline_source = TimelineSource::Live;
        state.timeline_points = live_points;
    } else {
        state.timeline_source = TimelineSource::None;
        state.timeline_points.clear();
    }

    state.timeline_line = fallback_timeline_line(
        state.timeline_source,
        state.timeline_points.size(),
        state.cpu_percent,
        state.memory_percent
    );
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
    state.timeline_source = TimelineSource::None;
    state.timeline_line = fallback_timeline_line(TimelineSource::None, 0U, 0.0, 0.0);
    state.status_line = "Telemetry degraded: " + reason;
    state.style_tokens = fallback_style_tokens(frame_phase_, 0.0, 0.0, 0.0);
    state.style_tokens_available = false;
    state.style_token_error = reason;
    return state;
}

}  // namespace aura::shell
