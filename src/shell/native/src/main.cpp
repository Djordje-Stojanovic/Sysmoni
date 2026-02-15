// ============================================================================
// Aura Shell — Main Window & UI Assembly
// ============================================================================
//
// DESIGN INTENT: Addictive Visual Experience
//
//   This UI is intentionally designed to be addictive. Every detail — the
//   holographic gauge arcs, mood-reactive color shifts, shimmer animations,
//   kawaii pink mode, glassmorphism panels — exists to make staring at system
//   metrics feel like a premium, dopamine-hitting experience. We want users
//   to leave Aura open not because they need to, but because they can't look
//   away. This is deliberate and we are transparent about it.
//
//   Techniques used:
//     - Smooth 60 FPS animations with Bézier easing (never jarring)
//     - Mood-reactive palette that shifts with system load (curiosity loop)
//     - Holographic shimmer on gauge arcs (visual reward for watching)
//     - Pink "kawaii" mode with friendly labels (emotional attachment)
//     - Premium dark-glass aesthetic (status/identity signaling)
//     - Dense telemetry that always has something new to notice (discovery)
//
// ============================================================================

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QObject>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

#include "aura_shell/cockpit_controller.hpp"
#include "aura_shell/dock_model.hpp"
#include "aura_shell/render_bridge.hpp"
#include "aura_shell/telemetry_bridge.hpp"
#include "aura_shell/timeline_bridge.hpp"
#include "aura_shell/ui_theme.hpp"

