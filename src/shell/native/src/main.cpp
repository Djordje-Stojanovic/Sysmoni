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
#include <QObject>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScrollArea>
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

namespace {

// ---------------------------------------------------------------------------
// Color palette — all depth levels and accent colors in one place so the
// stylesheet strings below stay readable and consistent.
// ---------------------------------------------------------------------------
//  Depth 0  (window bg)  : #060b14
//  Depth 1  (panels)     : #0a1221
//  Depth 2  (surfaces)   : #0f1a2e
//  Depth 3  (elevated)   : #162238
//  Border subtle         : #1e3350
//  Border active         : #2a4a6e
//  Text primary          : #e0ecf7
//  Text secondary        : #8badc4
//  Text muted            : #4d6d87
//  Accent blue           : #3b82f6
//  Accent blue hover     : #60a5fa
//  Danger (close)        : #ef4444
//  Danger hover          : #f87171

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

int interval_to_milliseconds(const double interval_seconds) {
    if (!std::isfinite(interval_seconds) || interval_seconds <= 0.0) {
        return 1000;
    }
    return std::clamp(static_cast<int>(std::llround(interval_seconds * 1000.0)), 16, 2000);
}

int clamp_timer_interval_ms(const int value) {
    return std::clamp(value, 16, 2000);
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

// ---------------------------------------------------------------------------
// Comprehensive application stylesheet
// ---------------------------------------------------------------------------
static const QString k_app_stylesheet = QStringLiteral(
    // --- Application-wide base ---
    "* {"
    "    font-family: 'Segoe UI';"
    "    font-size: 11px;"
    "    color: #e0ecf7;"
    "}"

    // --- Main window ---
    "QMainWindow {"
    "    background: #060b14;"
    "}"

    // --- Title bar ---
    "QFrame#titlebar {"
    "    background: qlineargradient("
    "        x1:0, y1:0, x2:0, y2:1,"
    "        stop:0 #0f1a2e,"
    "        stop:1 #0a1221"
    "    );"
    "    border-bottom: 1px solid #1e3350;"
    "    min-height: 36px;"
    "    max-height: 36px;"
    "}"

    // --- App logo label in title bar ---
    "QLabel#appLogo {"
    "    color: #3b82f6;"
    "    font-size: 13px;"
    "    font-weight: 600;"
    "    letter-spacing: 2px;"
    "    padding-left: 4px;"
    "}"

    // --- Title bar subtitle ---
    "QLabel#titleSubtitle {"
    "    color: #4d6d87;"
    "    font-size: 10px;"
    "    letter-spacing: 1px;"
    "    padding-left: 8px;"
    "}"

    // --- Title bar window-control buttons ---
    "QPushButton#minBtn, QPushButton#maxBtn {"
    "    background: transparent;"
    "    color: #8badc4;"
    "    border: none;"
    "    border-radius: 4px;"
    "    min-width: 28px;"
    "    max-width: 28px;"
    "    min-height: 24px;"
    "    max-height: 24px;"
    "    font-size: 13px;"
    "    padding: 0px;"
    "}"
    "QPushButton#minBtn:hover {"
    "    background: #162238;"
    "    color: #e0ecf7;"
    "}"
    "QPushButton#maxBtn:hover {"
    "    background: #162238;"
    "    color: #e0ecf7;"
    "}"
    "QPushButton#closeBtn {"
    "    background: transparent;"
    "    color: #8badc4;"
    "    border: none;"
    "    border-radius: 4px;"
    "    min-width: 28px;"
    "    max-width: 28px;"
    "    min-height: 24px;"
    "    max-height: 24px;"
    "    font-size: 13px;"
    "    padding: 0px;"
    "    margin-right: 4px;"
    "}"
    "QPushButton#closeBtn:hover {"
    "    background: #ef4444;"
    "    color: #ffffff;"
    "}"
    "QPushButton#closeBtn:pressed {"
    "    background: #b91c1c;"
    "    color: #ffffff;"
    "}"

    // --- Dock slot frames ---
    "QFrame#slot {"
    "    background: #0a1221;"
    "    border: 1px solid #1e3350;"
    "    border-radius: 10px;"
    "    padding: 0px;"
    "}"

    // --- Slot zone label (LEFT / CENTER / RIGHT) ---
    "QLabel#slotZoneLabel {"
    "    color: #2a4a6e;"
    "    font-size: 9px;"
    "    font-weight: 600;"
    "    letter-spacing: 3px;"
    "    padding: 0px 0px 0px 2px;"
    "}"

    // --- Tab bar ---
    "QTabBar {"
    "    background: transparent;"
    "    border: none;"
    "    border-bottom: 1px solid #1e3350;"
    "}"
    "QTabBar::tab {"
    "    background: transparent;"
    "    color: #4d6d87;"
    "    border: none;"
    "    border-bottom: 2px solid transparent;"
    "    padding: 7px 16px 6px 16px;"
    "    font-size: 11px;"
    "    font-weight: 500;"
    "    letter-spacing: 0.5px;"
    "    min-width: 60px;"
    "}"
    "QTabBar::tab:hover {"
    "    color: #8badc4;"
    "    border-bottom: 2px solid #2a4a6e;"
    "}"
    "QTabBar::tab:selected {"
    "    color: #e0ecf7;"
    "    border-bottom: 2px solid #3b82f6;"
    "    font-weight: 600;"
    "}"
    "QTabBar::tab:!enabled {"
    "    color: #2a4a6e;"
    "}"

    // --- Generic QPushButton (move buttons etc.) ---
    "QPushButton {"
    "    background: #0f1a2e;"
    "    color: #8badc4;"
    "    border: 1px solid #1e3350;"
    "    border-radius: 6px;"
    "    padding: 3px 10px;"
    "    font-size: 11px;"
    "}"
    "QPushButton:hover {"
    "    background: #162238;"
    "    color: #e0ecf7;"
    "    border-color: #2a4a6e;"
    "}"
    "QPushButton:pressed {"
    "    background: #1e3350;"
    "    color: #60a5fa;"
    "    border-color: #3b82f6;"
    "}"
    "QPushButton:disabled {"
    "    background: #060b14;"
    "    color: #2a4a6e;"
    "    border-color: #1e3350;"
    "}"

    // --- Panel move pill buttons ---
    "QPushButton#moveBtn {"
    "    background: #0f1a2e;"
    "    color: #4d6d87;"
    "    border: 1px solid #1e3350;"
    "    border-radius: 10px;"
    "    padding: 2px 9px;"
    "    font-size: 10px;"
    "    font-weight: 600;"
    "    min-width: 22px;"
    "    max-height: 20px;"
    "}"
    "QPushButton#moveBtn:hover {"
    "    background: #162238;"
    "    color: #60a5fa;"
    "    border-color: #3b82f6;"
    "}"
    "QPushButton#moveBtn:disabled {"
    "    background: #060b14;"
    "    color: #1e3350;"
    "    border-color: #0f1a2e;"
    "}"

    // --- Panel header separator line ---
    "QFrame#panelHeaderLine {"
    "    background: qlineargradient("
    "        x1:0, y1:0, x2:1, y2:0,"
    "        stop:0 #3b82f6,"
    "        stop:0.4 #1e3350,"
    "        stop:1 transparent"
    "    );"
    "    min-height: 1px;"
    "    max-height: 1px;"
    "    border: none;"
    "}"

    // --- Panel title label ---
    "QLabel#panelTitle {"
    "    color: #e0ecf7;"
    "    font-size: 12px;"
    "    font-weight: 600;"
    "    letter-spacing: 1px;"
    "}"

    // --- "Move to:" prompt label ---
    "QLabel#moveToLabel {"
    "    color: #4d6d87;"
    "    font-size: 9px;"
    "    letter-spacing: 1px;"
    "}"

    // --- Telemetry metric labels ---
    "QLabel#metricValue {"
    "    color: #e0ecf7;"
    "    font-size: 20px;"
    "    font-weight: 700;"
    "    letter-spacing: -0.5px;"
    "}"
    "QLabel#metricKey {"
    "    color: #4d6d87;"
    "    font-size: 9px;"
    "    font-weight: 600;"
    "    letter-spacing: 2px;"
    "}"
    "QLabel#metricUnit {"
    "    color: #3b82f6;"
    "    font-size: 12px;"
    "    font-weight: 600;"
    "}"

    // --- Telemetry timestamp ---
    "QLabel#telemetryTimestamp {"
    "    color: #4d6d87;"
    "    font-size: 10px;"
    "}"

    // --- Telemetry status ---
    "QLabel#telemetryStatus {"
    "    color: #8badc4;"
    "    font-size: 10px;"
    "    font-style: italic;"
    "}"

    // --- Process panel status label ---
    "QLabel#processStatus {"
    "    color: #4d6d87;"
    "    font-size: 10px;"
    "    font-style: italic;"
    "    padding-bottom: 4px;"
    "}"

    // --- Process row labels (monospace) ---
    "QLabel#processRow {"
    "    color: #8badc4;"
    "    font-family: 'Cascadia Mono', 'Consolas', monospace;"
    "    font-size: 10px;"
    "    padding: 3px 6px;"
    "    border-radius: 3px;"
    "}"
    "QLabel#processRowAlt {"
    "    color: #8badc4;"
    "    font-family: 'Cascadia Mono', 'Consolas', monospace;"
    "    font-size: 10px;"
    "    background: #0a1221;"
    "    padding: 3px 6px;"
    "    border-radius: 3px;"
    "}"

    // --- Timeline and render status labels ---
    "QLabel#timelineStatus {"
    "    color: #4d6d87;"
    "    font-size: 10px;"
    "    font-style: italic;"
    "}"
    "QLabel#renderStatus {"
    "    color: #4d6d87;"
    "    font-size: 10px;"
    "    font-style: italic;"
    "}"

    // --- Footer status bar ---
    "QLabel#footerStatus {"
    "    color: #2a4a6e;"
    "    font-size: 9px;"
    "    font-family: 'Cascadia Mono', 'Consolas', monospace;"
    "    padding: 4px 12px 8px 14px;"
    "    border-left: 2px solid #3b82f6;"
    "    margin-left: 12px;"
    "    margin-bottom: 4px;"
    "}"

    // --- Thin scrollbars ---
    "QScrollBar:vertical {"
    "    background: #060b14;"
    "    width: 6px;"
    "    border-radius: 3px;"
    "    margin: 0px;"
    "}"
    "QScrollBar::handle:vertical {"
    "    background: #1e3350;"
    "    border-radius: 3px;"
    "    min-height: 20px;"
    "}"
    "QScrollBar::handle:vertical:hover {"
    "    background: #2a4a6e;"
    "}"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
    "    height: 0px;"
    "}"
    "QScrollBar:horizontal {"
    "    background: #060b14;"
    "    height: 6px;"
    "    border-radius: 3px;"
    "    margin: 0px;"
    "}"
    "QScrollBar::handle:horizontal {"
    "    background: #1e3350;"
    "    border-radius: 3px;"
    "    min-width: 20px;"
    "}"
    "QScrollBar::handle:horizontal:hover {"
    "    background: #2a4a6e;"
    "}"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
    "    width: 0px;"
    "}"
);

// ---------------------------------------------------------------------------
// AuraShellWindow
// ---------------------------------------------------------------------------

class AuraShellWindow final : public QMainWindow {
public:
    explicit AuraShellWindow(const LaunchConfig& config, QWidget* parent = nullptr)
        : QMainWindow(parent),
          config_(config) {
        setWindowTitle("Aura | Native Shell");
        setMinimumSize(1100, 640);
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowMinMaxButtonsHint);
        setStyleSheet(k_app_stylesheet);
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

        title_layout->addWidget(min_btn);
        title_layout->addSpacing(2);
        title_layout->addWidget(max_btn);
        title_layout->addSpacing(2);
        title_layout->addWidget(close_btn);

        root_layout->addWidget(titlebar_);

        // ---------------------------------------------------------------
        // Main body — three dock slots
        // ---------------------------------------------------------------
        auto* body = new QWidget(root);
        body->setAutoFillBackground(false);
        auto* body_layout = new QHBoxLayout(body);
        body_layout->setContentsMargins(12, 12, 12, 8);
        body_layout->setSpacing(10);

        for (const auto slot : aura::shell::all_dock_slots()) {
            const std::size_t index = slot_index(slot);
            slot_widgets_[index] = build_slot(slot, body);
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
        root_layout->addWidget(body, 1);

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

private:
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

            auto* cpu_key = new QLabel("CPU LOAD", parent_page);
            cpu_key->setObjectName("metricKey");
            telemetry_cpu_ = new QLabel("CPU --", parent_page);
            telemetry_cpu_->setObjectName("metricValue");

            cpu_block_layout->addWidget(cpu_key);
            cpu_block_layout->addWidget(telemetry_cpu_);
            telemetry_layout->addWidget(cpu_block);

            // Memory metric block
            auto* mem_block = new QWidget(parent_page);
            auto* mem_block_layout = new QVBoxLayout(mem_block);
            mem_block_layout->setContentsMargins(0, 0, 0, 8);
            mem_block_layout->setSpacing(1);

            auto* mem_key = new QLabel("MEMORY USE", parent_page);
            mem_key->setObjectName("metricKey");
            telemetry_memory_ = new QLabel("Memory --", parent_page);
            telemetry_memory_->setObjectName("metricValue");

            mem_block_layout->addWidget(mem_key);
            mem_block_layout->addWidget(telemetry_memory_);
            telemetry_layout->addWidget(mem_block);

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
            quick_->setMinimumHeight(340);

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
                sw.tab_bar->addTab(panel_title(panel_id));
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
            QString("interval=%1s  persist=%2  db=%3  telemetry=%4  render=%5  timeline=%6  style=%7  sev=%8  quality=%9  fps=%10  anomalies=%11")
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
                .arg(state.timeline_anomaly_count);
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
        }

        if (update_timer_ != nullptr) {
            const int recommended_interval = clamp_timer_interval_ms(state.fps_recommended_delay_ms);
            if (update_timer_->interval() != recommended_interval) {
                update_timer_->setInterval(recommended_interval);
            }
            current_interval_seconds_ = static_cast<double>(recommended_interval) / 1000.0;
        }
    }

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------
    LaunchConfig config_{};
    double current_interval_seconds_{1.0};
    std::unique_ptr<aura::shell::CockpitController> controller_;
    aura::shell::DockState dock_state_{aura::shell::build_default_dock_state()};
    std::array<SlotWidgets, 3> slot_widgets_{};
    std::array<QWidget*, 4> panel_pages_{};
    std::array<std::array<QPushButton*, 3>, 4> panel_move_buttons_{};
    bool syncing_tabs_{false};
    QFrame* titlebar_{nullptr};
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
    bool dragging_{false};
    QPoint drag_origin_{};
};

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Aura");
    app.setApplicationVersion("1.0.0");
    const LaunchConfig config = parse_args(app);
    AuraShellWindow window(config);
    window.show();
    return app.exec();
}
