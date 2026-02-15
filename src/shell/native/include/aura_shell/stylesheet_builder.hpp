#pragma once

#include <QString>
#include "aura_shell/ui_theme.hpp"
#include "aura_shell/size_metrics.hpp"

namespace aura::shell {
QString build_app_stylesheet(UiThemeMode mode, const SizeMetrics& m);
}
