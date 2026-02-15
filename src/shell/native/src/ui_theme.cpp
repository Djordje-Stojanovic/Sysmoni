#include "aura_shell/ui_theme.hpp"

namespace aura::shell {

UiThemeMode ui_theme_mode_from_key(const std::string_view key) {
    if (key == "pink_cute") {
        return UiThemeMode::PinkCute;
    }
    return UiThemeMode::DarkBlue;
}

std::string ui_theme_mode_key(const UiThemeMode mode) {
    switch (mode) {
        case UiThemeMode::DarkBlue:
            return "dark_blue";
        case UiThemeMode::PinkCute:
            return "pink_cute";
    }
    return "dark_blue";
}

UiThemeMode toggle_ui_theme_mode(const UiThemeMode mode) {
    return mode == UiThemeMode::PinkCute ? UiThemeMode::DarkBlue : UiThemeMode::PinkCute;
}

std::string ui_theme_mode_label(const UiThemeMode mode) {
    switch (mode) {
        case UiThemeMode::DarkBlue:
            return "Theme: Dark Blue";
        case UiThemeMode::PinkCute:
            return "Theme: Pink Cute";
    }
    return "Theme: Dark Blue";
}

}  // namespace aura::shell
