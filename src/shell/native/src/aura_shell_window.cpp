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
#include "aura_shell/analytics_bridge.hpp"
#include "aura_shell/stylesheet_builder.hpp"
#include "aura_shell/persistence_bridge.hpp"
#include "aura_shell/render_bridge.hpp"
#include "aura_shell/telemetry_bridge.hpp"
#include "aura_shell/timeline_bridge.hpp"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QObject>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScreen>
#include <QSizePolicy>
#include <QSplitter>
#include <QStyle>
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
#ifndef SM_CXPADDEDBORDERWIDTH
#define SM_CXPADDEDBORDERWIDTH 92
#endif
#endif

namespace aura::shell {

static constexpr const char* k_panel_mime = "application/x-aura-panel-id";

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
    // Re-add WS_THICKFRAME+WS_CAPTION for native resize/snap while frameless.
    // Chrome/Electron/framelesshelper all use this pattern.
    apply_win32_frame_style();
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
    controller_config.persistence_enabled = config.persistence_enabled;
    if (config.retention_seconds.has_value()) {
        controller_config.retention_seconds = *config.retention_seconds;
    }
    auto telemetry = std::make_unique<TelemetryBridge>();
    telemetry_bridge_raw_ = telemetry.get();
    controller_ = std::make_unique<CockpitController>(
        std::move(telemetry),
        std::make_unique<RenderBridge>(),
        std::make_unique<TimelineBridge>(),
        std::move(controller_config),
        std::make_unique<PersistenceBridge>(),
        std::make_unique<AnalyticsBridge>()
    );

    auto* root = new QWidget(this);
    root->setObjectName("rootWidget");
    auto* root_layout = new QVBoxLayout(root);
    root_layout->setContentsMargins(6, 6, 6, 6);
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

    // Window-control buttons — Segoe Fluent Icons (Win11 native icon font)
    QFont icon_font(QStringLiteral("Segoe Fluent Icons"), 10);

    auto* min_btn = new QPushButton(titlebar_);
    min_btn->setFont(icon_font);
    min_btn->setText(QChar(0xE921));  // ChromeMinimize

    max_btn_ = new QPushButton(titlebar_);
    max_btn_->setFont(icon_font);
    max_btn_->setText(QChar(0xE922));  // ChromeMaximize

    auto* close_btn = new QPushButton(titlebar_);
    close_btn->setFont(icon_font);
    close_btn->setText(QChar(0xE8BB));  // ChromeClose

    min_btn->setObjectName("minBtn");
    max_btn_->setObjectName("maxBtn");
    close_btn->setObjectName("closeBtn");

    min_btn->setToolTip("Minimize");
    max_btn_->setToolTip("Maximize / Restore");
    close_btn->setToolTip("Close");

    min_btn->setCursor(Qt::ArrowCursor);
    max_btn_->setCursor(Qt::ArrowCursor);
    close_btn->setCursor(Qt::ArrowCursor);

    min_btn->setFocusPolicy(Qt::NoFocus);
    max_btn_->setFocusPolicy(Qt::NoFocus);
    close_btn->setFocusPolicy(Qt::NoFocus);

    connect(min_btn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(max_btn_, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
            max_btn_->setText(QChar(0xE922));  // ChromeMaximize
        } else {
            showMaximized();
            max_btn_->setText(QChar(0xE923));  // ChromeRestore
        }
    });
    connect(close_btn, &QPushButton::clicked, this, &QWidget::close);
    connect(theme_toggle_btn_, &QPushButton::clicked, this, [this]() {
        apply_theme(toggle_ui_theme_mode(current_theme_mode_), true);
    });

    title_layout->addWidget(min_btn);
    title_layout->addWidget(max_btn_);
    title_layout->addWidget(close_btn);

    root_layout->addWidget(titlebar_);

    // ---------------------------------------------------------------
    // Main body — three dock slots with resizable splitter
    // ---------------------------------------------------------------
    body_ = new QWidget(root);
    body_->setAutoFillBackground(false);
    auto* body_layout = new QVBoxLayout(body_);
    body_layout->setContentsMargins(
        current_metrics_.body_margin, current_metrics_.body_margin,
        current_metrics_.body_margin, current_metrics_.body_margin);
    body_layout->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal, body_);
    splitter_->setHandleWidth(qMax(8, current_metrics_.body_spacing));
    splitter_->setChildrenCollapsible(false);

    for (const auto slot : all_dock_slots()) {
        const std::size_t index = slot_index(slot);
        slot_widgets_[index] = build_slot(slot, splitter_);
        connect(slot_widgets_[index].tab_bar, &QTabBar::currentChanged, this, [this, slot](const int tab_index) {
            on_tab_changed(slot, tab_index);
        });
        splitter_->addWidget(slot_widgets_[index].frame);
    }

    // Minimum widths prevent panels from collapsing to zero
    slot_widgets_[0].frame->setMinimumWidth(120);
    slot_widgets_[1].frame->setMinimumWidth(200);
    slot_widgets_[2].frame->setMinimumWidth(120);

    // Set initial sizes proportional to 1:3:1
    const int total_w = width() - 2 * current_metrics_.body_margin;
    splitter_->setSizes({total_w / 5, total_w * 3 / 5, total_w / 5});

    body_layout->addWidget(splitter_, 1);

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

    // Note: titlebar event filter already installed above for drag handling.
    // Window resize is handled natively via WM_NCHITTEST — no Qt-level filter needed.
}

