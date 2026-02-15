#include "aura_shell/ui_theme.hpp"

namespace aura::shell {

static constexpr ThemePalette kDarkBluePalette{
    "#060b14",  // bg_window
    "#0a1221",  // bg_panel
    "#0f1a2e",  // bg_surface
    "#162238",  // bg_elevated
    "#1e3350",  // border_subtle
    "#2a4a6e",  // border_active
    "#3b82f6",  // border_accent
    "#e0ecf7",  // text_primary
    "#8badc4",  // text_secondary
    "#4d6d87",  // text_muted
    "#3b82f6",  // accent
    "#60a5fa",  // accent_hover
    "#ef4444",  // danger
    "#f87171",  // danger_hover
};

static constexpr ThemePalette kPinkCutePalette{
    "#150a12",  // bg_window
    "#1f0e1a",  // bg_panel
    "#2d1525",  // bg_surface
    "#3d1d33",  // bg_elevated
    "#6c2d55",  // border_subtle
    "#91406f",  // border_active
    "#ff4da6",  // border_accent
    "#ffe8f5",  // text_primary
    "#ffb3d9",  // text_secondary
    "#d887b1",  // text_muted
    "#ff4da6",  // accent
    "#ff92c5",  // accent_hover
    "#ef4444",  // danger
    "#f87171",  // danger_hover
};

const ThemePalette& get_theme_palette(const UiThemeMode mode) {
    if (mode == UiThemeMode::PinkCute) {
        return kPinkCutePalette;
    }
    return kDarkBluePalette;
}

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
