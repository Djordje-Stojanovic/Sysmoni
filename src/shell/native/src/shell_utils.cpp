#include "aura_shell/shell_utils.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>
#include <algorithm>
#include <cmath>

namespace aura::shell {

QString slot_title(const DockSlot slot) {
    switch (slot) {
        case DockSlot::Left:
            return QStringLiteral("LEFT");
        case DockSlot::Center:
            return QStringLiteral("CENTER");
        case DockSlot::Right:
            return QStringLiteral("RIGHT");
    }
    return QStringLiteral("UNKNOWN");
}

QString slot_short_label(const DockSlot slot) {
    switch (slot) {
        case DockSlot::Left:
            return QStringLiteral("L");
        case DockSlot::Center:
            return QStringLiteral("C");
        case DockSlot::Right:
            return QStringLiteral("R");
    }
    return QStringLiteral("?");
}

QString panel_title(const PanelId panel_id) {
    switch (panel_id) {
        case PanelId::TelemetryOverview:
            return QStringLiteral("Telemetry");
        case PanelId::TopProcesses:
            return QStringLiteral("Processes");
        case PanelId::DvrTimeline:
            return QStringLiteral("Timeline");
        case PanelId::RenderSurface:
            return QStringLiteral("Render");
        case PanelId::ProcessPanel:
            return QStringLiteral("Manage");
    }
    return QStringLiteral("Unknown");
}

QString timeline_source_label(const TimelineSource source) {
    switch (source) {
        case TimelineSource::None:
            return QStringLiteral("none");
        case TimelineSource::Live:
            return QStringLiteral("live");
        case TimelineSource::Dvr:
            return QStringLiteral("dvr");
    }
    return QStringLiteral("unknown");
}

QString style_mode_label(const bool style_tokens_available) {
    return style_tokens_available ? QStringLiteral("ok") : QStringLiteral("fallback");
}

QString severity_label(const int severity_level) {
    switch (std::clamp(severity_level, 0, 3)) {
        case 0:
            return QStringLiteral("calm");
        case 1:
            return QStringLiteral("elevated");
        case 2:
            return QStringLiteral("hot");
        case 3:
            return QStringLiteral("critical");
    }
    return QStringLiteral("calm");
}

QString quality_label(const int quality_hint) {
    return quality_hint > 0 ? QStringLiteral("throttled") : QStringLiteral("full");
}

QString format_rate(double bytes_per_sec) {
    if (bytes_per_sec >= 1073741824.0) return QString::number(bytes_per_sec / 1073741824.0, 'f', 1) + " GB/s";
    if (bytes_per_sec >= 1048576.0)    return QString::number(bytes_per_sec / 1048576.0, 'f', 1) + " MB/s";
    if (bytes_per_sec >= 1024.0)       return QString::number(bytes_per_sec / 1024.0, 'f', 1) + " KB/s";
    return QString::number(static_cast<int>(bytes_per_sec)) + " B/s";
}

int interval_to_milliseconds(const double interval_seconds) {
    if (!std::isfinite(interval_seconds) || interval_seconds <= 0.0) {
        return 1000;
    }
    return std::clamp(static_cast<int>(std::llround(interval_seconds * 1000.0)), 1000, 2000);
}

int clamp_timer_interval_ms(const int value) {
    return std::clamp(value, 1000, 2000);
}

LaunchConfig parse_args(QCoreApplication& app) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Aura native shell");
    parser.addHelpOption();

    QCommandLineOption interval_option(
        "interval",
        "Polling interval in seconds.",
        "seconds",
        "1.0"
    );
    QCommandLineOption no_persist_option(
        "no-persist",
        "Disable telemetry persistence."
    );
    QCommandLineOption db_path_option(
        "db-path",
        "SQLite telemetry store path.",
        "path"
    );
    QCommandLineOption retention_option(
        "retention-seconds",
        "Retention horizon in seconds.",
        "seconds"
    );

    parser.addOption(interval_option);
    parser.addOption(no_persist_option);
    parser.addOption(db_path_option);
    parser.addOption(retention_option);
    parser.process(app);

    LaunchConfig config{};
    config.interval_seconds = parser.value(interval_option).toDouble();
    config.persistence_enabled = !parser.isSet(no_persist_option);

    if (parser.isSet(db_path_option)) {
        config.db_path = parser.value(db_path_option);
    }
    if (parser.isSet(retention_option)) {
        config.retention_seconds = parser.value(retention_option).toDouble();
    }
    return config;
}

}  // namespace aura::shell
