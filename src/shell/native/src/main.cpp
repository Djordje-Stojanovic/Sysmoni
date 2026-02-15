#include <QApplication>
#include "aura_shell/aura_shell_window.hpp"
#include "aura_shell/shell_utils.hpp"

int main(int argc, char* argv[]) {
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
