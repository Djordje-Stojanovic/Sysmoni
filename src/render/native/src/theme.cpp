#include "render_native/theme.hpp"

#include "render_native/math.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace aura::render_native {

namespace {

int parse_hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    throw std::invalid_argument("Expected #RRGGBB color.");
}

int parse_hex_byte(char hi, char lo) {
    return (parse_hex_nibble(hi) * 16) + parse_hex_nibble(lo);
}

char to_hex_char(int value) {
    static constexpr std::array<char, 16> kHex = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    if (value < 0 || value > 15) {
        throw std::invalid_argument("hex nibble out of range");
    }
    return kHex[static_cast<std::size_t>(value)];
}

// Build a "#rrggbb" string from clamped 0-255 channel values.
std::string rgb_to_hex(int red, int green, int blue) {
    std::string out;
    out.reserve(7);
    out.push_back('#');
    out.push_back(to_hex_char((red >> 4) & 0x0f));
    out.push_back(to_hex_char(red & 0x0f));
    out.push_back(to_hex_char((green >> 4) & 0x0f));
    out.push_back(to_hex_char(green & 0x0f));
    out.push_back(to_hex_char((blue >> 4) & 0x0f));
    out.push_back(to_hex_char(blue & 0x0f));
    return out;
}

// Linearise a single 8-bit sRGB channel for WCAG luminance computation.
// Returns a value in [0, 1].
double linearise_channel(int channel_8bit) {
    const double c = static_cast<double>(channel_8bit) / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

}  // namespace

// ---------------------------------------------------------------------------
// Existing API — unchanged
// ---------------------------------------------------------------------------

RgbColor parse_hex_color(const std::string& value) {
    if (value.size() != 7 || value.front() != '#') {
        throw std::invalid_argument("Expected #RRGGBB color.");
    }

    return RgbColor{
        parse_hex_byte(value[1], value[2]),
        parse_hex_byte(value[3], value[4]),
        parse_hex_byte(value[5], value[6]),
    };
}

std::string blend_hex_color(const std::string& start, const std::string& end, double ratio) {
    const RgbColor start_rgb = parse_hex_color(start);
    const RgbColor end_rgb = parse_hex_color(end);
    const double t = clamp_unit(ratio);

    const int red = static_cast<int>(std::lround(
        static_cast<double>(start_rgb.red) + ((end_rgb.red - start_rgb.red) * t)
    ));
    const int green = static_cast<int>(std::lround(
        static_cast<double>(start_rgb.green) + ((end_rgb.green - start_rgb.green) * t)
    ));
    const int blue = static_cast<int>(std::lround(
        static_cast<double>(start_rgb.blue) + ((end_rgb.blue - start_rgb.blue) * t)
    ));

    return rgb_to_hex(red, green, blue);
}

int quantize_accent_intensity(double accent_intensity) {
    return static_cast<int>(std::lround(clamp_unit(accent_intensity) * 100.0));
}

// ---------------------------------------------------------------------------
// Gauge color interpolation
// ---------------------------------------------------------------------------

// Blend two RgbColor values by ratio t in [0, 1].
// Returns clamped integer channels.
namespace {

RgbColor blend_rgb(const RgbColor& a, const RgbColor& b, double t) {
    const double tc = clamp_unit(t);
    return RgbColor{
        static_cast<int>(std::lround(
            static_cast<double>(a.red)   + (static_cast<double>(b.red   - a.red)   * tc))),
        static_cast<int>(std::lround(
            static_cast<double>(a.green) + (static_cast<double>(b.green - a.green) * tc))),
        static_cast<int>(std::lround(
            static_cast<double>(a.blue)  + (static_cast<double>(b.blue  - a.blue)  * tc))),
    };
}

}  // namespace

RgbColor interpolate_gauge_color(double percent) {
    // Guard against NaN / Inf — treat as 0.
    if (!std::isfinite(percent)) {
        percent = 0.0;
    }
    // Clamp to [0, 100].
    if (percent < 0.0) {
        percent = 0.0;
    }
    if (percent > 100.0) {
        percent = 100.0;
    }

    // Pre-parsed palette stops.
    static const RgbColor kBlue  = {0x3b, 0x82, 0xf6};  // #3b82f6
    static const RgbColor kCyan  = {0x06, 0xb6, 0xd4};  // #06b6d4
    static const RgbColor kAmber = {0xf5, 0x9e, 0x0b};  // #f59e0b
    static const RgbColor kRed   = {0xef, 0x44, 0x44};  // #ef4444

    // Segment boundaries
    // [0, 40)   : flat blue
    // [40, 70)  : blue -> cyan
    // [70, 85)  : cyan -> amber
    // [85, 100] : amber -> red

    if (percent <= 40.0) {
        return kBlue;
    }
    if (percent <= 70.0) {
        const double t = (percent - 40.0) / 30.0;  // [0,1] over [40,70]
        return blend_rgb(kBlue, kCyan, t);
    }
    if (percent <= 85.0) {
        const double t = (percent - 70.0) / 15.0;  // [0,1] over [70,85]
        return blend_rgb(kCyan, kAmber, t);
    }
    // (85, 100]
    const double t = (percent - 85.0) / 15.0;  // [0,1] over [85,100]
    return blend_rgb(kAmber, kRed, t);
}

std::string interpolate_gauge_color_hex(double percent) {
    const RgbColor c = interpolate_gauge_color(percent);
    return rgb_to_hex(c.red, c.green, c.blue);
}

// ---------------------------------------------------------------------------
// Accessibility helpers (WCAG 2.x)
// ---------------------------------------------------------------------------

double relative_luminance(const RgbColor& color) {
    const double r = linearise_channel(color.red);
    const double g = linearise_channel(color.green);
    const double b = linearise_channel(color.blue);
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double contrast_ratio(const RgbColor& foreground, const RgbColor& background) {
    const double lf = relative_luminance(foreground);
    const double lb = relative_luminance(background);
    const double lighter = (lf > lb) ? lf : lb;
    const double darker  = (lf > lb) ? lb : lf;
    // Denominator is always >= 0.05, so no division-by-zero risk.
    return (lighter + 0.05) / (darker + 0.05);
}

}  // namespace aura::render_native