#ifdef _WIN32
void AuraShellWindow::apply_win32_frame_style() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    SetWindowLong(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

bool AuraShellWindow::nativeEvent(const QByteArray& /*eventType*/, void* message, qintptr* result) {
    auto* msg = static_cast<MSG*>(message);

    // Suppress the default non-client area (title bar) added by WS_THICKFRAME.
    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
        *result = 0;
        return true;
    }

    if (msg->message == WM_NCHITTEST) {
        // Let DWM handle first (snap layouts, caption buttons on Win11).
        LRESULT dwm_result = 0;
        if (DwmDefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam, &dwm_result)) {
            *result = static_cast<qintptr>(dwm_result);
            return true;
        }

        if (isMaximized()) {
            *result = HTCLIENT;
            return true;
        }

        // Screen coords from lParam + GetWindowRect — the reliable pattern.
        // ScreenToClient+GetClientRect has coord-space mismatches with DWM
        // shadow on Win11. All production frameless apps use screen coords.
        const POINT pt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        RECT rc;
        GetWindowRect(msg->hwnd, &rc);

        const int border = qMax(6, static_cast<int>(8.0 * devicePixelRatioF()));

        const bool left   = (pt.x >= rc.left   && pt.x < rc.left   + border);
        const bool right  = (pt.x <  rc.right  && pt.x >= rc.right  - border);
        const bool top    = (pt.y >= rc.top    && pt.y < rc.top    + border);
        const bool bottom = (pt.y <  rc.bottom && pt.y >= rc.bottom - border);

        if (top && left)      { *result = HTTOPLEFT;     return true; }
        if (top && right)     { *result = HTTOPRIGHT;    return true; }
        if (bottom && left)   { *result = HTBOTTOMLEFT;  return true; }
        if (bottom && right)  { *result = HTBOTTOMRIGHT; return true; }
        if (left)             { *result = HTLEFT;        return true; }
        if (right)            { *result = HTRIGHT;       return true; }
        if (top)              { *result = HTTOP;         return true; }
        if (bottom)           { *result = HTBOTTOM;      return true; }

        *result = HTCLIENT;
        return true;
    }
    return false;
}

void AuraShellWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    // Qt 6.8+ can reset window styles after construction — re-apply on
    // first show via deferred timer so it survives any Qt style fixup.
    if (!show_style_applied_) {
        show_style_applied_ = true;
        QTimer::singleShot(0, this, [this]() { apply_win32_frame_style(); });
    }
}
#endif

