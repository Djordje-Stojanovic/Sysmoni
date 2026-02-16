#include "aura_shell/aura_shell_window.hpp"

#include <QQuickItem>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace aura::shell {

void AuraShellWindow::refresh_cockpit() {
    if (!controller_) {
        return;
    }
    const auto state = controller_->tick(current_interval_seconds_);

    telemetry_cpu_->setText(QString::fromStdString(state.cpu_line));
    telemetry_memory_->setText(QString::fromStdString(state.memory_line));
    telemetry_timestamp_->setText(QString::fromStdString(state.timestamp_line));
    telemetry_status_->setText(QString::fromStdString(state.status_line));

    // GPU
    if (state.gpu.available) {
        const double vram_used_gb = static_cast<double>(state.gpu.vram_used_bytes) / 1073741824.0;
        const double vram_total_gb = static_cast<double>(state.gpu.vram_total_bytes) / 1073741824.0;
        gpu_value_->setText(
            QString("%1% \u2014 %2 / %3 GB VRAM")
                .arg(static_cast<int>(state.gpu.gpu_percent))
                .arg(vram_used_gb, 0, 'f', 1)
                .arg(vram_total_gb, 0, 'f', 1)
        );
    } else {
        gpu_value_->setText(QStringLiteral("Not available"));
    }

    // Disk I/O
    disk_value_->setText(
        format_rate(state.disk_io.read_bytes_per_sec) + QString::fromUtf8(" R / ") +
        format_rate(state.disk_io.write_bytes_per_sec) + QString::fromUtf8(" W")
    );

    // Network I/O
    net_value_->setText(
        format_rate(state.network_io.recv_bytes_per_sec) + QString::fromUtf8(" \u2193 / ") +
        format_rate(state.network_io.sent_bytes_per_sec) + QString::fromUtf8(" \u2191")
    );

    // Thermal
    if (state.thermal.available) {
        thermal_value_->setText(
            QString("CPU %1\u00b0C").arg(static_cast<int>(state.thermal.hottest_celsius))
        );
    } else {
        thermal_value_->setText(QStringLiteral("Not available"));
    }
    process_status_->setText(QString::fromStdString(state.status_line));
    render_status_->setText(QString::fromStdString(state.status_line));
    timeline_status_->setText(QString::fromStdString(state.timeline_line));

    for (std::size_t i = 0; i < process_labels_.size(); ++i) {
        const QString line = i < state.process_rows.size()
                                 ? QString::fromStdString(state.process_rows[i])
                                 : QStringLiteral("-");
        process_labels_[i]->setText(line);
    }

    // Refresh process list model
    if (process_model_ != nullptr && telemetry_bridge_raw_ != nullptr) {
        // Default to 16 GB as baseline; the memory_percent bar in QML
        // uses per-row bytes so this is only for the relative bar sizing
        constexpr std::uint64_t kDefaultTotalMemory = 16ULL * 1024 * 1024 * 1024;
        const std::uint64_t total_memory_estimate = kDefaultTotalMemory;
        process_model_->refresh(telemetry_bridge_raw_, total_memory_estimate);
    }

    // Bridge theme to process QML root
    if (process_quick_ != nullptr && process_quick_->rootObject() != nullptr) {
        QQuickItem* proc_root = process_quick_->rootObject();
        proc_root->setProperty("accentRed", state.style_tokens.accent_red);
        proc_root->setProperty("accentGreen", state.style_tokens.accent_green);
        proc_root->setProperty("accentBlue", state.style_tokens.accent_blue);
        proc_root->setProperty("severityLevel", state.severity_level);
        if (theme_dirty_) {
            proc_root->setProperty(
                "themeMode",
                QString::fromStdString(ui_theme_mode_key(current_theme_mode_))
            );
        }
    }

    // Footer — compact status summary
    QString footer_text =
        QString("interval=%1s  persist=%2  db=%3  telemetry=%4  render=%5  timeline=%6  style=%7  sev=%8  quality=%9  fps=%10  anomalies=%11  theme=%12")
            .arg(current_interval_seconds_, 0, 'f', 3)
            .arg(config_.persistence_enabled ? "on" : "off")
            .arg(config_.db_path.value_or("<none>"))
            .arg(state.telemetry_available ? "ok" : "degraded")
            .arg(state.render_available ? "ok" : "fallback")
            .arg(timeline_source_label(state.timeline_source))
            .arg(style_mode_label(state.style_tokens_available))
            .arg(severity_label(state.severity_level))
            .arg(quality_label(state.quality_hint))
            .arg(state.fps_target)
            .arg(state.timeline_anomaly_count)
            .arg(QString::fromStdString(ui_theme_mode_key(current_theme_mode_)));
    if (!state.style_token_error.empty()) {
        QString error_text = QString::fromStdString(state.style_token_error);
        if (error_text.size() > 56) {
            error_text = error_text.left(53) + "...";
        }
        footer_text += QString("  style_err=%1").arg(error_text);
    }
    footer_status_->setText(footer_text);

    // QML property bridge — property names unchanged
    if (quick_ != nullptr && quick_->rootObject() != nullptr) {
        QQuickItem* root = quick_->rootObject();
        root->setProperty("accentIntensity", state.accent_intensity);
        root->setProperty("cpuPercent", state.cpu_percent);
        root->setProperty("memoryPercent", state.memory_percent);
        root->setProperty("accentRed", state.style_tokens.accent_red);
        root->setProperty("accentGreen", state.style_tokens.accent_green);
        root->setProperty("accentBlue", state.style_tokens.accent_blue);
        root->setProperty("accentAlpha", state.style_tokens.accent_alpha);
        root->setProperty("frostIntensity", state.style_tokens.frost_intensity);
        root->setProperty("tintStrength", state.style_tokens.tint_strength);
        root->setProperty("ringLineWidth", state.style_tokens.ring_line_width);
        root->setProperty("ringGlowStrength", state.style_tokens.ring_glow_strength);
        root->setProperty("cpuAlpha", state.style_tokens.cpu_alpha);
        root->setProperty("memoryAlpha", state.style_tokens.memory_alpha);
        root->setProperty("severityLevel", state.severity_level);
        root->setProperty("motionScale", state.motion_scale);
        root->setProperty("qualityHint", state.quality_hint);
        root->setProperty("timelineAnomalyAlpha", state.style_tokens.timeline_anomaly_alpha);
        root->setProperty("statusText", QString::fromStdString(state.status_line));

        // Per-core CPU
        QVariantList core_list;
        core_list.reserve(static_cast<int>(state.per_core_cpu.core_percents.size()));
        for (const double pct : state.per_core_cpu.core_percents) {
            core_list.append(pct);
        }
        root->setProperty("perCoreCpu", core_list);
        root->setProperty("coreCount", static_cast<int>(state.per_core_cpu.core_count));

        // GPU
        root->setProperty("gpuAvailable", state.gpu.available);
        root->setProperty("gpuPercent", state.gpu.gpu_percent);
        root->setProperty("vramPercent", state.gpu.vram_percent);
        root->setProperty("vramUsedBytes", static_cast<double>(state.gpu.vram_used_bytes));
        root->setProperty("vramTotalBytes", static_cast<double>(state.gpu.vram_total_bytes));

        // Disk I/O
        root->setProperty("diskReadBps", state.disk_io.read_bytes_per_sec);
        root->setProperty("diskWriteBps", state.disk_io.write_bytes_per_sec);

        // Network I/O
        root->setProperty("netRecvBps", state.network_io.recv_bytes_per_sec);
        root->setProperty("netSentBps", state.network_io.sent_bytes_per_sec);

        // Thermal
        root->setProperty("thermalAvailable", state.thermal.available);
        root->setProperty("thermalHottest", state.thermal.hottest_celsius);
        QVariantList sensor_list;
        sensor_list.reserve(static_cast<int>(state.thermal.sensors.size()));
        for (const auto& s : state.thermal.sensors) {
            QVariantMap m;
            m["label"] = QString::fromStdString(s.label);
            m["current"] = s.current_celsius;
            m["high"] = s.high_celsius;
            m["critical"] = s.critical_celsius;
            m["hasHigh"] = s.has_high;
            m["hasCritical"] = s.has_critical;
            sensor_list.append(m);
        }
        root->setProperty("thermalSensors", sensor_list);

        if (theme_dirty_) {
            root->setProperty(
                "themeMode",
                QString::fromStdString(ui_theme_mode_key(current_theme_mode_))
            );
        }
    }

    // Timeline QML property bridge
    if (timeline_quick_ != nullptr && timeline_quick_->rootObject() != nullptr) {
        QQuickItem* tl_root = timeline_quick_->rootObject();

        QVariantList points;
        points.reserve(static_cast<int>(state.timeline_points.size()));
        for (const auto& pt : state.timeline_points) {
            QVariantMap m;
            m["timestamp"] = pt.timestamp;
            m["cpuPercent"] = pt.cpu_percent;
            m["memPercent"] = pt.memory_percent;
            m["gpuPercent"] = pt.gpu_percent;
            points.append(m);
        }
        tl_root->setProperty("timelinePoints", points);

        QString source_str = QStringLiteral("none");
        if (state.timeline_source == TimelineSource::Live)
            source_str = QStringLiteral("live");
        else if (state.timeline_source == TimelineSource::Dvr)
            source_str = QStringLiteral("dvr");
        tl_root->setProperty("timelineSource", source_str);

        tl_root->setProperty("accentRed", state.style_tokens.accent_red);
        tl_root->setProperty("accentGreen", state.style_tokens.accent_green);
        tl_root->setProperty("accentBlue", state.style_tokens.accent_blue);
        tl_root->setProperty("severityLevel", state.severity_level);
        tl_root->setProperty("gpuAvailable", state.gpu.available);

        if (theme_dirty_) {
            tl_root->setProperty(
                "themeMode",
                QString::fromStdString(ui_theme_mode_key(current_theme_mode_))
            );
        }
    }

    if (theme_dirty_) {
        theme_dirty_ = false;
    }

    if (update_timer_ != nullptr) {
        current_interval_seconds_ = static_cast<double>(update_timer_->interval()) / 1000.0;
    }
}

}  // namespace aura::shell
