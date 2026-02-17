#include "aura_shell/cockpit_controller.hpp"

#include "cockpit_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace aura::shell {

using namespace detail;

CockpitController::CockpitController(
    std::unique_ptr<ITelemetryBridge> telemetry_bridge,
    std::unique_ptr<IRenderBridge> render_bridge,
    std::unique_ptr<ITimelineBridge> timeline_bridge,
    Config config,
    std::unique_ptr<IPersistenceBridge> persistence_bridge,
    std::unique_ptr<IAnalyticsBridge> analytics_bridge
)
    : telemetry_bridge_(std::move(telemetry_bridge)),
      render_bridge_(std::move(render_bridge)),
      timeline_bridge_(std::move(timeline_bridge)),
      persistence_bridge_(std::move(persistence_bridge)),
      analytics_bridge_(std::move(analytics_bridge)),
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
    if (config_.snapshot_buffer_capacity == 0U) {
        config_.snapshot_buffer_capacity = 60U;
    }
    if (config_.analytics_refresh_ticks == 0U) {
        config_.analytics_refresh_ticks = 3U;
    }
    // Fire analytics on the very first tick
    ticks_since_analytics_ = config_.analytics_refresh_ticks - 1U;

    if (persistence_bridge_ != nullptr && config_.persistence_enabled && config_.db_path.has_value()) {
        std::string persist_error;
        persistence_active_ = persistence_bridge_->open_store(
            *config_.db_path, config_.retention_seconds, persist_error);
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

    state.severity_level = clamp_severity_level(state.style_tokens.severity_level);
    state.motion_scale = std::clamp(state.style_tokens.motion_scale, 0.60, 1.00);
    state.quality_hint = clamp_quality_hint(state.style_tokens.quality_hint);
    state.fps_recommended_delay_ms =
        recommended_delay_ms(state.style_tokens.next_delay_seconds, state.quality_hint);
    state.fps_target = fps_from_delay_ms(state.fps_recommended_delay_ms);

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

    // ── Extended sensor collection with tiered polling ──────────────────
    ++tick_count_;

    // Per-core CPU — every tick (lightweight)
    if (telemetry_bridge_ != nullptr) {
        std::string per_core_error;
        auto per_core = telemetry_bridge_->collect_per_core_cpu(per_core_error);
        if (per_core.has_value()) {
            state.per_core_cpu = std::move(*per_core);
        }
    }

    // Disk I/O — every tick (lightweight, cumulative counter delta)
    if (telemetry_bridge_ != nullptr) {
        std::string disk_error;
        auto disk = telemetry_bridge_->collect_disk_io(disk_error);
        if (disk.has_value()) {
            state.disk_io = *disk;
        }
    }

    // Network I/O — every tick (lightweight, NIC counter delta)
    if (telemetry_bridge_ != nullptr) {
        std::string net_error;
        auto net = telemetry_bridge_->collect_network_io(net_error);
        if (net.has_value()) {
            state.network_io = *net;
        }
    }

    // Persist snapshot to DVR store (after disk I/O is available)
    if (persistence_active_ && persistence_bridge_) {
        std::string persist_err;
        if (!persistence_bridge_->append_snapshot(
                state.timestamp, state.cpu_percent, state.memory_percent,
                state.disk_io.read_bytes_per_sec, state.disk_io.write_bytes_per_sec, persist_err)) {
            persistence_active_ = false;
        }
    }

    // GPU — every 2 ticks (moderate, NVML/D3DKMT)
    if (telemetry_bridge_ != nullptr && (tick_count_ % 2 == 0 || tick_count_ == 1)) {
        std::string gpu_error;
        auto gpu = telemetry_bridge_->collect_gpu(gpu_error);
        if (gpu.has_value()) {
            cached_gpu_ = *gpu;
        }
    }
    state.gpu = cached_gpu_;

    // Thermal — every 5 ticks (moderate, WMI query)
    if (telemetry_bridge_ != nullptr && (tick_count_ % 5 == 0 || tick_count_ == 1)) {
        std::string thermal_error;
        auto thermal = telemetry_bridge_->collect_thermal(thermal_error);
        if (thermal.has_value()) {
            cached_thermal_ = *thermal;
        }
    }
    state.thermal = cached_thermal_;

    // ── Analytics processing ───────────────────────────────────────────
    if (analytics_bridge_ != nullptr && analytics_bridge_->available()) {
        // Build analytics snapshot from current telemetry
        AnalyticsSnapshot a_snap;
        a_snap.timestamp = state.timestamp;
        a_snap.cpu_percent = state.cpu_percent;
        a_snap.memory_percent = state.memory_percent;
        a_snap.disk_read_bps = state.disk_io.read_bytes_per_sec;
        a_snap.disk_write_bps = state.disk_io.write_bytes_per_sec;
        a_snap.net_recv_bps = state.network_io.recv_bytes_per_sec;
        a_snap.net_sent_bps = state.network_io.sent_bytes_per_sec;

        // Append to ring buffer (every tick)
        snapshot_buffer_.push_back(a_snap);
        if (snapshot_buffer_.size() > config_.snapshot_buffer_capacity) {
            snapshot_buffer_.erase(snapshot_buffer_.begin());
        }

        // Smoothing (every tick, if enabled)
        if (smoothing_enabled_ && analytics_bridge_->smoother_available()) {
            std::string smooth_err;
            auto smoothed = analytics_bridge_->smooth(a_snap, smooth_err);
            if (smoothed.has_value()) {
                state.cpu_percent = clamp_percent(smoothed->cpu_percent);
                state.memory_percent = clamp_percent(smoothed->memory_percent);
                state.smoothing_active = true;
            }
        }

        // Alert evaluation (every tick)
        if (analytics_bridge_->alerts_available()) {
            std::string alert_err;
            if (analytics_bridge_->evaluate_alerts(a_snap, alert_err)) {
                state.active_alerts = analytics_bridge_->get_active_alerts(alert_err);
            }
        }

        // Tiered analytics: health score + trends (every N ticks)
        ++ticks_since_analytics_;
        if (ticks_since_analytics_ >= config_.analytics_refresh_ticks) {
            ticks_since_analytics_ = 0;

            // Health score
            if (analytics_bridge_->health_available()) {
                std::string health_err;
                auto health = analytics_bridge_->compute_health_score(a_snap, health_err);
                if (health.has_value()) {
                    state.health = *health;
                }
            }

            // Trend detection (needs minimum 10 samples)
            if (analytics_bridge_->trend_available() && snapshot_buffer_.size() >= 10U) {
                std::string trend_err;

                auto cpu_trend = analytics_bridge_->detect_trend(
                    snapshot_buffer_, 0, config_.trend_sensitivity, trend_err);
                if (cpu_trend.has_value()) {
                    state.cpu_trend = cpu_trend->direction;
                }

                auto mem_trend = analytics_bridge_->detect_trend(
                    snapshot_buffer_, 1, config_.trend_sensitivity, trend_err);
                if (mem_trend.has_value()) {
                    state.memory_trend = mem_trend->direction;
                }
            }
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

void CockpitController::set_smoothing_enabled(const bool enabled) {
    smoothing_enabled_ = enabled;
    if (!enabled && analytics_bridge_ != nullptr) {
        std::string err;
        analytics_bridge_->reset_smoother(err);
    }
}

bool CockpitController::smoothing_enabled() const {
    return smoothing_enabled_;
}

bool CockpitController::acknowledge_alert(const int rule_id, std::string& error) {
    if (analytics_bridge_ == nullptr) {
        error = "Analytics bridge not available.";
        return false;
    }
    return analytics_bridge_->acknowledge_alert(rule_id, error);
}

std::optional<DvrStatsResult> CockpitController::compute_dvr_stats(std::string& error) {
    if (analytics_bridge_ == nullptr || !analytics_bridge_->dvr_stats_available()) {
        error = "DVR stats not available.";
        return std::nullopt;
    }
    if (persistence_bridge_ == nullptr) {
        error = "Persistence bridge not available.";
        return std::nullopt;
    }
    void* handle = persistence_bridge_->store_handle();
    if (handle == nullptr) {
        error = "Store not open.";
        return std::nullopt;
    }
    return analytics_bridge_->compute_dvr_stats(handle, 0.0, 0.0, error);
}

bool CockpitController::export_dvr_json(const std::string& path, std::string& error) {
    if (analytics_bridge_ == nullptr) {
        error = "Analytics bridge not available.";
        return false;
    }
    if (persistence_bridge_ == nullptr) {
        error = "Persistence bridge not available.";
        return false;
    }
    void* handle = persistence_bridge_->store_handle();
    if (handle == nullptr) {
        error = "Store not open.";
        return false;
    }
    return analytics_bridge_->export_json(handle, 0.0, 0.0, true, path, error);
}

bool CockpitController::export_dvr_csv(const std::string& path, std::string& error) {
    if (analytics_bridge_ == nullptr) {
        error = "Analytics bridge not available.";
        return false;
    }
    if (persistence_bridge_ == nullptr) {
        error = "Persistence bridge not available.";
        return false;
    }
    void* handle = persistence_bridge_->store_handle();
    if (handle == nullptr) {
        error = "Store not open.";
        return false;
    }
    return analytics_bridge_->export_csv(handle, 0.0, 0.0, path, error);
}

}  // namespace aura::shell