bool AuraShellWindow::eventFilter(QObject* watched, QEvent* event) {
    // ── Cross-zone drag-and-drop on slot frames ──────────────────────────
    if (event->type() == QEvent::DragEnter) {
        auto* de = static_cast<QDragEnterEvent*>(event);
        if (de->mimeData()->hasFormat(QString::fromLatin1(k_panel_mime))) {
            // Highlight target slot frame
            if (auto* frame = qobject_cast<QFrame*>(watched)) {
                frame->setProperty("auraDragOver", true);
                frame->style()->polish(frame);
            }
            de->acceptProposedAction();
            return true;
        }
    }
    if (event->type() == QEvent::DragLeave) {
        // Remove slot highlight
        if (auto* frame = qobject_cast<QFrame*>(watched)) {
            frame->setProperty("auraDragOver", QVariant());
            frame->style()->polish(frame);
        }
    }
    if (event->type() == QEvent::Drop) {
        auto* de = static_cast<QDropEvent*>(event);
        if (de->mimeData()->hasFormat(QString::fromLatin1(k_panel_mime))) {
            const int panel_int = de->mimeData()->data(QString::fromLatin1(k_panel_mime)).toInt();
            const auto panel_id = static_cast<PanelId>(panel_int);

            // Clear drag highlight on drop target
            if (auto* frame = qobject_cast<QFrame*>(watched)) {
                frame->setProperty("auraDragOver", QVariant());
                frame->style()->polish(frame);
            }

            // Find which slot frame received the drop
            for (const auto target_slot : all_dock_slots()) {
                if (slot_widgets_[slot_index(target_slot)].frame == watched) {
                    move_panel_to_slot(panel_id, target_slot);
                    de->acceptProposedAction();
                    return true;
                }
            }
        }
    }

    // Titlebar drag + double-click (resize is handled natively via WM_NCHITTEST)
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
                if (max_btn_) max_btn_->setText(QChar(0xE922));
            } else {
                showMaximized();
                if (max_btn_) max_btn_->setText(QChar(0xE923));
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void AuraShellWindow::mousePressEvent(QMouseEvent* event) {
    QMainWindow::mousePressEvent(event);
}

void AuraShellWindow::mouseMoveEvent(QMouseEvent* event) {
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
        if (body_ != nullptr && body_->layout() != nullptr) {
            body_->layout()->setContentsMargins(
                current_metrics_.body_margin, current_metrics_.body_margin,
                current_metrics_.body_margin, current_metrics_.body_margin);
        }
        if (splitter_ != nullptr) {
            splitter_->setHandleWidth(qMax(8, current_metrics_.body_spacing));
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
    if (process_quick_ != nullptr && process_quick_->rootObject() != nullptr) {
        process_quick_->rootObject()->setProperty(
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
    if (panel_title_labels_[panel_index(PanelId::ProcessPanel)])
        panel_title_labels_[panel_index(PanelId::ProcessPanel)]->setText(
            is_pink_ ? QStringLiteral("\u2728 RUNNING APPS") : QStringLiteral("PROCESS MANAGER"));

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
                case PanelId::ProcessPanel:
                    sw.tab_bar->setTabText(i, is_pink_ ? QStringLiteral("Manager") : QStringLiteral("Manage"));
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

    // Tab bar with cross-zone drag-and-drop support
    auto* tab_bar = new DragTabBar(slot, this, frame);
    tab_bar->setDocumentMode(true);
    tab_bar->setExpanding(false);
    tab_bar->setMovable(true);
    tab_bar->setUsesScrollButtons(true);
    tab_bar->setDrawBase(false);
    tab_bar->setContentsMargins(8, 0, 8, 0);

    // Content stack — padded inside the frame
    auto* stack = new QStackedWidget(frame);
    stack->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(zone_label);
    layout->addWidget(tab_bar);
    layout->addWidget(stack, 1);

    // Enable drop on frame for cross-zone panel moves
    frame->setAcceptDrops(true);
    frame->installEventFilter(this);

    return {frame, tab_bar, stack};
}

// ── DragTabBar implementation ────────────────────────────────────────────────

DragTabBar::DragTabBar(const DockSlot slot, AuraShellWindow* window, QWidget* parent)
    : QTabBar(parent), slot_(slot), window_(window) {}

void DragTabBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        drag_start_ = event->pos();
        drag_tab_index_ = tabAt(event->pos());
    }
    QTabBar::mousePressEvent(event);
}

void DragTabBar::mouseMoveEvent(QMouseEvent* event) {
    if (drag_tab_index_ >= 0
        && (event->buttons() & Qt::LeftButton)
        && (event->pos() - drag_start_).manhattanLength() >= QApplication::startDragDistance() * 2) {

        // Resolve which PanelId is at this tab index
        const auto slot_idx = slot_index(slot_);
        const auto& tabs = window_->dock_state_.slot_tabs[slot_idx];
        if (drag_tab_index_ >= static_cast<int>(tabs.size())) {
            drag_tab_index_ = -1;
            return;
        }
        const auto panel_id = tabs[static_cast<std::size_t>(drag_tab_index_)];

        auto* mime = new QMimeData();
        mime->setData(QString::fromLatin1(k_panel_mime),
                      QByteArray::number(static_cast<int>(panel_id)));

        auto* drag = new QDrag(this);
        drag->setMimeData(mime);

        // Visual ghost preview of the tab being dragged
        const QRect tab_rect = tabRect(drag_tab_index_);
        if (tab_rect.isValid()) {
            QPixmap pix = grab(tab_rect);
            drag->setPixmap(pix.scaledToWidth(qMin(140, pix.width()), Qt::SmoothTransformation));
            drag->setHotSpot(QPoint(20, 16));
        }

        // Bug #17: QDrag::exec() runs a nested event loop that blocks the
        // update_timer_ for the entire drag.  Stop/restart around exec() so
        // a long drag doesn't queue stale ticks or corrupt controller state.
        if (window_->update_timer_ != nullptr) {
            window_->update_timer_->stop();
        }
        drag->exec(Qt::MoveAction);
        if (window_->update_timer_ != nullptr) {
            window_->update_timer_->start();
        }
        drag_tab_index_ = -1;
        return;
    }
    QTabBar::mouseMoveEvent(event);
}

}  // namespace aura::shell
