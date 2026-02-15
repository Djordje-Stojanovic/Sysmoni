#include <QApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include "aura_shell/aura_shell_window.hpp"
#include "aura_shell/shell_utils.hpp"

int main(int argc, char* argv[]) {
    // Force hardware-accelerated rendering (D3D11 on Windows, OpenGL elsewhere)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("Aura");
    app.setApplicationVersion("1.0.0");
    const aura::shell::LaunchConfig config = aura::shell::parse_args(app);
    aura::shell::AuraShellWindow window(config);
    window.show();
    return app.exec();
}
