#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QQuickWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <optional>

#include "aura_shell/dock_model.hpp"

namespace {

struct LaunchConfig {
    double interval_seconds{1.0};
    bool persistence_enabled{true};
    std::optional<QString> db_path;
    std::optional<double> retention_seconds;
};

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

class AuraShellWindow final : public QMainWindow {
public:
    explicit AuraShellWindow(const LaunchConfig& config, QWidget* parent = nullptr)
        : QMainWindow(parent) {
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

        auto* root = new QWidget(this);
        auto* root_layout = new QVBoxLayout(root);
        root_layout->setContentsMargins(0, 0, 0, 0);
        root_layout->setSpacing(0);

        auto* titlebar = new QFrame(root);
        titlebar->setObjectName("titlebar");
        auto* title_layout = new QHBoxLayout(titlebar);
        title_layout->setContentsMargins(10, 4, 4, 4);
        title_layout->setSpacing(6);
        title_layout->addWidget(new QLabel("Aura Native Cockpit", titlebar));
        title_layout->addStretch(1);

        auto* min_btn = new QPushButton("_", titlebar);
        auto* max_btn = new QPushButton("[]", titlebar);
        auto* close_btn = new QPushButton("X", titlebar);
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
        root_layout->addWidget(titlebar);

        auto* body = new QWidget(root);
        auto* body_layout = new QHBoxLayout(body);
        body_layout->setContentsMargins(12, 12, 12, 12);
        body_layout->setSpacing(10);

        auto* left_slot = build_slot("LEFT", "Telemetry + Process summaries");
        auto* center_slot = build_slot("CENTER", "QML Cockpit scene");
        auto* right_slot = build_slot("RIGHT", "Render bridge + DVR status");

        auto* quick = new QQuickWidget(center_slot);
        quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
        quick->setSource(QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_QML_PATH)));
        quick->setMinimumHeight(340);
        center_slot->layout()->addWidget(quick);

        body_layout->addWidget(left_slot, 1);
        body_layout->addWidget(center_slot, 2);
        body_layout->addWidget(right_slot, 1);
        root_layout->addWidget(body, 1);

        auto* status = new QLabel(
            QString("interval=%1  persist=%2  db=%3")
                .arg(config.interval_seconds)
                .arg(config.persistence_enabled ? "on" : "off")
                .arg(config.db_path.value_or("<none>")),
            root
        );
        status->setObjectName("status");
        status->setContentsMargins(12, 0, 12, 12);
        root_layout->addWidget(status);

        const auto dock = aura::shell::build_default_dock_state();
        const auto left_active = aura::shell::active_panel(dock, aura::shell::DockSlot::Left);
        if (!left_active.has_value()) {
            status->setText(status->text() + "  dock=invalid");
        }

        setCentralWidget(root);
    }

private:
    static QFrame* build_slot(const QString& title, const QString& content) {
        auto* frame = new QFrame();
        frame->setObjectName("slot");
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto* title_label = new QLabel(title, frame);
        auto* content_label = new QLabel(content, frame);
        content_label->setWordWrap(true);

        layout->addWidget(title_label);
        layout->addWidget(content_label);
        layout->addStretch(1);
        return frame;
    }
};

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const LaunchConfig config = parse_args(app);
    AuraShellWindow window(config);
    window.show();
    return app.exec();
}

