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

#include "aura_shell/aura_shell_window.hpp"
#include "aura_shell/stylesheet_builder.hpp"
#include "aura_shell/render_bridge.hpp"
#include "aura_shell/telemetry_bridge.hpp"
#include "aura_shell/timeline_bridge.hpp"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QObject>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>
#include <Qt>

#include <cmath>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace aura::shell {

AuraShellWindow::AuraShellWindow(const LaunchConfig& config, QWidget* parent)
    : QMainWindow(parent),
      config_(config) {
    current_theme_mode_ = ui_theme_mode_from_key(
        settings_.value(k_theme_mode_setting_key, QStringLiteral("dark_blue"))
            .toString()
            .toStdString()
    );
    setWindowTitle("Aura | Native Shell");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowMinMaxButtonsHint);
    setMouseTracking(true);

#ifdef _WIN32
    // Re-add WS_THICKFRAME for native resize borders while keeping frameless look.
    // This is the standard approach (Chrome, VS Code, etc.).
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    SetWindowLong(hwnd, GWL_STYLE, style);

    // Extend DWM frame 1px into client area for proper compositing
    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#endif

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

    CockpitController::Config controller_config;
    controller_config.poll_interval_seconds = current_interval_seconds_;
    controller_config.max_process_rows = process_labels_.size();
    if (config.db_path.has_value()) {
        controller_config.db_path = config.db_path->toStdString();
    }
    controller_ = std::make_unique<CockpitController>(
        std::make_unique<TelemetryBridge>(),
        std::make_unique<RenderBridge>(),
        std::make_unique<TimelineBridge>(),
        std::move(controller_config)
    );

    auto* root = new QWidget(this);
    root->setObjectName("rootWidget");
    auto* root_layout = new QVBoxLayout(root);
    root_layout->setContentsMargins(3, 3, 3, 3);
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
        apply_theme(toggle_ui_theme_mode(current_theme_mode_), true);
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

    for (const auto slot : all_dock_slots()) {
        const std::size_t index = slot_index(slot);
        slot_widgets_[index] = build_slot(slot, body_);
        connect(slot_widgets_[index].tab_bar, &QTabBar::currentChanged, this, [this, slot](const int tab_index) {
            on_tab_changed(slot, tab_index);
        });
        body_layout->addWidget(
            slot_widgets_[index].frame,
            slot == DockSlot::Center ? 3 : 1
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

    // Install app-level event filter so edge resize works even over child widgets
    qApp->installEventFilter(this);
}

#ifdef _WIN32
bool AuraShellWindow::nativeEvent(const QByteArray& /*eventType*/, void* message, qintptr* result) {
    auto* msg = static_cast<MSG*>(message);

    // Suppress the default non-client area (title bar) added by WS_THICKFRAME.
    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
        *result = 0;
        return true;
    }

    if (msg->message == WM_NCHITTEST && !isMaximized()) {
        const LONG border = static_cast<LONG>(kResizeBorder);
        RECT rc;
        GetWindowRect(reinterpret_cast<HWND>(winId()), &rc);
        const LONG x = GET_X_LPARAM(msg->lParam);
        const LONG y = GET_Y_LPARAM(msg->lParam);

        const bool left   = (x < rc.left + border);
        const bool right  = (x >= rc.right - border);
        const bool top    = (y < rc.top + border);
        const bool bottom = (y >= rc.bottom - border);

        if (top && left)      { *result = HTTOPLEFT;     return true; }
        if (top && right)     { *result = HTTOPRIGHT;    return true; }
        if (bottom && left)   { *result = HTBOTTOMLEFT;  return true; }
        if (bottom && right)  { *result = HTBOTTOMRIGHT; return true; }
        if (left)             { *result = HTLEFT;        return true; }
        if (right)            { *result = HTRIGHT;       return true; }
        if (top)              { *result = HTTOP;         return true; }
        if (bottom)           { *result = HTBOTTOM;      return true; }
    }
    return false;
}
#endif

bool AuraShellWindow::eventFilter(QObject* watched, QEvent* event) {
    // Intercept mouse events from ANY child widget near window edges for resize
    if (!isMaximized() && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const QPoint win_pos = mapFromGlobal(me->globalPosition().toPoint());
            const Qt::Edges edges = hit_test_edge(win_pos);
            if (edges != Qt::Edges{} && windowHandle()) {
                windowHandle()->startSystemResize(edges);
                return true;
            }
        }
    }
    if (!isMaximized() && event->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint win_pos = mapFromGlobal(me->globalPosition().toPoint());
        const Qt::Edges edge = hit_test_edge(win_pos);
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

    // Titlebar drag + double-click
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

void AuraShellWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !isMaximized()) {
        const Qt::Edges edges = hit_test_edge(event->pos());
        if (edges != Qt::Edges{}) {
            // Use native OS resize — works even when child widgets eat events
            if (windowHandle()) {
                windowHandle()->startSystemResize(edges);
            }
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void AuraShellWindow::mouseMoveEvent(QMouseEvent* event) {
    // Update cursor shape on hover near edges
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

void AuraShellWindow::mouseReleaseEvent(QMouseEvent* event) {
    QMainWindow::mouseReleaseEvent(event);
}

void AuraShellWindow::resizeEvent(QResizeEvent* event) {
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
        for (const auto slot : all_dock_slots()) {
            auto& sw = slot_widgets_[slot_index(slot)];
            if (sw.frame != nullptr && sw.frame->layout() != nullptr) {
                const int top_pad = current_metrics_.body_margin > 8 ? 8 : 4;
                const int bot_pad = current_metrics_.body_margin > 8 ? 10 : 6;
                sw.frame->layout()->setContentsMargins(0, top_pad, 0, bot_pad);
            }
        }
    }
}

Qt::Edges AuraShellWindow::hit_test_edge(const QPoint& pos) const {
    Qt::Edges edges;
    if (pos.x() < kResizeBorder)                edges |= Qt::LeftEdge;
    if (pos.x() >= width() - kResizeBorder)     edges |= Qt::RightEdge;
    if (pos.y() < kResizeBorder)                edges |= Qt::TopEdge;
    if (pos.y() >= height() - kResizeBorder)    edges |= Qt::BottomEdge;
    return edges;
}

void AuraShellWindow::sync_theme_to_qml() {
    if (quick_ != nullptr && quick_->rootObject() != nullptr) {
        quick_->rootObject()->setProperty(
            "themeMode",
            QString::fromStdString(ui_theme_mode_key(current_theme_mode_))
        );
    }
    if (timeline_quick_ != nullptr && timeline_quick_->rootObject() != nullptr) {
        timeline_quick_->rootObject()->setProperty(
            "themeMode",
            QString::fromStdString(ui_theme_mode_key(current_theme_mode_))
        );
    }
}

void AuraShellWindow::apply_theme(const UiThemeMode mode, const bool persist) {
    current_theme_mode_ = mode;
    is_pink_ = (mode == UiThemeMode::PinkCute);
    theme_dirty_ = true;
    setStyleSheet(build_app_stylesheet(current_theme_mode_, current_metrics_));
    if (theme_toggle_btn_ != nullptr) {
        theme_toggle_btn_->setText(QString::fromStdString(ui_theme_mode_label(mode)));
    }
    if (persist) {
        settings_.setValue(
            k_theme_mode_setting_key,
            QString::fromStdString(ui_theme_mode_key(current_theme_mode_))
        );
    }

    // Update panel title labels
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
    for (const auto slot : all_dock_slots()) {
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

SlotWidgets AuraShellWindow::build_slot(const DockSlot slot, QWidget* parent) {
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

}  // namespace aura::shell
