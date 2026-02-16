#include "aura_shell/cockpit_controller.hpp"

#include "cockpit_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace aura::shell {

using namespace detail;

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
    const int anomaly_count,
    const double cpu_percent,
    const double memory_percent
) {
    std::ostringstream timeline;
    timeline << "timeline=" << timeline_source_to_string(source);
    timeline << " points=" << point_count;
    timeline << " anomalies=" << std::max(0, anomaly_count);
    timeline << " cpu_now=" << std::fixed << std::setprecision(1) << clamp_percent(cpu_percent) << "%";
    timeline << " mem_now=" << std::fixed << std::setprecision(1) << clamp_percent(memory_percent) << "%";
    return timeline.str();
}

void CockpitController::append_live_timeline_point(
    const double timestamp,
    const double cpu_percent,
    const double memory_percent,
    const double gpu_percent
) {
    TimelinePoint next;
    next.timestamp = timestamp;
    next.cpu_percent = clamp_percent(cpu_percent);
    next.memory_percent = clamp_percent(memory_percent);
    next.gpu_percent = clamp_percent(gpu_percent);
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
    append_live_timeline_point(state.timestamp, state.cpu_percent, state.memory_percent, state.gpu.gpu_percent);
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

    state.timeline_anomaly_count = count_timeline_anomalies(state.timeline_points);
    state.timeline_line = fallback_timeline_line(
        state.timeline_source,
        state.timeline_points.size(),
        state.timeline_anomaly_count,
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
        state.status_line = "Telemetry degraded; using last-known-good state: " + reason;
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
    state.timeline_anomaly_count = 0;
    state.timeline_line = fallback_timeline_line(TimelineSource::None, 0U, 0, 0.0, 0.0);
    state.status_line = "Telemetry degraded; live fallback active: " + reason;
    state.style_tokens = fallback_style_tokens(frame_phase_, 0.0, 0.0, 0.0);
    state.style_tokens_available = false;
    state.style_token_error = reason;
    return state;
}

}  // namespace aura::shell