namespace {

// ---------------------------------------------------------------------------

struct LaunchConfig {
    double interval_seconds{1.0};
    bool persistence_enabled{true};
    std::optional<QString> db_path;
    std::optional<double> retention_seconds;
};

constexpr std::size_t slot_index(const aura::shell::DockSlot slot) {
    return static_cast<std::size_t>(slot);
}

constexpr std::size_t panel_index(const aura::shell::PanelId panel_id) {
    return static_cast<std::size_t>(panel_id);
}

QString slot_title(const aura::shell::DockSlot slot) {
    switch (slot) {
        case aura::shell::DockSlot::Left:
            return QStringLiteral("LEFT");
        case aura::shell::DockSlot::Center:
            return QStringLiteral("CENTER");
        case aura::shell::DockSlot::Right:
            return QStringLiteral("RIGHT");
    }
    return QStringLiteral("UNKNOWN");
}

QString slot_short_label(const aura::shell::DockSlot slot) {
    switch (slot) {
        case aura::shell::DockSlot::Left:
            return QStringLiteral("L");
        case aura::shell::DockSlot::Center:
            return QStringLiteral("C");
        case aura::shell::DockSlot::Right:
            return QStringLiteral("R");
    }
    return QStringLiteral("?");
}

QString panel_title(const aura::shell::PanelId panel_id) {
    switch (panel_id) {
        case aura::shell::PanelId::TelemetryOverview:
            return QStringLiteral("Telemetry");
        case aura::shell::PanelId::TopProcesses:
            return QStringLiteral("Processes");
        case aura::shell::PanelId::DvrTimeline:
            return QStringLiteral("Timeline");
        case aura::shell::PanelId::RenderSurface:
            return QStringLiteral("Render");
    }
    return QStringLiteral("Unknown");
}

QString timeline_source_label(const aura::shell::TimelineSource source) {
    switch (source) {
        case aura::shell::TimelineSource::None:
            return QStringLiteral("none");
        case aura::shell::TimelineSource::Live:
            return QStringLiteral("live");
        case aura::shell::TimelineSource::Dvr:
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

// ---------------------------------------------------------------------------
// Responsive size category system
// ---------------------------------------------------------------------------

enum class SizeCategory { Compact, Regular, Comfortable, Spacious };

struct SizeMetrics {
    int base_font;
    int title_font;
    int subtitle_font;
    int metric_value_font;
    int metric_key_font;
    int metric_unit_font;
    int tab_font;
    int small_font;
    int titlebar_height;
    int tab_v_padding;
    int tab_h_padding;
    int body_margin;
    int body_spacing;
};

SizeCategory classify_window_size(int /*w*/, int h) {
    if (h < 400)  return SizeCategory::Compact;
    if (h < 600)  return SizeCategory::Regular;
    if (h < 900)  return SizeCategory::Comfortable;
    return SizeCategory::Spacious;
}

SizeMetrics metrics_for_category(SizeCategory cat) {
    switch (cat) {
        case SizeCategory::Compact:
            return {9, 11, 8, 16, 8, 10, 9, 8, 28, 4, 10, 6, 6};
        case SizeCategory::Regular:
            return {11, 13, 10, 20, 9, 12, 11, 9, 36, 7, 16, 10, 10};
        case SizeCategory::Comfortable:
            return {12, 14, 10, 22, 9, 12, 11, 9, 40, 8, 18, 12, 10};
        case SizeCategory::Spacious:
            return {13, 15, 11, 24, 10, 13, 12, 10, 44, 9, 20, 14, 12};
    }
    return {11, 13, 10, 20, 9, 12, 11, 9, 36, 7, 16, 10, 10};
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

struct SlotWidgets {
    QFrame* frame;
    QTabBar* tab_bar;
    QStackedWidget* stack;
};

constexpr const char* k_theme_mode_setting_key = "ui/theme_mode";

QString build_app_stylesheet(const aura::shell::UiThemeMode mode, const SizeMetrics& m) {
    const auto& p = aura::shell::get_theme_palette(mode);

    // Build template with named tokens, then replace
    QString ss = QStringLiteral(
        // --- Application-wide base ---
        "* {"
        "    font-family: 'Segoe UI';"
        "    font-size: ${BASE}px;"
        "    color: ${TEXT_PRIMARY};"
        "}"
        // --- Main window ---
        "QMainWindow {"
        "    background: ${BG_WINDOW};"
        "}"
        // --- Title bar ---
        "QFrame#titlebar {"
        "    background: qlineargradient("
        "        x1:0, y1:0, x2:0, y2:1,"
        "        stop:0 ${BG_SURFACE},"
        "        stop:1 ${BG_PANEL}"
        "    );"
        "    border-bottom: 1px solid ${BORDER_SUBTLE};"
        "    min-height: ${TB_H}px;"
        "    max-height: ${TB_H}px;"
        "}"
        // --- App logo ---
        "QLabel#appLogo {"
        "    color: ${ACCENT};"
        "    font-size: ${TITLE}px;"
        "    font-weight: 600;"
        "    letter-spacing: 2px;"
        "    padding-left: 4px;"
        "}"
        // --- Subtitle ---
        "QLabel#titleSubtitle {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${SUBTITLE}px;"
        "    letter-spacing: 1px;"
        "    padding-left: 8px;"
        "}"
        // --- Window control buttons ---
        "QPushButton#minBtn, QPushButton#maxBtn {"
        "    background: transparent;"
        "    color: ${TEXT_SECONDARY};"
        "    border: none;"
        "    border-radius: 4px;"
        "    min-width: 28px; max-width: 28px;"
        "    min-height: 24px; max-height: 24px;"
        "    font-size: ${TITLE}px;"
        "    padding: 0px;"
        "}"
        "QPushButton#minBtn:hover, QPushButton#maxBtn:hover {"
        "    background: ${BG_ELEVATED};"
        "    color: ${TEXT_PRIMARY};"
        "}"
        "QPushButton#closeBtn {"
        "    background: transparent;"
        "    color: ${TEXT_SECONDARY};"
        "    border: none;"
        "    border-radius: 4px;"
        "    min-width: 28px; max-width: 28px;"
        "    min-height: 24px; max-height: 24px;"
        "    font-size: ${TITLE}px;"
        "    padding: 0px;"
        "    margin-right: 4px;"
        "}"
        "QPushButton#closeBtn:hover {"
        "    background: ${DANGER};"
        "    color: #ffffff;"
        "}"
        "QPushButton#closeBtn:pressed {"
        "    background: #b91c1c;"
        "    color: #ffffff;"
        "}"
        // --- Theme toggle ---
        "QPushButton#themeToggleBtn {"
        "    background: ${BG_SURFACE};"
        "    color: ${TEXT_SECONDARY};"
        "    border: 1px solid ${BORDER_SUBTLE};"
        "    border-radius: 6px;"
        "    min-height: 24px;"
        "    padding: 0px 10px;"
        "    font-size: ${SUBTITLE}px;"
        "    font-weight: 600;"
        "}"
        "QPushButton#themeToggleBtn:hover {"
        "    background: ${BG_ELEVATED};"
        "    color: ${TEXT_PRIMARY};"
        "    border-color: ${ACCENT};"
        "}"
        "QPushButton#themeToggleBtn:pressed {"
        "    background: ${BORDER_SUBTLE};"
        "    color: ${ACCENT_HOVER};"
        "    border-color: ${ACCENT};"
        "}"
        // --- Dock slot frames ---
        "QFrame#slot {"
        "    background: ${BG_PANEL};"
        "    border: 1px solid ${BORDER_SUBTLE};"
        "    border-radius: 10px;"
        "    padding: 0px;"
        "}"
        // --- Slot zone label ---
        "QLabel#slotZoneLabel {"
        "    color: ${BORDER_ACTIVE};"
        "    font-size: ${SMALL}px;"
        "    font-weight: 600;"
        "    letter-spacing: 3px;"
        "    padding: 0px 0px 0px 2px;"
        "}"
        // --- Tab bar ---
        "QTabBar {"
        "    background: transparent;"
        "    border: none;"
        "    border-bottom: 1px solid ${BORDER_SUBTLE};"
        "}"
        "QTabBar::tab {"
        "    background: transparent;"
        "    color: ${TEXT_MUTED};"
        "    border: none;"
        "    border-bottom: 2px solid transparent;"
        "    padding: ${TAB_VP}px ${TAB_HP}px ${TAB_VP}px ${TAB_HP}px;"
        "    font-size: ${TAB}px;"
        "    font-weight: 500;"
        "    letter-spacing: 0.5px;"
        "    min-width: 60px;"
        "}"
        "QTabBar::tab:hover {"
        "    color: ${TEXT_SECONDARY};"
        "    border-bottom: 2px solid ${BORDER_ACTIVE};"
        "}"
        "QTabBar::tab:selected {"
        "    color: ${TEXT_PRIMARY};"
        "    border-bottom: 2px solid ${ACCENT};"
        "    font-weight: 600;"
        "}"
        "QTabBar::tab:!enabled {"
        "    color: ${BORDER_ACTIVE};"
        "}"
        // --- Generic QPushButton ---
        "QPushButton {"
        "    background: ${BG_SURFACE};"
        "    color: ${TEXT_SECONDARY};"
        "    border: 1px solid ${BORDER_SUBTLE};"
        "    border-radius: 6px;"
        "    padding: 3px 10px;"
        "    font-size: ${BASE}px;"
        "}"
        "QPushButton:hover {"
        "    background: ${BG_ELEVATED};"
        "    color: ${TEXT_PRIMARY};"
        "    border-color: ${BORDER_ACTIVE};"
        "}"
        "QPushButton:pressed {"
        "    background: ${BORDER_SUBTLE};"
        "    color: ${ACCENT_HOVER};"
        "    border-color: ${ACCENT};"
        "}"
        "QPushButton:disabled {"
        "    background: ${BG_WINDOW};"
        "    color: ${BORDER_ACTIVE};"
        "    border-color: ${BORDER_SUBTLE};"
        "}"
        // --- Move pill buttons ---
        "QPushButton#moveBtn {"
        "    background: ${BG_SURFACE};"
        "    color: ${TEXT_MUTED};"
        "    border: 1px solid ${BORDER_SUBTLE};"
        "    border-radius: 10px;"
        "    padding: 2px 9px;"
        "    font-size: ${SUBTITLE}px;"
        "    font-weight: 600;"
        "    min-width: 22px;"
        "    max-height: 20px;"
        "}"
        "QPushButton#moveBtn:hover {"
        "    background: ${BG_ELEVATED};"
        "    color: ${ACCENT_HOVER};"
        "    border-color: ${ACCENT};"
        "}"
        "QPushButton#moveBtn:disabled {"
        "    background: ${BG_WINDOW};"
        "    color: ${BORDER_SUBTLE};"
        "    border-color: ${BG_SURFACE};"
        "}"
        // --- Panel header separator ---
        "QFrame#panelHeaderLine {"
        "    background: qlineargradient("
        "        x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 ${ACCENT},"
        "        stop:0.4 ${BORDER_SUBTLE},"
        "        stop:1 transparent"
        "    );"
        "    min-height: 1px;"
        "    max-height: 1px;"
        "    border: none;"
        "}"
        // --- Panel title ---
        "QLabel#panelTitle {"
        "    color: ${TEXT_PRIMARY};"
        "    font-size: ${PANEL_TITLE}px;"
        "    font-weight: 600;"
        "    letter-spacing: 1px;"
        "}"
        // --- Move-to label ---
        "QLabel#moveToLabel {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${SMALL}px;"
        "    letter-spacing: 1px;"
        "}"
        // --- Metric labels ---
        "QLabel#metricValue {"
        "    color: ${TEXT_PRIMARY};"
        "    font-size: ${METRIC_VAL}px;"
        "    font-weight: 700;"
        "    letter-spacing: -0.5px;"
        "}"
        "QLabel#metricKey {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${METRIC_KEY}px;"
        "    font-weight: 600;"
        "    letter-spacing: 2px;"
        "}"
        "QLabel#metricUnit {"
        "    color: ${ACCENT};"
        "    font-size: ${METRIC_UNIT}px;"
        "    font-weight: 600;"
        "}"
        // --- Telemetry timestamp ---
        "QLabel#telemetryTimestamp {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${SUBTITLE}px;"
        "}"
        // --- Telemetry status ---
        "QLabel#telemetryStatus {"
        "    color: ${TEXT_SECONDARY};"
        "    font-size: ${SUBTITLE}px;"
        "    font-style: italic;"
        "}"
        // --- Process status ---
        "QLabel#processStatus {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${SUBTITLE}px;"
        "    font-style: italic;"
        "    padding-bottom: 4px;"
        "}"
        // --- Process row labels ---
        "QLabel#processRow {"
        "    color: ${TEXT_SECONDARY};"
        "    font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "    font-size: ${SUBTITLE}px;"
        "    padding: 3px 6px;"
        "    border-radius: 3px;"
        "}"
        "QLabel#processRowAlt {"
        "    color: ${TEXT_SECONDARY};"
        "    font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "    font-size: ${SUBTITLE}px;"
        "    background: ${BG_PANEL};"
        "    padding: 3px 6px;"
        "    border-radius: 3px;"
        "}"
        // --- Timeline / render status ---
        "QLabel#timelineStatus {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${SUBTITLE}px;"
        "    font-style: italic;"
        "}"
        "QLabel#renderStatus {"
        "    color: ${TEXT_MUTED};"
        "    font-size: ${SUBTITLE}px;"
        "    font-style: italic;"
        "}"
        // --- Footer ---
        "QLabel#footerStatus {"
        "    color: ${BORDER_ACTIVE};"
        "    font-size: ${SMALL}px;"
        "    font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "    padding: 4px 12px 8px 14px;"
        "    border-left: 2px solid ${ACCENT};"
        "    margin-left: 12px;"
        "    margin-bottom: 4px;"
        "}"
        // --- Scrollbars ---
        "QScrollBar:vertical {"
        "    background: ${BG_WINDOW};"
        "    width: 6px;"
        "    border-radius: 3px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: ${BORDER_SUBTLE};"
        "    border-radius: 3px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: ${BORDER_ACTIVE};"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
        "QScrollBar:horizontal {"
        "    background: ${BG_WINDOW};"
        "    height: 6px;"
        "    border-radius: 3px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "    background: ${BORDER_SUBTLE};"
        "    border-radius: 3px;"
        "    min-width: 20px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "    background: ${BORDER_ACTIVE};"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "    width: 0px;"
        "}"
    );

    // Size metric tokens
    ss.replace(QLatin1String("${BASE}"), QString::number(m.base_font));
    ss.replace(QLatin1String("${TITLE}"), QString::number(m.title_font));
    ss.replace(QLatin1String("${SUBTITLE}"), QString::number(m.subtitle_font));
    ss.replace(QLatin1String("${METRIC_VAL}"), QString::number(m.metric_value_font));
    ss.replace(QLatin1String("${METRIC_KEY}"), QString::number(m.metric_key_font));
    ss.replace(QLatin1String("${METRIC_UNIT}"), QString::number(m.metric_unit_font));
    ss.replace(QLatin1String("${TAB}"), QString::number(m.tab_font));
    ss.replace(QLatin1String("${SMALL}"), QString::number(m.small_font));
    ss.replace(QLatin1String("${TB_H}"), QString::number(m.titlebar_height));
    ss.replace(QLatin1String("${TAB_VP}"), QString::number(m.tab_v_padding));
    ss.replace(QLatin1String("${TAB_HP}"), QString::number(m.tab_h_padding));
    ss.replace(QLatin1String("${PANEL_TITLE}"), QString::number(m.base_font + 1));

    // Color tokens
    ss.replace(QLatin1String("${TEXT_PRIMARY}"), p.text_primary);
    ss.replace(QLatin1String("${BG_WINDOW}"), p.bg_window);
    ss.replace(QLatin1String("${BG_SURFACE}"), p.bg_surface);
    ss.replace(QLatin1String("${BG_PANEL}"), p.bg_panel);
    ss.replace(QLatin1String("${BORDER_SUBTLE}"), p.border_subtle);
    ss.replace(QLatin1String("${ACCENT}"), p.accent);
    ss.replace(QLatin1String("${TEXT_MUTED}"), p.text_muted);
    ss.replace(QLatin1String("${TEXT_SECONDARY}"), p.text_secondary);
    ss.replace(QLatin1String("${BG_ELEVATED}"), p.bg_elevated);
    ss.replace(QLatin1String("${DANGER}"), p.danger);
    ss.replace(QLatin1String("${ACCENT_HOVER}"), p.accent_hover);
    ss.replace(QLatin1String("${BORDER_ACTIVE}"), p.border_active);

    // Pink-mode visual enhancements
    if (mode == aura::shell::UiThemeMode::PinkCute) {
        ss += QStringLiteral(
            "QFrame#slot { border: 1px solid #91406f; }"
            "QLabel#metricValue { color: #ffb3d9; }"
            "QLabel#processRow { background: rgba(255, 77, 166, 0.06); border-radius: 8px; }"
            "QLabel#processRowAlt { background: rgba(192, 132, 252, 0.06); border-radius: 8px; }"
            "QLabel#panelTitle { color: #ff4da6; }"
            "QLabel#footerStatus { border-left-color: #ff4da6; }"
        );
    }

    return ss;
}

// ---------------------------------------------------------------------------
// AuraShellWindow
// ---------------------------------------------------------------------------

class AuraShellWindow final : public QMainWindow {
public:
    explicit AuraShellWindow(const LaunchConfig& config, QWidget* parent = nullptr)
        : QMainWindow(parent),
          config_(config) {
        current_theme_mode_ = aura::shell::ui_theme_mode_from_key(
            settings_.value(k_theme_mode_setting_key, QStringLiteral("dark_blue"))
                .toString()
                .toStdString()
        );
        setWindowTitle("Aura | Native Shell");
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowMinMaxButtonsHint);
        setMouseTracking(true);

        // Responsive minimum — enables split-screen on 1080p+
        setMinimumSize(420, 300);
        if (const QScreen* screen = QApplication::primaryScreen()) {
            const QRect avail = screen->availableGeometry();
            resize(static_cast<int>(avail.width() * 0.75),
                   static_cast<int>(avail.height() * 0.75));
        }
        current_size_category_ = classify_window_size(width(), height());
        current_metrics_ = metrics_for_category(current_size_category_);
        setStyleSheet(build_app_stylesheet(current_theme_mode_, current_metrics_));
        current_interval_seconds_ =
            (std::isfinite(config.interval_seconds) && config.interval_seconds > 0.0)
                ? config.interval_seconds
                : 1.0;

        aura::shell::CockpitController::Config controller_config;
        controller_config.poll_interval_seconds = current_interval_seconds_;
        controller_config.max_process_rows = process_labels_.size();
        if (config.db_path.has_value()) {
            controller_config.db_path = config.db_path->toStdString();
        }
        controller_ = std::make_unique<aura::shell::CockpitController>(
            std::make_unique<aura::shell::TelemetryBridge>(),
            std::make_unique<aura::shell::RenderBridge>(),
            std::make_unique<aura::shell::TimelineBridge>(),
            std::move(controller_config)
        );

        auto* root = new QWidget(this);
        auto* root_layout = new QVBoxLayout(root);
        root_layout->setContentsMargins(0, 0, 0, 0);
        root_layout->setSpacing(0);

        // ---------------------------------------------------------------
        // Title bar
        // ---------------------------------------------------------------
        titlebar_ = new QFrame(root);
        titlebar_->setObjectName("titlebar");
        titlebar_->installEventFilter(this);

        auto* title_layout = new QHBoxLayout(titlebar_);
        title_layout->setContentsMargins(12, 0, 6, 0);
        title_layout->setSpacing(0);

        // Logo / app name
        auto* logo_label = new QLabel("\u25c8 AURA", titlebar_);
        logo_label->setObjectName("appLogo");
        title_layout->addWidget(logo_label);

        // Subtitle
        auto* subtitle_label = new QLabel("NATIVE COCKPIT", titlebar_);
        subtitle_label->setObjectName("titleSubtitle");
        title_layout->addWidget(subtitle_label);

        theme_toggle_btn_ = new QPushButton(titlebar_);
        theme_toggle_btn_->setObjectName("themeToggleBtn");
        theme_toggle_btn_->setFocusPolicy(Qt::NoFocus);
        theme_toggle_btn_->setCursor(Qt::ArrowCursor);
        theme_toggle_btn_->setToolTip("Switch between blue and pink cute themes");
        title_layout->addWidget(theme_toggle_btn_);
        title_layout->addSpacing(8);

        title_layout->addStretch(1);

        // Window-control buttons with Unicode glyphs
        auto* min_btn = new QPushButton("\u2212", titlebar_);   // − (minus)
        auto* max_btn = new QPushButton("\u25a1", titlebar_);   // □ (square)
        auto* close_btn = new QPushButton("\u00d7", titlebar_); // × (times)

        min_btn->setObjectName("minBtn");
        max_btn->setObjectName("maxBtn");
        close_btn->setObjectName("closeBtn");

        min_btn->setToolTip("Minimize");
        max_btn->setToolTip("Maximize / Restore");
        close_btn->setToolTip("Close");

        min_btn->setCursor(Qt::ArrowCursor);
        max_btn->setCursor(Qt::ArrowCursor);
        close_btn->setCursor(Qt::ArrowCursor);

        min_btn->setFocusPolicy(Qt::NoFocus);
        max_btn->setFocusPolicy(Qt::NoFocus);
        close_btn->setFocusPolicy(Qt::NoFocus);

        connect(min_btn, &QPushButton::clicked, this, &QWidget::showMinimized);
        connect(max_btn, &QPushButton::clicked, this, [this]() {
            if (isMaximized()) {
                showNormal();
            } else {
                showMaximized();
            }
        });
        connect(close_btn, &QPushButton::clicked, this, &QWidget::close);
        connect(theme_toggle_btn_, &QPushButton::clicked, this, [this]() {
            apply_theme(aura::shell::toggle_ui_theme_mode(current_theme_mode_), true);
        });

        title_layout->addWidget(min_btn);
        title_layout->addSpacing(2);
        title_layout->addWidget(max_btn);
        title_layout->addSpacing(2);
        title_layout->addWidget(close_btn);

        root_layout->addWidget(titlebar_);

        // ---------------------------------------------------------------
        // Main body — three dock slots
        // ---------------------------------------------------------------
        body_ = new QWidget(root);
        body_->setAutoFillBackground(false);
        auto* body_layout = new QHBoxLayout(body_);
        body_layout->setContentsMargins(
            current_metrics_.body_margin, current_metrics_.body_margin,
            current_metrics_.body_margin, current_metrics_.body_margin);
        body_layout->setSpacing(current_metrics_.body_spacing);

        for (const auto slot : aura::shell::all_dock_slots()) {
            const std::size_t index = slot_index(slot);
            slot_widgets_[index] = build_slot(slot, body_);
            connect(slot_widgets_[index].tab_bar, &QTabBar::currentChanged, this, [this, slot](const int tab_index) {
                on_tab_changed(slot, tab_index);
            });
            body_layout->addWidget(
                slot_widgets_[index].frame,
                slot == aura::shell::DockSlot::Center ? 2 : 1
            );
        }

        build_panel_pages();
        rebuild_dock_slots();
        root_layout->addWidget(body_, 1);

        // ---------------------------------------------------------------
        // Footer status bar
        // ---------------------------------------------------------------
        footer_status_ = new QLabel(
            QString("interval=%1  persist=%2  db=%3")
                .arg(config.interval_seconds)
                .arg(config.persistence_enabled ? "on" : "off")
                .arg(config.db_path.value_or("<none>")),
            root
        );
        footer_status_->setObjectName("footerStatus");
        footer_status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        root_layout->addWidget(footer_status_);

        setCentralWidget(root);

        // ---------------------------------------------------------------
        // Refresh timer
        // ---------------------------------------------------------------
        update_timer_ = new QTimer(this);
        update_timer_->setInterval(interval_to_milliseconds(current_interval_seconds_));
        connect(update_timer_, &QTimer::timeout, this, [this]() {
            refresh_cockpit();
        });
        update_timer_->start();
        apply_theme(current_theme_mode_, false);
        refresh_cockpit();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == titlebar_) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->button() == Qt::LeftButton) {
                    dragging_ = true;
                    drag_origin_ = mouse_event->globalPosition().toPoint() - frameGeometry().topLeft();
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove) {
                auto* mouse_event = static_cast<QMouseEvent*>(event);
                if (dragging_ && (mouse_event->buttons() & Qt::LeftButton) && !isMaximized()) {
                    move(mouse_event->globalPosition().toPoint() - drag_origin_);
                    return true;
                }
            } else if (event->type() == QEvent::MouseButtonRelease) {
                dragging_ = false;
            } else if (event->type() == QEvent::MouseButtonDblClick) {
                if (isMaximized()) {
                    showNormal();
                } else {
                    showMaximized();
                }
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    // Edge resize for frameless window — detect edges and resize via Qt
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && !isMaximized()) {
            resize_edge_ = hit_test_edge(event->pos());
            if (resize_edge_ != Qt::Edges{}) {
                resize_origin_ = event->globalPosition().toPoint();
                resize_geometry_ = geometry();
                event->accept();
                return;
            }
        }
        QMainWindow::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (resize_edge_ != Qt::Edges{} && (event->buttons() & Qt::LeftButton) && !isMaximized()) {
            const QPoint delta = event->globalPosition().toPoint() - resize_origin_;
            QRect geo = resize_geometry_;
            if (resize_edge_ & Qt::LeftEdge)   geo.setLeft(geo.left() + delta.x());
            if (resize_edge_ & Qt::RightEdge)  geo.setRight(geo.right() + delta.x());
            if (resize_edge_ & Qt::TopEdge)    geo.setTop(geo.top() + delta.y());
            if (resize_edge_ & Qt::BottomEdge) geo.setBottom(geo.bottom() + delta.y());
            // Enforce minimum size
            const QSize min_sz = minimumSize();
            if (geo.width() < min_sz.width()) {
                if (resize_edge_ & Qt::LeftEdge) geo.setLeft(geo.right() - min_sz.width());
                else geo.setRight(geo.left() + min_sz.width());
            }
            if (geo.height() < min_sz.height()) {
                if (resize_edge_ & Qt::TopEdge) geo.setTop(geo.bottom() - min_sz.height());
                else geo.setBottom(geo.top() + min_sz.height());
            }
            setGeometry(geo);
            event->accept();
            return;
        }
        // Update cursor shape on hover
        if (!isMaximized()) {
            const Qt::Edges edge = hit_test_edge(event->pos());
            if (edge == (Qt::LeftEdge | Qt::TopEdge) || edge == (Qt::RightEdge | Qt::BottomEdge))
                setCursor(Qt::SizeFDiagCursor);
            else if (edge == (Qt::RightEdge | Qt::TopEdge) || edge == (Qt::LeftEdge | Qt::BottomEdge))
                setCursor(Qt::SizeBDiagCursor);
            else if (edge & (Qt::LeftEdge | Qt::RightEdge))
                setCursor(Qt::SizeHorCursor);
            else if (edge & (Qt::TopEdge | Qt::BottomEdge))
                setCursor(Qt::SizeVerCursor);
            else
                unsetCursor();
        }
        QMainWindow::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        resize_edge_ = Qt::Edges{};
        QMainWindow::mouseReleaseEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override {
        QMainWindow::resizeEvent(event);
        const auto new_cat = classify_window_size(event->size().width(), event->size().height());
        if (new_cat != current_size_category_) {
            current_size_category_ = new_cat;
            current_metrics_ = metrics_for_category(new_cat);
            setStyleSheet(build_app_stylesheet(current_theme_mode_, current_metrics_));
            if (body_ != nullptr) {
                if (auto* bl = body_->layout()) {
                    bl->setContentsMargins(
                        current_metrics_.body_margin, current_metrics_.body_margin,
                        current_metrics_.body_margin, current_metrics_.body_margin);
                    bl->setSpacing(current_metrics_.body_spacing);
                }
            }
            // Update slot internal padding to match new category
            for (const auto slot : aura::shell::all_dock_slots()) {
                auto& sw = slot_widgets_[slot_index(slot)];
                if (sw.frame != nullptr && sw.frame->layout() != nullptr) {
                    const int top_pad = current_metrics_.body_margin > 8 ? 8 : 4;
                    const int bot_pad = current_metrics_.body_margin > 8 ? 10 : 6;
                    sw.frame->layout()->setContentsMargins(0, top_pad, 0, bot_pad);
                }
            }
        }
    }

private:
    static constexpr int kResizeBorder = 14;

    Qt::Edges hit_test_edge(const QPoint& pos) const {
        Qt::Edges edges;
        if (pos.x() < kResizeBorder)                edges |= Qt::LeftEdge;
        if (pos.x() >= width() - kResizeBorder)     edges |= Qt::RightEdge;
        if (pos.y() < kResizeBorder)                edges |= Qt::TopEdge;
        if (pos.y() >= height() - kResizeBorder)    edges |= Qt::BottomEdge;
        return edges;
    }

    void sync_theme_to_qml() {
        if (quick_ == nullptr || quick_->rootObject() == nullptr) {
            return;
        }
        QQuickItem* root = quick_->rootObject();
        root->setProperty(
            "themeMode",
            QString::fromStdString(aura::shell::ui_theme_mode_key(current_theme_mode_))
        );
    }

    void apply_theme(const aura::shell::UiThemeMode mode, const bool persist) {
        current_theme_mode_ = mode;
        is_pink_ = (mode == aura::shell::UiThemeMode::PinkCute);
        theme_dirty_ = true;
        setStyleSheet(build_app_stylesheet(current_theme_mode_, current_metrics_));
        if (theme_toggle_btn_ != nullptr) {
            theme_toggle_btn_->setText(QString::fromStdString(aura::shell::ui_theme_mode_label(mode)));
        }
        if (persist) {
            settings_.setValue(
                k_theme_mode_setting_key,
                QString::fromStdString(aura::shell::ui_theme_mode_key(current_theme_mode_))
            );
        }

        // Update panel title labels
        using PanelId = aura::shell::PanelId;
        if (panel_title_labels_[panel_index(PanelId::TelemetryOverview)])
            panel_title_labels_[panel_index(PanelId::TelemetryOverview)]->setText(
                is_pink_ ? QStringLiteral("\u2728 SYSTEM STATUS") : QStringLiteral("TELEMETRY OVERVIEW"));
        if (panel_title_labels_[panel_index(PanelId::TopProcesses)])
            panel_title_labels_[panel_index(PanelId::TopProcesses)]->setText(
                is_pink_ ? QStringLiteral("\u2728 RUNNING APPS") : QStringLiteral("TOP PROCESSES"));
        if (panel_title_labels_[panel_index(PanelId::DvrTimeline)])
            panel_title_labels_[panel_index(PanelId::DvrTimeline)]->setText(
                is_pink_ ? QStringLiteral("\u2728 HISTORY") : QStringLiteral("DVR TIMELINE"));
        if (panel_title_labels_[panel_index(PanelId::RenderSurface)])
            panel_title_labels_[panel_index(PanelId::RenderSurface)]->setText(
                is_pink_ ? QStringLiteral("\u2728 COCKPIT") : QStringLiteral("RENDER SURFACE"));

        // Update tab text in all slot tab bars
        for (const auto slot : aura::shell::all_dock_slots()) {
            const std::size_t slot_idx = slot_index(slot);
            auto& sw = slot_widgets_[slot_idx];
            if (sw.tab_bar == nullptr) continue;
            const auto& tabs = dock_state_.slot_tabs[slot_idx];
            for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
                switch (tabs[static_cast<std::size_t>(i)]) {
                    case PanelId::TelemetryOverview:
                        sw.tab_bar->setTabText(i, is_pink_ ? QStringLiteral("Status") : QStringLiteral("Telemetry"));
                        break;
                    case PanelId::TopProcesses:
                        sw.tab_bar->setTabText(i, is_pink_ ? QStringLiteral("Apps") : QStringLiteral("Processes"));
                        break;
                    case PanelId::DvrTimeline:
                        sw.tab_bar->setTabText(i, is_pink_ ? QStringLiteral("History") : QStringLiteral("Timeline"));
                        break;
                    case PanelId::RenderSurface:
                        sw.tab_bar->setTabText(i, is_pink_ ? QStringLiteral("Cockpit") : QStringLiteral("Render"));
                        break;
                }
            }
        }

        // Update metric key labels
        if (cpu_key_) cpu_key_->setText(is_pink_ ? QStringLiteral("PROCESSOR") : QStringLiteral("CPU LOAD"));
        if (mem_key_) mem_key_->setText(is_pink_ ? QStringLiteral("MEMORY") : QStringLiteral("MEMORY USE"));
        if (gpu_key_) gpu_key_->setText(is_pink_ ? QStringLiteral("GRAPHICS") : QStringLiteral("GPU"));
        if (disk_key_) disk_key_->setText(is_pink_ ? QStringLiteral("STORAGE") : QStringLiteral("DISK ACTIVITY"));
        if (net_key_) net_key_->setText(is_pink_ ? QStringLiteral("INTERNET") : QStringLiteral("NETWORK"));
        if (thermal_key_) thermal_key_->setText(is_pink_ ? QStringLiteral("TEMPERATURE") : QStringLiteral("TEMPERATURE"));

        sync_theme_to_qml();
    }
    // -----------------------------------------------------------------------
    // build_slot  —  creates one of the three dock-slot frames
    // -----------------------------------------------------------------------
    SlotWidgets build_slot(const aura::shell::DockSlot slot, QWidget* parent) {
        auto* frame = new QFrame(parent);
        frame->setObjectName("slot");

        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(0, 8, 0, 10);
        layout->setSpacing(0);

        // Tiny zone label (LEFT / CENTER / RIGHT) rendered as watermark text
        auto* zone_label = new QLabel(slot_title(slot), frame);
        zone_label->setObjectName("slotZoneLabel");
        zone_label->setContentsMargins(12, 0, 0, 2);

        // Tab bar sits flush below the zone label
        auto* tab_bar = new QTabBar(frame);
        tab_bar->setDocumentMode(true);
        tab_bar->setExpanding(false);
        tab_bar->setMovable(false);
        tab_bar->setUsesScrollButtons(true);
        tab_bar->setDrawBase(false);
        tab_bar->setContentsMargins(8, 0, 8, 0);

        // Content stack — padded inside the frame
        auto* stack = new QStackedWidget(frame);
        stack->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(zone_label);
        layout->addWidget(tab_bar);
        layout->addWidget(stack, 1);

        return {frame, tab_bar, stack};
    }

    // -----------------------------------------------------------------------
    // create_panel_page  —  builds a panel widget with premium header
    // -----------------------------------------------------------------------
    QVBoxLayout* create_panel_page(const aura::shell::PanelId panel_id, const QString& title) {
        auto* page = new QWidget(this);
        panel_pages_[panel_index(panel_id)] = page;

        auto* page_layout = new QVBoxLayout(page);
        page_layout->setContentsMargins(12, 10, 12, 8);
        page_layout->setSpacing(0);

        // --- Header row ---
        auto* header_layout = new QHBoxLayout();
        header_layout->setContentsMargins(0, 0, 0, 0);
        header_layout->setSpacing(6);

        auto* title_label = new QLabel(title.toUpper(), page);
        title_label->setObjectName("panelTitle");
        panel_title_labels_[panel_index(panel_id)] = title_label;
        header_layout->addWidget(title_label);
        header_layout->addStretch(1);

        auto* move_to_label = new QLabel("MOVE:", page);
        move_to_label->setObjectName("moveToLabel");
        header_layout->addWidget(move_to_label);

        for (const auto slot : aura::shell::all_dock_slots()) {
            auto* move_button = new QPushButton(slot_short_label(slot), page);
            move_button->setObjectName("moveBtn");
            move_button->setFocusPolicy(Qt::NoFocus);
            connect(move_button, &QPushButton::clicked, this, [this, panel_id, slot]() {
                move_panel_to_slot(panel_id, slot);
            });
            panel_move_buttons_[panel_index(panel_id)][slot_index(slot)] = move_button;
            header_layout->addWidget(move_button);
        }

        page_layout->addLayout(header_layout);
        page_layout->addSpacing(6);

        // Accent separator line below header
        auto* sep_line = new QFrame(page);
        sep_line->setObjectName("panelHeaderLine");
        sep_line->setFrameShape(QFrame::HLine);
        sep_line->setFixedHeight(1);
        page_layout->addWidget(sep_line);
        page_layout->addSpacing(10);

        // Content area
        auto* content_layout = new QVBoxLayout();
        content_layout->setContentsMargins(0, 0, 0, 0);
        content_layout->setSpacing(4);
        page_layout->addLayout(content_layout, 1);

        return content_layout;
    }

    // -----------------------------------------------------------------------
    // build_panel_pages  —  constructs the 4 panel content areas
    // -----------------------------------------------------------------------
    void build_panel_pages() {
        using aura::shell::PanelId;

        // --- Telemetry Overview ---
        {
            auto* telemetry_layout =
                create_panel_page(PanelId::TelemetryOverview, QStringLiteral("Telemetry Overview"));

            QWidget* parent_page = panel_pages_[panel_index(PanelId::TelemetryOverview)];

            // CPU metric block
            auto* cpu_block = new QWidget(parent_page);
            auto* cpu_block_layout = new QVBoxLayout(cpu_block);
            cpu_block_layout->setContentsMargins(0, 0, 0, 8);
            cpu_block_layout->setSpacing(1);

            cpu_key_ = new QLabel("CPU LOAD", parent_page);
            cpu_key_->setObjectName("metricKey");
            telemetry_cpu_ = new QLabel("CPU --", parent_page);
            telemetry_cpu_->setObjectName("metricValue");

            cpu_block_layout->addWidget(cpu_key_);
            cpu_block_layout->addWidget(telemetry_cpu_);
            telemetry_layout->addWidget(cpu_block);

            // Memory metric block
            auto* mem_block = new QWidget(parent_page);
            auto* mem_block_layout = new QVBoxLayout(mem_block);
            mem_block_layout->setContentsMargins(0, 0, 0, 8);
            mem_block_layout->setSpacing(1);

            mem_key_ = new QLabel("MEMORY USE", parent_page);
            mem_key_->setObjectName("metricKey");
            telemetry_memory_ = new QLabel("Memory --", parent_page);
            telemetry_memory_->setObjectName("metricValue");

            mem_block_layout->addWidget(mem_key_);
            mem_block_layout->addWidget(telemetry_memory_);
            telemetry_layout->addWidget(mem_block);

            // GPU metric block
            auto* gpu_block = new QWidget(parent_page);
            auto* gpu_block_layout = new QVBoxLayout(gpu_block);
            gpu_block_layout->setContentsMargins(0, 0, 0, 8);
            gpu_block_layout->setSpacing(1);

            gpu_key_ = new QLabel("GPU", parent_page);
            gpu_key_->setObjectName("metricKey");
            gpu_value_ = new QLabel("--", parent_page);
            gpu_value_->setObjectName("metricValue");

            gpu_block_layout->addWidget(gpu_key_);
            gpu_block_layout->addWidget(gpu_value_);
            telemetry_layout->addWidget(gpu_block);

            // Disk metric block
            auto* disk_block = new QWidget(parent_page);
            auto* disk_block_layout = new QVBoxLayout(disk_block);
            disk_block_layout->setContentsMargins(0, 0, 0, 8);
            disk_block_layout->setSpacing(1);

            disk_key_ = new QLabel("DISK ACTIVITY", parent_page);
            disk_key_->setObjectName("metricKey");
            disk_value_ = new QLabel("--", parent_page);
            disk_value_->setObjectName("metricValue");

            disk_block_layout->addWidget(disk_key_);
            disk_block_layout->addWidget(disk_value_);
            telemetry_layout->addWidget(disk_block);

            // Network metric block
            auto* net_block = new QWidget(parent_page);
            auto* net_block_layout = new QVBoxLayout(net_block);
            net_block_layout->setContentsMargins(0, 0, 0, 8);
            net_block_layout->setSpacing(1);

            net_key_ = new QLabel("NETWORK", parent_page);
            net_key_->setObjectName("metricKey");
            net_value_ = new QLabel("--", parent_page);
            net_value_->setObjectName("metricValue");

            net_block_layout->addWidget(net_key_);
            net_block_layout->addWidget(net_value_);
            telemetry_layout->addWidget(net_block);

            // Thermal metric block
            auto* thermal_block = new QWidget(parent_page);
            auto* thermal_block_layout = new QVBoxLayout(thermal_block);
            thermal_block_layout->setContentsMargins(0, 0, 0, 8);
            thermal_block_layout->setSpacing(1);

            thermal_key_ = new QLabel("TEMPERATURE", parent_page);
            thermal_key_->setObjectName("metricKey");
            thermal_value_ = new QLabel("--", parent_page);
            thermal_value_->setObjectName("metricValue");

            thermal_block_layout->addWidget(thermal_key_);
            thermal_block_layout->addWidget(thermal_value_);
            telemetry_layout->addWidget(thermal_block);

            // Timestamp and status
            telemetry_timestamp_ = new QLabel("Timestamp --", parent_page);
            telemetry_timestamp_->setObjectName("telemetryTimestamp");
            telemetry_layout->addWidget(telemetry_timestamp_);

            telemetry_status_ = new QLabel("Awaiting telemetry snapshot...", parent_page);
            telemetry_status_->setObjectName("telemetryStatus");
            telemetry_status_->setWordWrap(true);
            telemetry_layout->addWidget(telemetry_status_);

            telemetry_layout->addStretch(1);
        }

        // --- Top Processes ---
        {
            auto* processes_layout =
                create_panel_page(PanelId::TopProcesses, QStringLiteral("Top Processes"));

            QWidget* parent_page = panel_pages_[panel_index(PanelId::TopProcesses)];

            process_status_ = new QLabel("Waiting for process samples...", parent_page);
            process_status_->setObjectName("processStatus");
            process_status_->setWordWrap(true);
            processes_layout->addWidget(process_status_);

            for (std::size_t i = 0; i < process_labels_.size(); ++i) {
                process_labels_[i] = new QLabel("-", parent_page);
                // Alternate row styling for readability
                process_labels_[i]->setObjectName((i % 2 == 0) ? "processRow" : "processRowAlt");
                process_labels_[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
                process_labels_[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                processes_layout->addWidget(process_labels_[i]);
            }
            processes_layout->addStretch(1);
        }

        // --- DVR Timeline ---
        {
            auto* timeline_layout =
                create_panel_page(PanelId::DvrTimeline, QStringLiteral("DVR Timeline"));

            QWidget* parent_page = panel_pages_[panel_index(PanelId::DvrTimeline)];

            timeline_status_ = new QLabel("Awaiting timeline samples...", parent_page);
            timeline_status_->setObjectName("timelineStatus");
            timeline_status_->setWordWrap(true);
            timeline_layout->addWidget(timeline_status_);
            timeline_layout->addStretch(1);
        }

        // --- Render Surface ---
        {
            auto* render_layout =
                create_panel_page(PanelId::RenderSurface, QStringLiteral("Render Surface"));

            QWidget* parent_page = panel_pages_[panel_index(PanelId::RenderSurface)];

            quick_ = new QQuickWidget(parent_page);
            quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
            quick_->setSource(QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_QML_PATH)));
            quick_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            render_status_ = new QLabel("Cockpit scene online", parent_page);
            render_status_->setObjectName("renderStatus");
            render_status_->setWordWrap(true);

            render_layout->addWidget(quick_, 1);
            render_layout->addWidget(render_status_);
        }
    }

    // -----------------------------------------------------------------------
    // Dock model helpers
    // -----------------------------------------------------------------------
    std::optional<aura::shell::DockSlot> panel_slot(const aura::shell::PanelId panel_id) const {
        for (const auto slot : aura::shell::all_dock_slots()) {
            const auto& tabs = dock_state_.slot_tabs[slot_index(slot)];
            if (std::find(tabs.begin(), tabs.end(), panel_id) != tabs.end()) {
                return slot;
            }
        }
        return std::nullopt;
    }

    QString themed_tab_title(const aura::shell::PanelId panel_id) const {
        if (is_pink_) {
            switch (panel_id) {
                case aura::shell::PanelId::TelemetryOverview: return QStringLiteral("Status");
                case aura::shell::PanelId::TopProcesses:      return QStringLiteral("Apps");
                case aura::shell::PanelId::DvrTimeline:        return QStringLiteral("History");
                case aura::shell::PanelId::RenderSurface:      return QStringLiteral("Cockpit");
            }
        }
        return panel_title(panel_id);
    }

    void rebuild_dock_slots() {
        syncing_tabs_ = true;
        for (const auto slot : aura::shell::all_dock_slots()) {
            const std::size_t slot_idx = slot_index(slot);
            auto& sw = slot_widgets_[slot_idx];

            while (sw.tab_bar->count() > 0) {
                sw.tab_bar->removeTab(0);
            }
            while (sw.stack->count() > 0) {
                sw.stack->removeWidget(sw.stack->widget(0));
            }
        }

        for (const auto slot : aura::shell::all_dock_slots()) {
            const std::size_t slot_idx = slot_index(slot);
            auto& sw = slot_widgets_[slot_idx];
            const auto& tabs = dock_state_.slot_tabs[slot_idx];

            for (const auto panel_id : tabs) {
                sw.tab_bar->addTab(themed_tab_title(panel_id));
                QWidget* page = panel_pages_[panel_index(panel_id)];
                if (page != nullptr) {
                    sw.stack->addWidget(page);
                }
            }

            if (tabs.empty()) {
                sw.tab_bar->setCurrentIndex(-1);
                sw.stack->setCurrentIndex(-1);
                continue;
            }

            const int active_tab = static_cast<int>(std::min(
                dock_state_.active_tab[slot_idx],
                tabs.size() - 1U
            ));
            sw.tab_bar->setCurrentIndex(active_tab);
            sw.stack->setCurrentIndex(active_tab);
        }
        syncing_tabs_ = false;
        update_move_button_states();
    }

    void on_tab_changed(const aura::shell::DockSlot slot, const int tab_index) {
        if (syncing_tabs_ || tab_index < 0) {
            return;
        }

        const std::size_t slot_idx = slot_index(slot);
        const auto tab_count = dock_state_.slot_tabs[slot_idx].size();
        if (tab_count == 0U || static_cast<std::size_t>(tab_index) >= tab_count) {
            return;
        }

        try {
            dock_state_ = aura::shell::set_active_tab(dock_state_, slot, static_cast<std::size_t>(tab_index));
        } catch (const std::invalid_argument&) {
            return;
        }

        syncing_tabs_ = true;
        slot_widgets_[slot_idx].stack->setCurrentIndex(tab_index);
        syncing_tabs_ = false;
        update_move_button_states();
    }

    void move_panel_to_slot(
        const aura::shell::PanelId panel_id,
        const aura::shell::DockSlot target_slot
    ) {
        try {
            dock_state_ = aura::shell::move_panel(
                dock_state_,
                aura::shell::PanelMoveRequest{
                    panel_id,
                    target_slot,
                    std::nullopt,
                }
            );
        } catch (const std::invalid_argument&) {
            return;
        }
        rebuild_dock_slots();
    }

    void update_move_button_states() {
        for (const auto panel_id : aura::shell::all_panel_ids()) {
            const std::optional<aura::shell::DockSlot> source_slot = panel_slot(panel_id);
            for (const auto target_slot : aura::shell::all_dock_slots()) {
                QPushButton* button = panel_move_buttons_[panel_index(panel_id)][slot_index(target_slot)];
                if (button == nullptr) {
                    continue;
                }
                const bool disable = source_slot.has_value() && *source_slot == target_slot;
                button->setEnabled(!disable);
            }
        }
    }

    // -----------------------------------------------------------------------
    // refresh_cockpit  —  propagate CockpitController state to all widgets
    // -----------------------------------------------------------------------
    void refresh_cockpit() {
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
                .arg(QString::fromStdString(aura::shell::ui_theme_mode_key(current_theme_mode_)));
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
                    QString::fromStdString(aura::shell::ui_theme_mode_key(current_theme_mode_))
                );
                theme_dirty_ = false;
            }
        }

        if (update_timer_ != nullptr) {
            current_interval_seconds_ = static_cast<double>(update_timer_->interval()) / 1000.0;
        }
    }

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------
    LaunchConfig config_{};
    QSettings settings_{QStringLiteral("Aura"), QStringLiteral("AuraShell")};
    aura::shell::UiThemeMode current_theme_mode_{aura::shell::UiThemeMode::DarkBlue};
    SizeCategory current_size_category_{SizeCategory::Regular};
    SizeMetrics current_metrics_{metrics_for_category(SizeCategory::Regular)};
    QWidget* body_{nullptr};
    double current_interval_seconds_{1.0};
    std::unique_ptr<aura::shell::CockpitController> controller_;
    aura::shell::DockState dock_state_{aura::shell::build_default_dock_state()};
    std::array<SlotWidgets, 3> slot_widgets_{};
    std::array<QWidget*, 4> panel_pages_{};
    std::array<std::array<QPushButton*, 3>, 4> panel_move_buttons_{};
    bool syncing_tabs_{false};
    QFrame* titlebar_{nullptr};
    QPushButton* theme_toggle_btn_{nullptr};
    QQuickWidget* quick_{nullptr};
    QTimer* update_timer_{nullptr};
    QLabel* telemetry_cpu_{nullptr};
    QLabel* telemetry_memory_{nullptr};
    QLabel* telemetry_timestamp_{nullptr};
    QLabel* telemetry_status_{nullptr};
    QLabel* process_status_{nullptr};
    QLabel* timeline_status_{nullptr};
    QLabel* render_status_{nullptr};
    QLabel* footer_status_{nullptr};
    std::array<QLabel*, 5> process_labels_{};
    bool theme_dirty_{true};
    bool is_pink_{false};

    // Extended metric labels (telemetry panel)
    QLabel* cpu_key_{nullptr};
    QLabel* mem_key_{nullptr};
    QLabel* gpu_key_{nullptr};
    QLabel* gpu_value_{nullptr};
    QLabel* disk_key_{nullptr};
    QLabel* disk_value_{nullptr};
    QLabel* net_key_{nullptr};
    QLabel* net_value_{nullptr};
    QLabel* thermal_key_{nullptr};
    QLabel* thermal_value_{nullptr};

    // Panel title labels (for dynamic text updates)
    std::array<QLabel*, 4> panel_title_labels_{};
    bool dragging_{false};
    QPoint drag_origin_{};
    Qt::Edges resize_edge_{};
    QPoint resize_origin_{};
    QRect resize_geometry_{};
};

}  // namespace

int main(int argc, char* argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("Aura");
    app.setApplicationVersion("1.0.0");
    const LaunchConfig config = parse_args(app);
    AuraShellWindow window(config);
    window.show();
    return app.exec();
}
