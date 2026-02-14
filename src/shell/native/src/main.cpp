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
    QVBoxLayout* layout;
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

        const auto left_slot = build_slot("LEFT");
        left_cpu_ = new QLabel("CPU --", left_slot.frame);
        left_memory_ = new QLabel("Memory --", left_slot.frame);
        left_timestamp_ = new QLabel("Timestamp --", left_slot.frame);
        left_slot.layout->addWidget(left_cpu_);
        left_slot.layout->addWidget(left_memory_);
        left_slot.layout->addWidget(left_timestamp_);
        left_slot.layout->addStretch(1);

        const auto center_slot = build_slot("CENTER");
        center_status_ = new QLabel("Cockpit scene online", center_slot.frame);
        center_status_->setWordWrap(true);

        quick_ = new QQuickWidget(center_slot.frame);
        quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
        quick_->setSource(QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_QML_PATH)));
        quick_->setMinimumHeight(340);
        center_slot.layout->addWidget(quick_);
        center_slot.layout->addWidget(center_status_);

        const auto right_slot = build_slot("RIGHT");
        right_status_ = new QLabel("Initializing bridges...", right_slot.frame);
        right_status_->setWordWrap(true);
        right_slot.layout->addWidget(right_status_);
        for (QLabel*& process_label : process_labels_) {
            process_label = new QLabel("-", right_slot.frame);
            process_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            right_slot.layout->addWidget(process_label);
        }
        right_slot.layout->addStretch(1);

        body_layout->addWidget(left_slot.frame, 1);
        body_layout->addWidget(center_slot.frame, 2);
        body_layout->addWidget(right_slot.frame, 1);
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
    static SlotWidgets build_slot(const QString& title) {
        auto* frame = new QFrame();
        frame->setObjectName("slot");
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto* title_label = new QLabel(title, frame);

        layout->addWidget(title_label);
        return {frame, layout};
    }

    void refresh_cockpit() {
        if (!controller_) {
            return;
        }
        const auto state = controller_->tick(config_.interval_seconds);
        left_cpu_->setText(QString::fromStdString(state.cpu_line));
        left_memory_->setText(QString::fromStdString(state.memory_line));
        left_timestamp_->setText(QString::fromStdString(state.timestamp_line));
        center_status_->setText(QString::fromStdString(state.status_line));
        right_status_->setText(QString::fromStdString(state.status_line));

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
    QFrame* titlebar_{nullptr};
    QQuickWidget* quick_{nullptr};
    QTimer* update_timer_{nullptr};
    QLabel* left_cpu_{nullptr};
    QLabel* left_memory_{nullptr};
    QLabel* left_timestamp_{nullptr};
    QLabel* center_status_{nullptr};
    QLabel* right_status_{nullptr};
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
