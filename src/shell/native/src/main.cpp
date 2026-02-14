#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QObject>
#include <QPushButton>
#include <QQuickWidget>
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

namespace {

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

int interval_to_milliseconds(const double interval_seconds) {
    if (!std::isfinite(interval_seconds) || interval_seconds <= 0.0) {
        return 1000;
    }
    return std::max(100, static_cast<int>(std::llround(interval_seconds * 1000.0)));
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

class AuraShellWindow final : public QMainWindow {
public:
    explicit AuraShellWindow(const LaunchConfig& config, QWidget* parent = nullptr)
        : QMainWindow(parent),
          config_(config) {
        setWindowTitle("Aura | Native Shell");
        setMinimumSize(1100, 640);
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowMinMaxButtonsHint);
        setStyleSheet(
            "QMainWindow{background:#091523;color:#d8ebff;}"
            "QFrame#titlebar{background:#0e2237;border-bottom:1px solid #2f5e8b;}"
            "QFrame#slot{background:#10263d;border:1px solid #355d84;border-radius:8px;}"
            "QPushButton{background:#143251;color:#d8ebff;border:1px solid #3e6f9c;border-radius:4px;padding:4px 8px;}"
            "QLabel#status{color:#9cc7ea;}"
        );

        aura::shell::CockpitController::Config controller_config;
        controller_config.poll_interval_seconds = config.interval_seconds;
        controller_config.max_process_rows = process_labels_.size();
        if (config.db_path.has_value()) {
            controller_config.db_path = config.db_path->toStdString();
        }
        controller_ = std::make_unique<aura::shell::CockpitController>(
            std::make_unique<aura::shell::TelemetryBridge>(),
            std::make_unique<aura::shell::RenderBridge>(),
            std::move(controller_config)
        );

        auto* root = new QWidget(this);
        auto* root_layout = new QVBoxLayout(root);
        root_layout->setContentsMargins(0, 0, 0, 0);
        root_layout->setSpacing(0);

        titlebar_ = new QFrame(root);
        titlebar_->setObjectName("titlebar");
        titlebar_->installEventFilter(this);

        auto* title_layout = new QHBoxLayout(titlebar_);
        title_layout->setContentsMargins(10, 4, 4, 4);
        title_layout->setSpacing(6);
        title_layout->addWidget(new QLabel("Aura Native Cockpit", titlebar_));
        title_layout->addStretch(1);

        auto* min_btn = new QPushButton("_", titlebar_);
        auto* max_btn = new QPushButton("[]", titlebar_);
        auto* close_btn = new QPushButton("X", titlebar_);
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
        title_layout->addWidget(max_btn);
        title_layout->addWidget(close_btn);
        root_layout->addWidget(titlebar_);

        auto* body = new QWidget(root);
        auto* body_layout = new QHBoxLayout(body);
        body_layout->setContentsMargins(12, 12, 12, 12);
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

        footer_status_ = new QLabel(
            QString("interval=%1  persist=%2  db=%3")
                .arg(config.interval_seconds)
                .arg(config.persistence_enabled ? "on" : "off")
                .arg(config.db_path.value_or("<none>")),
            root
        );
        footer_status_->setObjectName("status");
        footer_status_->setContentsMargins(12, 0, 12, 12);
        root_layout->addWidget(footer_status_);

        setCentralWidget(root);

        update_timer_ = new QTimer(this);
        update_timer_->setInterval(interval_to_milliseconds(config_.interval_seconds));
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
    SlotWidgets build_slot(const aura::shell::DockSlot slot, QWidget* parent) {
        auto* frame = new QFrame(parent);
        frame->setObjectName("slot");
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto* title_label = new QLabel(slot_title(slot), frame);
        auto* tab_bar = new QTabBar(frame);
        tab_bar->setDocumentMode(true);
        tab_bar->setExpanding(true);
        tab_bar->setMovable(false);
        tab_bar->setUsesScrollButtons(true);
        auto* stack = new QStackedWidget(frame);
        layout->addWidget(title_label);
        layout->addWidget(tab_bar);
        layout->addWidget(stack, 1);
        return {
            frame,
            tab_bar,
            stack,
        };
    }

    QVBoxLayout* create_panel_page(const aura::shell::PanelId panel_id, const QString& title) {
        auto* page = new QWidget(this);
        panel_pages_[panel_index(panel_id)] = page;

        auto* page_layout = new QVBoxLayout(page);
        page_layout->setContentsMargins(0, 0, 0, 0);
        page_layout->setSpacing(8);

        auto* header_layout = new QHBoxLayout();
        header_layout->setContentsMargins(0, 0, 0, 0);
        header_layout->setSpacing(6);
        header_layout->addWidget(new QLabel(title, page));
        header_layout->addStretch(1);
        header_layout->addWidget(new QLabel("Move", page));

        for (const auto slot : aura::shell::all_dock_slots()) {
            auto* move_button = new QPushButton(slot_short_label(slot), page);
            connect(move_button, &QPushButton::clicked, this, [this, panel_id, slot]() {
                move_panel_to_slot(panel_id, slot);
            });
            panel_move_buttons_[panel_index(panel_id)][slot_index(slot)] = move_button;
            header_layout->addWidget(move_button);
        }

        page_layout->addLayout(header_layout);

        auto* content_layout = new QVBoxLayout();
        content_layout->setContentsMargins(0, 0, 0, 0);
        content_layout->setSpacing(6);
        page_layout->addLayout(content_layout, 1);
        return content_layout;
    }

    void build_panel_pages() {
        using aura::shell::PanelId;

        {
            auto* telemetry_layout =
                create_panel_page(PanelId::TelemetryOverview, QStringLiteral("Telemetry Overview"));
            telemetry_cpu_ = new QLabel("CPU --", panel_pages_[panel_index(PanelId::TelemetryOverview)]);
            telemetry_memory_ = new QLabel("Memory --", panel_pages_[panel_index(PanelId::TelemetryOverview)]);
            telemetry_timestamp_ = new QLabel("Timestamp --", panel_pages_[panel_index(PanelId::TelemetryOverview)]);
            telemetry_status_ =
                new QLabel("Awaiting telemetry snapshot...", panel_pages_[panel_index(PanelId::TelemetryOverview)]);
            telemetry_status_->setWordWrap(true);

            telemetry_layout->addWidget(telemetry_cpu_);
            telemetry_layout->addWidget(telemetry_memory_);
            telemetry_layout->addWidget(telemetry_timestamp_);
            telemetry_layout->addWidget(telemetry_status_);
            telemetry_layout->addStretch(1);
        }

        {
            auto* processes_layout = create_panel_page(PanelId::TopProcesses, QStringLiteral("Top Processes"));
            process_status_ = new QLabel("Waiting for process samples...", panel_pages_[panel_index(PanelId::TopProcesses)]);
            process_status_->setWordWrap(true);
            processes_layout->addWidget(process_status_);
            for (QLabel*& process_label : process_labels_) {
                process_label = new QLabel("-", panel_pages_[panel_index(PanelId::TopProcesses)]);
                process_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
                processes_layout->addWidget(process_label);
            }
            processes_layout->addStretch(1);
        }

        {
            auto* timeline_layout = create_panel_page(PanelId::DvrTimeline, QStringLiteral("DVR Timeline"));
            timeline_status_ = new QLabel(
                "Timeline panel ready. Runtime timeline query hookup remains session-local placeholder in this native shell window.",
                panel_pages_[panel_index(PanelId::DvrTimeline)]
            );
            timeline_status_->setWordWrap(true);
            timeline_layout->addWidget(timeline_status_);
            timeline_layout->addStretch(1);
        }

        {
            auto* render_layout = create_panel_page(PanelId::RenderSurface, QStringLiteral("Render Surface"));
            quick_ = new QQuickWidget(panel_pages_[panel_index(PanelId::RenderSurface)]);
            quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
            quick_->setSource(QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_QML_PATH)));
            quick_->setMinimumHeight(340);
            render_status_ = new QLabel("Cockpit scene online", panel_pages_[panel_index(PanelId::RenderSurface)]);
            render_status_->setWordWrap(true);
            render_layout->addWidget(quick_, 1);
            render_layout->addWidget(render_status_);
        }
    }

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
            auto& slot_widgets = slot_widgets_[slot_idx];

