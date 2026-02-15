#pragma once

#include <QString>
#include <QCoreApplication>
#include <optional>
#include <cmath>
#include <algorithm>
#include <cstddef>

#include "aura_shell/dock_model.hpp"
#include "aura_shell/cockpit_types.hpp"

namespace aura::shell {

struct LaunchConfig {
    double interval_seconds{1.0};
    bool persistence_enabled{true};
    std::optional<QString> db_path;
    std::optional<double> retention_seconds;
};

constexpr std::size_t slot_index(const DockSlot slot) {
    return static_cast<std::size_t>(slot);
}

constexpr std::size_t panel_index(const PanelId panel_id) {
    return static_cast<std::size_t>(panel_id);
}

QString slot_title(DockSlot slot);
QString slot_short_label(DockSlot slot);
QString panel_title(PanelId panel_id);
QString timeline_source_label(TimelineSource source);
QString style_mode_label(bool style_tokens_available);
QString severity_label(int severity_level);
QString quality_label(int quality_hint);
QString format_rate(double bytes_per_sec);
int interval_to_milliseconds(double interval_seconds);
int clamp_timer_interval_ms(int value);
LaunchConfig parse_args(QCoreApplication& app);

}  // namespace aura::shell
