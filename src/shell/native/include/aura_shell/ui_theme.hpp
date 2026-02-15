#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aura::shell {

enum class UiThemeMode : std::uint8_t {
    DarkBlue = 0,
    PinkCute = 1,
};

UiThemeMode ui_theme_mode_from_key(std::string_view key);
std::string ui_theme_mode_key(UiThemeMode mode);
UiThemeMode toggle_ui_theme_mode(UiThemeMode mode);
std::string ui_theme_mode_label(UiThemeMode mode);

}  // namespace aura::shell
