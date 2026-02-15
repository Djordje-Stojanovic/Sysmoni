#include "aura_shell/ui_theme.hpp"

#include <iostream>
#include <string>

namespace {

bool expect_true(const bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}

bool test_mode_parse_defaults_to_dark() {
    bool ok = true;
    ok &= expect_true(
        aura::shell::ui_theme_mode_from_key("dark_blue") == aura::shell::UiThemeMode::DarkBlue,
        "parse dark_blue"
    );
    ok &= expect_true(
        aura::shell::ui_theme_mode_from_key("pink_cute") == aura::shell::UiThemeMode::PinkCute,
        "parse pink_cute"
    );
    ok &= expect_true(
        aura::shell::ui_theme_mode_from_key("unknown_value") == aura::shell::UiThemeMode::DarkBlue,
        "parse unknown defaults to dark"
    );
    return ok;
}

bool test_mode_key_roundtrip() {
    bool ok = true;
    ok &= expect_true(
        aura::shell::ui_theme_mode_key(aura::shell::UiThemeMode::DarkBlue) == "dark_blue",
        "key dark_blue"
    );
    ok &= expect_true(
        aura::shell::ui_theme_mode_key(aura::shell::UiThemeMode::PinkCute) == "pink_cute",
        "key pink_cute"
    );
    return ok;
}

bool test_mode_toggle() {
    bool ok = true;
    ok &= expect_true(
        aura::shell::toggle_ui_theme_mode(aura::shell::UiThemeMode::DarkBlue) ==
            aura::shell::UiThemeMode::PinkCute,
        "toggle dark to pink"
    );
    ok &= expect_true(
        aura::shell::toggle_ui_theme_mode(aura::shell::UiThemeMode::PinkCute) ==
            aura::shell::UiThemeMode::DarkBlue,
        "toggle pink to dark"
    );
    return ok;
}

bool test_mode_label() {
    bool ok = true;
    ok &= expect_true(
        aura::shell::ui_theme_mode_label(aura::shell::UiThemeMode::DarkBlue) == "Theme: Dark Blue",
        "label dark"
    );
    ok &= expect_true(
        aura::shell::ui_theme_mode_label(aura::shell::UiThemeMode::PinkCute) == "Theme: Pink Cute",
        "label pink"
    );
    return ok;
}

}  // namespace

int main() {
    int failures = 0;
    if (!test_mode_parse_defaults_to_dark()) {
        ++failures;
    }
    if (!test_mode_key_roundtrip()) {
        ++failures;
    }
    if (!test_mode_toggle()) {
        ++failures;
    }
    if (!test_mode_label()) {
        ++failures;
    }

    if (failures == 0) {
        std::cout << "All UI theme tests passed." << '\n';
        return 0;
    }
    std::cerr << failures << " UI theme tests failed." << '\n';
    return 1;
}
