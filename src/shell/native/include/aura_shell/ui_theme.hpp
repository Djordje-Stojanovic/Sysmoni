#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aura::shell {

enum class UiThemeMode : std::uint8_t {
    DarkBlue = 0,
    PinkCute = 1,
};

struct ThemePalette {
    const char* bg_window;
    const char* bg_panel;
    const char* bg_surface;
    const char* bg_elevated;
    const char* border_subtle;
    const char* border_active;
    const char* border_accent;
    const char* text_primary;
    const char* text_secondary;
    const char* text_muted;
    const char* accent;
    const char* accent_hover;
    const char* danger;
    const char* danger_hover;
};

const ThemePalette& get_theme_palette(UiThemeMode mode);

UiThemeMode ui_theme_mode_from_key(std::string_view key);
std::string ui_theme_mode_key(UiThemeMode mode);
UiThemeMode toggle_ui_theme_mode(UiThemeMode mode);
std::string ui_theme_mode_label(UiThemeMode mode);

}  // namespace aura::shell
