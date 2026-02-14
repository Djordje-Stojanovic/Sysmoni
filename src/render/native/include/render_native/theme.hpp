#pragma once

#include <string>

namespace aura::render_native {

struct RgbColor {
    int red{0};
    int green{0};
    int blue{0};
};

// Named color palette for the Aura visual identity.
struct AuraPalette {
    // Background depths (darker = further back)
    static constexpr const char* kWindowBg      = "#060b14";
    static constexpr const char* kPanelBg       = "#0a1221";
    static constexpr const char* kSurfaceBg     = "#0f1a2e";
    static constexpr const char* kElevatedBg    = "#162238";

    // Borders
    static constexpr const char* kBorderSubtle  = "#1e3350";
    static constexpr const char* kBorderActive  = "#2a4a6e";
    static constexpr const char* kBorderAccent  = "#3b82f6";

    // Text hierarchy
    static constexpr const char* kTextPrimary   = "#e0ecf7";
    static constexpr const char* kTextSecondary = "#8badc4";
    static constexpr const char* kTextMuted     = "#4d6d87";
    static constexpr const char* kTextDisabled  = "#2e4a63";

    // Accent colors
    static constexpr const char* kAccentBlue    = "#3b82f6";
    static constexpr const char* kAccentCyan    = "#06b6d4";
    static constexpr const char* kAccentAmber   = "#f59e0b";
    static constexpr const char* kAccentRed     = "#ef4444";
    static constexpr const char* kAccentGreen   = "#22c55e";

    // Gauge colors
    static constexpr const char* kGaugeTrack    = "#1a2940";
    static constexpr const char* kGaugeLow      = "#3b82f6";
    static constexpr const char* kGaugeMid      = "#06b6d4";
    static constexpr const char* kGaugeHigh     = "#f59e0b";
    static constexpr const char* kGaugeCritical = "#ef4444";
};

// Existing API — unchanged.
RgbColor parse_hex_color(const std::string& value);
std::string blend_hex_color(const std::string& start, const std::string& end, double ratio);
int quantize_accent_intensity(double accent_intensity);

// Returns an interpolated color based on a 0-100 percent value.
// Segments:
//   0-40  : blue (#3b82f6)
//   40-70 : blue  -> cyan  (#3b82f6 -> #06b6d4)
//   70-85 : cyan  -> amber (#06b6d4 -> #f59e0b)
//   85-100: amber -> red   (#f59e0b -> #ef4444)
RgbColor interpolate_gauge_color(double percent);
std::string interpolate_gauge_color_hex(double percent);

// Accessibility helpers.
// relative_luminance returns the WCAG 2.x relative luminance in [0, 1].
// contrast_ratio returns the WCAG 2.x contrast ratio in [1, 21].
double relative_luminance(const RgbColor& color);
double contrast_ratio(const RgbColor& foreground, const RgbColor& background);

}  // namespace aura::render_native