            slot_widgets.tab_bar->clear();
            while (slot_widgets.stack->count() > 0) {
                slot_widgets.stack->removeWidget(slot_widgets.stack->widget(0));
            }
        }

        for (const auto slot : aura::shell::all_dock_slots()) {
            const std::size_t slot_idx = slot_index(slot);
            auto& slot_widgets = slot_widgets_[slot_idx];

            const auto& tabs = dock_state_.slot_tabs[slot_idx];
            for (const auto panel_id : tabs) {
                slot_widgets.tab_bar->addTab(panel_title(panel_id));
                QWidget* page = panel_pages_[panel_index(panel_id)];
                if (page != nullptr) {
                    slot_widgets.stack->addWidget(page);
                }
            }

            if (tabs.empty()) {
                slot_widgets.tab_bar->setCurrentIndex(-1);
                slot_widgets.stack->setCurrentIndex(-1);
                continue;
            }

            const int active_tab = static_cast<int>(std::min(
                dock_state_.active_tab[slot_idx],
                tabs.size() - 1U
            ));
            slot_widgets.tab_bar->setCurrentIndex(active_tab);
            slot_widgets.stack->setCurrentIndex(active_tab);
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

    void refresh_cockpit() {
        if (!controller_) {
            return;
        }
        const auto state = controller_->tick(config_.interval_seconds);
        telemetry_cpu_->setText(QString::fromStdString(state.cpu_line));
        telemetry_memory_->setText(QString::fromStdString(state.memory_line));
        telemetry_timestamp_->setText(QString::fromStdString(state.timestamp_line));
        telemetry_status_->setText(QString::fromStdString(state.status_line));
        process_status_->setText(QString::fromStdString(state.status_line));
        render_status_->setText(QString::fromStdString(state.status_line));
        timeline_status_->setText(
            QString("Timeline panel ready. Live runtime timeline feed pending in this shell pass. status=%1")
                .arg(QString::fromStdString(state.status_line))
        );

        for (std::size_t i = 0; i < process_labels_.size(); ++i) {
            const QString line = i < state.process_rows.size()
                                     ? QString::fromStdString(state.process_rows[i])
                                     : QStringLiteral("-");
            process_labels_[i]->setText(line);
        }

        footer_status_->setText(
            QString("interval=%1  persist=%2  db=%3  telemetry=%4  render=%5")
                .arg(config_.interval_seconds)
                .arg(config_.persistence_enabled ? "on" : "off")
                .arg(config_.db_path.value_or("<none>"))
                .arg(state.telemetry_available ? "ok" : "degraded")
                .arg(state.render_available ? "ok" : "fallback")
        );

        if (quick_ != nullptr && quick_->rootObject() != nullptr) {
            QObject* root = quick_->rootObject();
            root->setProperty("accentIntensity", state.accent_intensity);
            root->setProperty("cpuPercent", state.cpu_percent);
            root->setProperty("memoryPercent", state.memory_percent);
            root->setProperty("statusText", QString::fromStdString(state.status_line));
        }
    }

    LaunchConfig config_{};
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
    const LaunchConfig config = parse_args(app);
    AuraShellWindow window(config);
    window.show();
    return app.exec();
}
